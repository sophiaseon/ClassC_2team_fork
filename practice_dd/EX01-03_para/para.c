#include <linux/init.h>
#include <linux/module.h>

static int p_int = 10;

static int pa_int[] = {1,2,3,4};
static int count;

module_param(p_int, int, S_IRUGO|S_IWUSR);
module_param_array(pa_int, int, &count, 0);

static int __init para_init(void)
{
	printk("p_int=%d\n", p_int);
	printk("pa_int[0]=%d\n", pa_int[0]);
	printk("pa_int[1]=%d\n", pa_int[1]);
	printk("pa_int[2]=%d\n", pa_int[2]);
	printk("pa_int[3]=%d\n", pa_int[3]);
	printk("count=%d\n", count);

	return 0;
}

static void __exit para_exit(void)
{
	printk("exit!\n");
}

module_init(para_init);
module_exit(para_exit);

MODULE_LICENSE("GPL");

