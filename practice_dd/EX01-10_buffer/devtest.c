#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include "my_ioctl.h"

#define MAX_NUM_OF_MINOR 4
#define DEFAULT_BUF_SIZE 4096

static unsigned int device_major = 120;
static unsigned int device_minor_start = 0;
static unsigned int device_minor_count = MAX_NUM_OF_MINOR;
static dev_t devt;
static struct cdev *my_cdev;
static unsigned int buf_size = DEFAULT_BUF_SIZE;

module_param(device_minor_count, uint, 0);
module_param(buf_size, uint, 0);

static struct _my_buf {
	char *buf;
	int wr;
	int rd;
} my_buf[MAX_NUM_OF_MINOR];

static long device_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0, data;
	int minor = iminor(filp->f_path.dentry->d_inode);

	printk("devtest: device_ioctl (minor = %d)\n", minor);

	/* Implement code */


}

static ssize_t device_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	ssize_t rlen; 
	int minor = iminor(filp->f_path.dentry->d_inode);

	printk("devtest: device_read (minor = %d)\n", minor);

	/* Implement code */


}

static ssize_t device_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	ssize_t wlen;
	int minor = iminor(filp->f_path.dentry->d_inode);

	printk("devtest: device_write (minor = %d)\n", minor);

	/* Implement code */


}

static int device_open(struct inode *inode, struct file *filp)
{
	int minor = iminor(inode);

	printk("devtest: device_open (minor = %d)\n", minor);

	filp->private_data = &my_buf[minor];

	return 0;
}

static int device_release(struct inode *inode, struct file *filp)
{
	printk("devtest: device_release\n");

	return 0;
}


static const struct file_operations my_fops = {
	.owner = THIS_MODULE,
	.open = device_open,
	.release = device_release,
	.read = device_read,
	.write = device_write,
	.unlocked_ioctl = device_ioctl,
};

static int __init device_init(void)
{
	int ret, i;

	printk("devtest: device_init\n");
	printk("devtest: device_minor_count=%d, buf_size=%d\n", device_minor_count, buf_size);

	devt = MKDEV(device_major, device_minor_start);
	ret = register_chrdev_region(devt, device_minor_count, "my_buf");

	if(ret < 0) {
		printk("devtest: can't get major %d\n", device_major);
		return ret;
	}

	my_cdev = cdev_alloc();
	my_cdev->ops = &my_fops;
	my_cdev->owner = THIS_MODULE;
	ret = cdev_add(my_cdev, devt, device_minor_count);
	if(ret) {
		printk("devtest: can't add device %d\n", devt);
		unregister_chrdev_region(devt, device_minor_count);
		return ret;
	}

	for(i=0; i<device_minor_count; i++) {
		my_buf[i].buf = kmalloc(buf_size, GFP_KERNEL);
		if(my_buf[i].buf == NULL) {
			printk("devtest: can't alloc %d bytes\n", buf_size);
		}
	}

	return 0;
}

static void __exit device_exit(void)
{
	int i;

	printk("devtest: device_exit\n");

	for(i=0; i<device_minor_count; i++) {
		if(my_buf[i].buf) kfree(my_buf[i].buf);
	}
	cdev_del(my_cdev);
	unregister_chrdev_region(devt, device_minor_count);
}

module_init(device_init);
module_exit(device_exit);

MODULE_LICENSE("GPL");

