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
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>

#include "my_ioctl.h"

static unsigned int device_major = 121;
static unsigned int device_minor_start = 0;
static unsigned int device_minor_count = 1;
static dev_t devt;
static struct cdev *my_cdev;

#define MAX_BUF (40*2)
static char wbuf[MAX_BUF];
#define SONG_TETRIS 0
#define SONG_CRAZY  1
#define SONG_MARIO  2


#define PWM_CTL 0x00
#define PWM_RNG1 0x10
#define PWM_DAT1 0x14

static volatile unsigned long pwm_base;
static struct resource *pwm_mem;
static unsigned long clk_rate;

#include <linux/platform_device.h>
#include "my_ioctl.h"
#define DEVICE_NAME "my_buzzer"
#define MARIO_BASE 300
static DEFINE_MUTEX(play_lock);

static int phy_base;
static int phy_size;
static int current_song = -1;
static int is_playing = 0;
static volatile int g_stop = 0;  /* set to 1 by IOCTL_STOP */


#define BASE 500
/* crazy arcade music data
const static unsigned int tone2freq[] = {261,277,293,311,329,349,369,391,415,440,466,493,523,554,587,622,659,698,739,783,830,880,932,987};
const static unsigned int note2msec[] = {BASE/3, BASE/2, BASE, BASE*1.5, BASE*3};
*/

/* below is tetris music data
const static unsigned int tone2freq[] = {261,277,293,311,329,349,369,391,415,440,466,493,523,554,587,622,659,698,739,783,830,880,932,987};
const static unsigned int note2msec[] = {BASE / 8, BASE / 4, BASE, BASE*1.5, BASE*3};
*/

// below is super mario music data
const static unsigned int tone2freq[] = {261,277,293,311,329,349,369,391,415,440,466,493,523,554,587,622,659,698,739,783,830,880,932,987};
const static unsigned int note2msec[] = {MARIO_BASE / 3, MARIO_BASE / 2, MARIO_BASE, BASE*1.5, BASE*3};


void buzzer_beep(int freq, int msec)
{
	/* Implement code */
	// iowrite32() ghkfdyd
	// 1. PWM_CTL -> 0x80
	iowrite32(0x80, (void *)(pwm_base +PWM_CTL)); 
	// 2. PWM_RNG1, PWM_DT1 set
	int range = 10000000 / freq;
	iowrite32(range, (void *)(pwm_base + PWM_RNG1));
	iowrite32(range / 2, (void *)(pwm_base + PWM_DAT1));
	// 3. PWM_CTL -> 0x81
	iowrite32(0x81, (void *)(pwm_base + PWM_CTL));
	// 4. msleep(msec);
	msleep_interruptible(msec);
	iowrite32(0x80, (void *)(pwm_base +PWM_CTL)); 
}

static bool playing = true;

void play_super_mario(void) {
	int i = 0;
	ssize_t wlen;
	const static unsigned int note2msecOfMario[] = {MARIO_BASE / 3, MARIO_BASE / 2, MARIO_BASE, BASE*1.5, BASE*3};

	printk("buzzer: playing super mario\n");
	static const char *mario_notes = "QBQBsBQBsBMBQBTBsCHBsCMCsCHBsCJCLBLBJCHBQBTBVBVBsARBTBQBMBOBLB";

	while (mario_notes[i] && mario_notes[i + 1]) {

		char tone = mario_notes[i];
		char note = mario_notes[i + 1];
		int msec = note2msecOfMario[note - 'A'];

		// 쉼표
		if (tone == 's') {
			msleep(msec);
			i += 2;
			continue;
		}

		int freq = tone2freq[tone - 'A'];

		printk("tone=%c(%d Hz), note=%c(%d ms)\n",
		       tone, freq, note, msec);

		buzzer_beep(freq, msec);

		i += 2;
	}
}

void play_crazy_arcade(void) {
	int i = 0;
	ssize_t wlen;
	
	#define CARZY_BASE 250
	const static unsigned int note2msecOfCrazyArcade[] = {CARZY_BASE/3, CARZY_BASE/2, CARZY_BASE, CARZY_BASE*1.5, CARZY_BASE*3};

	printk("buzzertest: playing crazy arcade\n");
	static const char *crazy_notes = "MDOBQCOCRCQCOCMCLBMBLBMBOCLCHE";

	while (crazy_notes[i] && crazy_notes[i + 1]) {
		char tone = crazy_notes[i];
		char note = crazy_notes[i + 1];
		int msec = note2msecOfCrazyArcade[note - 'A'];

		// 쉼표
		if (tone == 's') {
			msleep(msec);
			i += 2;
			continue;
		}

		int freq = tone2freq[tone - 'A'];

		printk("tone=%c(%d Hz), note=%c(%d ms)\n",
		       tone, freq, note, msec);

		buzzer_beep(freq, msec);

		i += 2;
	}
}

void play_tetris(void) {
	int i = 0;
	ssize_t wlen;
	
	const static unsigned int note2msecOfTetris[] = {BASE / 8, BASE / 4, BASE, BASE*1.5, BASE*3};

	printk("buzzertest: playing tetris\n");
	static const char *tetris_notes = "LBLBMALAJAMALBJBHBJBHBHBJAHBGAHAEB";

	g_stop = 0;  /* reset stop flag at start */

	while (tetris_notes[i] && tetris_notes[i + 1]) {

		if (g_stop) break;  /* stop requested — exit immediately */

		char tone = tetris_notes[i];
		char note = tetris_notes[i + 1];
		int msec = note2msec[note - 'A'];

		// 쉼표
		if (tone == 's') {
			msleep(msec);
			i += 2;
			continue;
		}

		int freq = tone2freq[tone - 'A'];

		printk("tone=%c(%d Hz), note=%c(%d ms)\n",
		       tone, freq, note, msec);

		buzzer_beep(freq, msec);

		i += 2;
	}
	/* Ensure buzzer is silent after play (or early stop) */
	iowrite32(0x80, (void *)(pwm_base + PWM_CTL));
}

static long device_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case IOCTL_PLAY_TETRIS:
		play_tetris();
		break;
	case IOCTL_PLAY_CRAZY:
		play_crazy_arcade();
		break;
	case IOCTL_PLAY_MARIO:
		play_super_mario();
		break;
	case IOCTL_STOP:
		g_stop = 1;  /* signal any running play loop to exit */
		iowrite32(0x80, (void *)(pwm_base + PWM_CTL));  /* silence PWM immediately */
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static ssize_t device_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	return 0;
}

static ssize_t device_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	int i;
	ssize_t wlen;

	printk("buzzertest: device_write (minor = %d)\n", iminor(filp->f_path.dentry->d_inode));

	wlen = MAX_BUF; 
	if(wlen > count) wlen = count;
	if(copy_from_user(wbuf, buf, wlen)) {
		return -EFAULT;
	}

	/* Implement code */
	for (i = 0; i < wlen; i+=2) {
		char tone = wbuf[i];
		char note = wbuf[i + 1];
		int msec = note2msec[note - 'A'];
		if (tone == 's') {
		   mdelay(msec);
		   continue;
		}
		int freq = tone2freq[tone - 'A'];
		printk("tone=%c(%4d Hz), note = %c(%4d msec)\n", tone, freq, note ,msec);
		buzzer_beep(freq, msec);
	}
	return wlen;
}

static int device_open(struct inode *inode, struct file *filp)
{
	printk("buzzertest: device_open (minor = %d)\n", iminor(inode));
	return 0;
}

static int device_release(struct inode *inode, struct file *filp)
{
	printk("buzzertest: device_release\n");
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

static const struct of_device_id my_pdev_of_match[] = {
	{ .compatible = "my-buzzer", },
	{ },
};
MODULE_DEVICE_TABLE(of, my_pdev_of_match);

static int driver_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;
	struct clk *clk;

	printk("buzzertest: device_init\n");

	devt = MKDEV(device_major, device_minor_start);
	ret = register_chrdev_region(devt, device_minor_count, "my_device");
	if(ret < 0) {
		printk("buzzertest: can't get major %d\n", device_major);
		goto err0;
	}

	my_cdev = cdev_alloc();
	my_cdev->ops = &my_fops;
	my_cdev->owner = THIS_MODULE;
	ret = cdev_add(my_cdev, devt, device_minor_count);
	if(ret) {
		printk("buzzertest: can't add device %d\n", devt);
		goto err1;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		printk("buzzertest: can't allocate resource\n");
		ret = -ENODEV;
		goto err2;
	}
	phy_base = res->start;
	phy_size = res->end - res->start + 1;
	printk("buzzertest: phy_base is %x, and phy_size is 0x%x\n", phy_base, phy_size);

	pwm_mem = request_mem_region(phy_base, phy_size, "pwm");
	if (pwm_mem == NULL) {
		printk("buzzertest: failed to get memory region\n");
		ret = -EIO;
		goto err2;
	}

	pwm_base = (unsigned long)ioremap(phy_base, phy_size);
	if (pwm_base == 0) {
		printk("buzzertest: ioremap error\n");
		ret = -EIO;
		goto err3;
	}

	clk = clk_get(&pdev->dev, NULL);
	if (clk == 0) {
		printk("buzzertest: clock not found\n");
		ret = -EIO;
		goto err4;
	}

	ret = clk_prepare_enable(clk);
	if (ret) {
		printk("buzzertest: failed to enable clock\n");
		ret = -EIO;
		goto err4;
	}

	clk_rate = clk_get_rate(clk);
	printk("buzzertest: clock rate is %luHz\n", clk_rate);

	return 0;

err4:
	iounmap((void *)pwm_base);
err3:
	release_mem_region(phy_base, phy_size);
err2:
	cdev_del(my_cdev);
err1:
	unregister_chrdev_region(devt, device_minor_count);
err0:
	return ret;
}

static int driver_remove(struct platform_device *pdev)
{
	printk("buzzertest: device_exit\n");

	iounmap((void *)pwm_base);
	release_mem_region(phy_base, phy_size);
	cdev_del(my_cdev);
	unregister_chrdev_region(devt, device_minor_count);

	return 0;
}

static int driver_suspend(struct platform_device *pdev, pm_message_t pm)
{
	return 0;
}

static int driver_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver my_platform_driver = {
	.probe          = driver_probe,
	.remove         = driver_remove,
	.suspend        = driver_suspend,
	.resume         = driver_resume,
	.driver         = {
		.name   = DEVICE_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = my_pdev_of_match,
	},
};

static int __init platform_device_init(void)
{
	int ret;

	printk("buzzertest: platform_device_init\n");

	ret = platform_driver_register(&my_platform_driver);
	if(ret) {
		printk("platform_driver_register failed (ret=%d)\n", ret);
	}

	return ret;
}

static void __exit platform_device_exit(void)
{
	printk("buzzertest: platform_device_exit\n");

	platform_driver_unregister(&my_platform_driver);
}

module_init(platform_device_init);
module_exit(platform_device_exit);

MODULE_LICENSE("GPL");
