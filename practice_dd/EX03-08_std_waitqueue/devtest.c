#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>

#include "my_ioctl.h"

#define MAX_BUF 26

static unsigned int device_major = 120;
static unsigned int device_minor_start = 0;
static unsigned int device_minor_count = 4;
static dev_t devt;
static struct cdev *my_cdev;

static char rbuf[MAX_BUF];
static char wbuf[MAX_BUF];

#define GPIO_PHY_BASE		0x3f200000
#define GPIO_PHY_SIZE		0x100
#define GPFSEL0			0x000
#define GPSET0			0x01c
#define GPCLR0			0x028

#define CONF_REQUEST_MEM_REGION_EN 0

static volatile unsigned long gpio_base;
#if CONF_REQUEST_MEM_REGION_EN
static struct resource *gpio_mem;
#endif

#define GPIO_KEY 17
int irq_key;
static int irq_enabled = 0;

#define TEST_WAITQUEUE 1
#if TEST_WAITQUEUE
#include <linux/sched.h>
#include <linux/wait.h>
static DECLARE_WAIT_QUEUE_HEAD(my_waitqueue);
static int my_flag;
#endif

static void led_init(void)
{
	iowrite32((ioread32((void *)(gpio_base+GPFSEL0)) & ~(0x3f<<15)) | (0x9<<15), (void *)(gpio_base+GPFSEL0));
	iowrite32(0x3<<5, (void *)(gpio_base+GPSET0));
}

static void led_on(void)
{
	iowrite32(0x3<<5, (void *)(gpio_base+GPCLR0));
}

static void led_off(void)
{
	iowrite32(0x3<<5, (void *)(gpio_base+GPSET0));
}

static long device_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0, data;

	printk("devtest: device_ioctl (minor = %d)\n", iminor(filp->f_path.dentry->d_inode));
	switch(cmd) {
		case MY_IOCTL_CMD_ONE:
			printk("devtest: MY_IOCTL_CMD_ONE\n");
			break;
		case MY_IOCTL_CMD_TWO:
			if(copy_from_user(&data, (int *)arg, sizeof(int))) {
				return -EFAULT;
			}
			printk("devtest: MY_IOCTL_CMD_TWO(%d)\n", data);
			ret = 2;
			break;
		case MY_IOCTL_CMD_LED_ON:
			printk("devtest: MY_IOCTL_CMD_LED_ON\n");
			led_on();
			break;
		case MY_IOCTL_CMD_LED_OFF:
			printk("devtest: MY_IOCTL_CMD_LED_OFF\n");
			led_off();
			break;
		case MY_IOCTL_CMD_IRQ_EN:
			printk("devtest: MY_IOCTL_CMD_IRQ_EN\n");
			if(!irq_enabled) {
				enable_irq(irq_key);
				irq_enabled = 1;
			}
			break;
		case MY_IOCTL_CMD_IRQ_DIS:
			printk("devtest: MY_IOCTL_CMD_IRQ_DIS\n");
			if(irq_enabled) {
				disable_irq(irq_key);
				irq_enabled = 0;
			}
			break;
		default:
			printk("devtest: unknown command\n");
			ret = -EINVAL;
			break;
	}

	return ret;
}

static ssize_t device_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	ssize_t rlen; 

	printk("devtest: device_read (minor = %d)\n", iminor(filp->f_path.dentry->d_inode));

#if TEST_WAITQUEUE
	printk("devtest: waiting key event\n");
	if(wait_event_interruptible(my_waitqueue, my_flag != 0)) {
		printk("devtest: interrupted\n");
		return -ERESTARTSYS;
	}

	my_flag = 0;
	printk("devtest: awoken!\n");
#endif

	rlen = MAX_BUF;
	if(rlen > count) {
		rlen = count;
	}
	if(copy_to_user(buf, rbuf, rlen)) {
		return -EFAULT;
	}
	printk("devtest: read %ld bytes\n", rlen);

	return rlen;
}

static ssize_t device_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	ssize_t wlen;

	printk("devtest: device_write (minor = %d)\n", iminor(filp->f_path.dentry->d_inode));
	wlen = MAX_BUF;
	if(wlen > count) {
		wlen = count;
	}
	if(copy_from_user(wbuf, buf, wlen)) {
		return -EFAULT;
	}
	printk("devtest: wrote %ld bytes\n", wlen);

	return wlen;
}

static int device_open(struct inode *inode, struct file *filp)
{
	printk("devtest: device_open (minor = %d)\n", iminor(inode));
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

irqreturn_t key_isr(int irq, void *dev_id)
{
	static int count;

	printk("devtest: %s(): count = %d\n", __FUNCTION__, ++count);

#if TEST_WAITQUEUE
	my_flag = 1;
	wake_up_interruptible(&my_waitqueue);
#endif

	return IRQ_HANDLED;
}

static int __init device_init(void)
{
	int ret, i;

	printk("devtest: device_init\n");

	devt = MKDEV(device_major, device_minor_start);
	ret = register_chrdev_region(devt, device_minor_count, "my_device");
	if(ret < 0) {
		printk("devtest: can't get major %d\n", device_major);
		goto err0;
	}

	my_cdev = cdev_alloc();
	my_cdev->ops = &my_fops;
	my_cdev->owner = THIS_MODULE;
	ret = cdev_add(my_cdev, devt, device_minor_count);
	if(ret) {
		printk("devtest: can't add device %d\n", devt);
		goto err1;
	}

	/* init buffers */
	for(i=0; i<MAX_BUF; i++) rbuf[i] = 'A' + i;
	for(i=0; i<MAX_BUF; i++) wbuf[i] = 0;

#if CONF_REQUEST_MEM_REGION_EN
	gpio_mem = request_mem_region(GPIO_PHY_BASE, GPIO_PHY_SIZE, "gpio");
	if (gpio_mem == NULL) {
		printk("devtest: failed to get memory region\n");
		ret = -EIO;
		goto err2;
	}
#endif

	gpio_base = (unsigned long)ioremap(GPIO_PHY_BASE, GPIO_PHY_SIZE);
	if (gpio_base == 0) {
		printk("devtest: ioremap error\n");
		ret = -EIO;
		goto err3;
	}

	led_init();

	irq_key = gpio_to_irq(GPIO_KEY);
	if(request_irq(irq_key, key_isr, IRQF_TRIGGER_FALLING, "key_int", NULL)) {
		printk("devtest: IRQ %d is not free\n", irq_key);
		ret = -EIO;
		goto err4;
	}
	printk("devtest: IRQ %d is enabled\n", irq_key);
	irq_enabled = 1;
	return 0;

err4:
	iounmap((void *)gpio_base);
err3:
#if CONF_REQUEST_MEM_REGION_EN
	release_mem_region(GPIO_PHY_BASE, GPIO_PHY_SIZE);
err2:
#endif
	cdev_del(my_cdev);
err1:
	unregister_chrdev_region(devt, device_minor_count);
err0:
	return ret;
}

static void __exit device_exit(void)
{
	printk("devtest: device_exit\n");

	free_irq(irq_key, NULL);
	irq_enabled = 0;
	iounmap((void *)gpio_base);
#if CONF_REQUEST_MEM_REGION_EN
	release_mem_region(GPIO_PHY_BASE, GPIO_PHY_SIZE);
#endif
	cdev_del(my_cdev);
	unregister_chrdev_region(devt, device_minor_count);
}

module_init(device_init);
module_exit(device_exit);

MODULE_LICENSE("GPL");
