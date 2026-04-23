#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>

unsigned int device_major = 120;

static const struct file_operations my_fops = {
	.owner = THIS_MODULE,
};

static int __init device_init(void)
{
	int ret;

	printk("devtest: device_init\n");

	ret = register_chrdev(device_major, "my_device", &my_fops);
	if(ret < 0) {
		printk ("devtest: unable to get major %d\n", device_major);
		return ret;
	}

	return 0;
}

static void __exit device_exit(void)
{
	printk("devtest: device_exit\n");

	unregister_chrdev(device_major, "my_device");
}

module_init(device_init);
module_exit(device_exit);

MODULE_LICENSE("GPL");

