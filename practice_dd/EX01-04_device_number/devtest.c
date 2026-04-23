#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>

unsigned int device_major = 120;
unsigned int device_minor_start = 0;
unsigned int device_minor_count = 4;
dev_t devt;

static int __init device_init(void)
{
	int ret;

	printk("devtest: device_init\n");

	devt = MKDEV(device_major, device_minor_start);
	ret = register_chrdev_region(devt, device_minor_count, "my_device");
	if(ret < 0) {
		printk("devtest: can't get major %d\n", device_major);
		return ret;
	}

	return 0;
}

static void __exit device_exit(void)
{
	printk("devtest: device_exit\n");

	unregister_chrdev_region(devt, device_minor_count);
}

module_init(device_init);
module_exit(device_exit);

MODULE_LICENSE("GPL");
