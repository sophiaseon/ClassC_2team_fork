#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/gpio/consumer.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/limits.h>

#define DRV_NAME                "hcsr04-array"
#define HCSR04_MAX_SENSORS      4
#define READ_BUF_SIZE           256

struct hcsr04_sensor {
	struct gpio_desc *trig_gpiod;
	struct gpio_desc *echo_gpiod;
	unsigned int last_distance_cm;
	int last_error;
};

struct hcsr04_array_data {
	struct device *dev;
	struct hcsr04_sensor sensors[HCSR04_MAX_SENSORS];
	unsigned int num_sensors;
	unsigned int inter_sensor_delay_us;
	struct miscdevice miscdev;
	struct mutex lock;
};

static int hcsr04_wait_for_level(struct gpio_desc *gpiod, int target_level,
				 unsigned int timeout_us, ktime_t *ts)
{
	ktime_t start;
	ktime_t now;
	s64 elapsed_us;

	start = ktime_get();

	for (;;) {
		if (gpiod_get_value_cansleep(gpiod) == target_level) {
			if (ts)
				*ts = ktime_get();
			return 0;
		}

		now = ktime_get();
		elapsed_us = ktime_us_delta(now, start);
		if (elapsed_us >= timeout_us)
			return -ETIMEDOUT;

		cpu_relax();
	}
}

static int hcsr04_measure_one(struct hcsr04_sensor *sensor,
			      unsigned int *distance_cm)
{
	int ret;
	ktime_t rise_ts;
	ktime_t fall_ts;
	s64 pulse_us;
	u64 dist;

	gpiod_set_value_cansleep(sensor->trig_gpiod, 0);
	usleep_range(2, 10);

	gpiod_set_value_cansleep(sensor->trig_gpiod, 1);
	udelay(10);
	gpiod_set_value_cansleep(sensor->trig_gpiod, 0);

	ret = hcsr04_wait_for_level(sensor->echo_gpiod, 1, 30000, &rise_ts);
	if (ret)
		return ret;

	ret = hcsr04_wait_for_level(sensor->echo_gpiod, 0, 30000, &fall_ts);
	if (ret)
		return ret;

	pulse_us = ktime_us_delta(fall_ts, rise_ts);
	if (pulse_us <= 0)
		return -EIO;

	dist = div_u64((u64)pulse_us, 58ULL);
	if (dist > UINT_MAX)
		return -ERANGE;

	*distance_cm = (unsigned int)dist;
	return 0;
}

static int hcsr04_measure_all(struct hcsr04_array_data *data)
{
	unsigned int i;

	for (i = 0; i < data->num_sensors; i++) {
		unsigned int distance_cm = 0;
		int ret;

		ret = hcsr04_measure_one(&data->sensors[i], &distance_cm);
		data->sensors[i].last_error = ret;

		if (!ret)
			data->sensors[i].last_distance_cm = distance_cm;

		if (i + 1 < data->num_sensors) {
			usleep_range(data->inter_sensor_delay_us,
				     data->inter_sensor_delay_us + 1000);
		}
	}

	return 0;
}

static ssize_t hcsr04_array_read(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	struct miscdevice *mdev = file->private_data;
	struct hcsr04_array_data *data =
		container_of(mdev, struct hcsr04_array_data, miscdev);
	char kbuf[READ_BUF_SIZE];
	int len = 0;
	unsigned int i;
	int ret;

	if (*ppos != 0)
		return 0;

	mutex_lock(&data->lock);
	ret = hcsr04_measure_all(data);
	if (ret) {
		mutex_unlock(&data->lock);
		return ret;
	}

	for (i = 0; i < data->num_sensors; i++) {
		if (data->sensors[i].last_error) {
			len += scnprintf(kbuf + len, sizeof(kbuf) - len,
					 "sensor%u:error=%d\n",
					 i, data->sensors[i].last_error);
		} else {
			len += scnprintf(kbuf + len, sizeof(kbuf) - len,
					 "sensor%u:%u\n",
					 i, data->sensors[i].last_distance_cm);
		}

		if (len >= sizeof(kbuf)) {
			mutex_unlock(&data->lock);
			return -ENOSPC;
		}
	}
	mutex_unlock(&data->lock);

	if (count < len)
		return -EINVAL;

	if (copy_to_user(buf, kbuf, len))
		return -EFAULT;

	*ppos += len;
	return len;
}

static const struct file_operations hcsr04_array_fops = {
	.owner = THIS_MODULE,
	.read = hcsr04_array_read,
	.llseek = no_llseek,
};

static int hcsr04_array_probe(struct platform_device *pdev)
{
	struct hcsr04_array_data *data;
	u32 delay_us;
	unsigned int i;
	int ret;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->dev = &pdev->dev;
	data->num_sensors = HCSR04_MAX_SENSORS;
	mutex_init(&data->lock);

	for (i = 0; i < data->num_sensors; i++) {
		data->sensors[i].trig_gpiod =
			devm_gpiod_get_index(&pdev->dev, "trig", i, GPIOD_OUT_LOW);
		if (IS_ERR(data->sensors[i].trig_gpiod)) {
			dev_err(&pdev->dev, "failed to get trig gpio index %u\n", i);
			return PTR_ERR(data->sensors[i].trig_gpiod);
		}

		data->sensors[i].echo_gpiod =
			devm_gpiod_get_index(&pdev->dev, "echo", i, GPIOD_IN);
		if (IS_ERR(data->sensors[i].echo_gpiod)) {
			dev_err(&pdev->dev, "failed to get echo gpio index %u\n", i);
			return PTR_ERR(data->sensors[i].echo_gpiod);
		}

		data->sensors[i].last_distance_cm = 0;
		data->sensors[i].last_error = 0;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
				   "inter-sensor-delay-us",
				   &delay_us);
	if (ret)
		delay_us = 60000;

	data->inter_sensor_delay_us = delay_us;

	data->miscdev.minor = MISC_DYNAMIC_MINOR;
	data->miscdev.name = "hcsr04_array";
	data->miscdev.fops = &hcsr04_array_fops;
	data->miscdev.parent = &pdev->dev;

	ret = misc_register(&data->miscdev);
	if (ret) {
		dev_err(&pdev->dev, "failed to register misc device\n");
		return ret;
	}

	platform_set_drvdata(pdev, data);

	dev_info(&pdev->dev,
		 "HC-SR04 array driver probed: %u sensors, inter-delay=%uus\n",
		 data->num_sensors, data->inter_sensor_delay_us);
	dev_info(&pdev->dev, "/dev/%s created\n", data->miscdev.name);

	return 0;
}

static void hcsr04_array_remove(struct platform_device *pdev)
{
	struct hcsr04_array_data *data = platform_get_drvdata(pdev);

	misc_deregister(&data->miscdev);
	dev_info(&pdev->dev, "HC-SR04 array driver removed\n");
}

static const struct of_device_id hcsr04_array_of_match[] = {
	{ .compatible = "custom,hcsr04-array" },
	{ }
};
MODULE_DEVICE_TABLE(of, hcsr04_array_of_match);

static struct platform_driver hcsr04_array_driver = {
	.probe = hcsr04_array_probe,
	.remove_new = hcsr04_array_remove,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = hcsr04_array_of_match,
	},
};

module_platform_driver(hcsr04_array_driver);

MODULE_AUTHOR("OpenAI");
MODULE_DESCRIPTION("4-channel HC-SR04 array driver with independent trigger/echo pairs");
MODULE_LICENSE("GPL");
