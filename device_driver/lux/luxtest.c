#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>

#define BH1750_POWER_DOWN       0x00
#define BH1750_POWER_ON         0x01
#define BH1750_CONTINUOUS_HIGH  0x10

struct bh1750_data {
    struct i2c_client *client;
    struct mutex lock;
};

static int bh1750_open(struct inode *inode, struct file *file)
{
    struct miscdevice *mdev = file->private_data;
    struct bh1750_data *data = dev_get_drvdata(mdev->parent);
    int ret;

    mutex_lock(&data->lock);
    
    /* 1. 센서 깨우기 */
    ret = i2c_smbus_write_byte(data->client, BH1750_POWER_ON);
    if (ret < 0) goto out;

    /* 2. 고해상도 연속 측정 모드 설정 */
    ret = i2c_smbus_write_byte(data->client, BH1750_CONTINUOUS_HIGH);
    
    /* 센서가 첫 번째 데이터를 준비할 때까지 대기 (최대 180ms) */
    msleep(180);

    pr_info("BH1750: Device opened and initialized\n");

out:
    mutex_unlock(&data->lock);
    return ret;
}

static int bh1750_release(struct inode *inode, struct file *file)
{
    struct miscdevice *mdev = file->private_data;
    struct bh1750_data *data = dev_get_drvdata(mdev->parent);

    /* 사용이 끝나면 전력을 아끼기 위해 Power Down */
    i2c_smbus_write_byte(data->client, BH1750_POWER_DOWN);
    
    pr_info("BH1750: Device closed and powered down\n");
    return 0;
}

static ssize_t bh1750_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
    struct miscdevice *mdev = file->private_data;
    struct bh1750_data *data = dev_get_drvdata(mdev->parent);
    unsigned char raw[2];
    unsigned short lux;
    int ret;

    /* I2C로부터 2바이트 읽기 */
    ret = i2c_master_recv(data->client, raw, 2);
    if (ret != 2) return -EIO;

    /* 데이터 조합 및 계산 (Raw / 1.2) */
    lux = (raw[0] << 8) | raw[1];
    lux = (lux * 10) / 12;

    if (copy_to_user(buf, &lux, sizeof(lux)))
        return -EFAULT;

    return sizeof(lux);
}

static const struct file_operations bh1750_fops = {
    .owner   = THIS_MODULE,
    .open    = bh1750_open,
    .release = bh1750_release,
    .read    = bh1750_read,
};

static struct miscdevice bh1750_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "bh1750",
    .fops  = &bh1750_fops,
};

static int bh1750_probe(struct i2c_client *client)
{
    struct bh1750_data *data;

    data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
    if (!data) return -ENOMEM;

    data->client = client;
    mutex_init(&data->lock);
    i2c_set_clientdata(client, data);

    bh1750_misc.parent = &client->dev;
    return misc_register(&bh1750_misc);
}

static void bh1750_remove(struct i2c_client *client)
{
    misc_deregister(&bh1750_misc);
}

/* DT 매칭 정보를 모듈에 등록 */
static const struct of_device_id bh1750_of_match[] = {
    { .compatible = "willtek,bh1750" },
    { }
};
MODULE_DEVICE_TABLE(of, bh1750_of_match);

static struct i2c_driver bh1750_driver = {
    .driver = {
        .name = "bh1750_mod",
        .of_match_table = bh1750_of_match,
    },
    .probe = bh1750_probe,
    .remove = bh1750_remove,
};

module_i2c_driver(bh1750_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gemini & User");
MODULE_DESCRIPTION("BH1750 Light Sensor LKM with auto-init on open");
