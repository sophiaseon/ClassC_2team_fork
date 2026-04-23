/*
 * Simple block ramdisk driver
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/slab.h>	
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/hdreg.h>
#include <linux/kdev_t.h>
#include <linux/vmalloc.h>
#include <linux/blk-mq.h>
#include <linux/idr.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>
#include <linux/bio.h>

#include "my_ioctl.h"

static int mybrd_major = 0;
module_param(mybrd_major, int, 0);

static int nsectors = 1024;
module_param(nsectors, int, 0);

unsigned long lbs = 512;
module_param(lbs, ulong, 0);

unsigned long pbs = 512;
module_param(pbs, ulong, 0);

unsigned long max_segments = 32;
module_param(max_segments, ulong, 0);

unsigned long max_segment_size = 65536;
module_param(max_segment_size, ulong, 0);

#define MYBRD_MINORS	16

struct mybrd_dev {
	int size;                       /* Device size in sectors */
	u8 *data;                       /* The data array */
	struct blk_mq_tag_set tag_set;  /* The tag set structure */
	struct gendisk *gd;             /* The gendisk structure */
};

static struct mybrd_dev *device = NULL;

static void mybrd_transfer(struct mybrd_dev *dev, unsigned long sector,
		char *buffer, unsigned long len, int write)
{
	unsigned long offset = sector << SECTOR_SHIFT;

	printk ("mybrd: write=%u sec=%ld buf=%p len=%ld\n",
			write, sector, buffer, len);

	if ((offset + len) > dev->size << SECTOR_SHIFT) {
		printk ("mybrd: exceed range (%ld %ld)\n", offset, len);
		return;
	}
	if (write)
		memcpy(dev->data + offset, buffer, len);
	else
		memcpy(buffer, dev->data + offset, len);
}

static blk_status_t mybrd_queue_rq(struct blk_mq_hw_ctx *hctx,
		const struct blk_mq_queue_data *bd)
{
	struct request *rq = bd->rq;
	blk_status_t err = BLK_STS_OK;
	struct bio_vec bv;
	struct req_iterator iter;
	loff_t sector = blk_rq_pos(rq);
	struct mybrd_dev *dev = hctx->queue->queuedata;

	blk_mq_start_request(rq);

	rq_for_each_segment(bv, rq, iter) {
		unsigned int len = bv.bv_len;
		void *buf = page_address(bv.bv_page) + bv.bv_offset;

		switch (req_op(rq)) {
			case REQ_OP_READ:
				mybrd_transfer(dev, sector, buf, len, 0);
				break;
			case REQ_OP_WRITE:
				mybrd_transfer(dev, sector, buf, len, 1);
				break;
			default:
				err = BLK_STS_IOERR;
				goto end_request;
		}
		sector += len >> SECTOR_SHIFT;
	}

end_request:
	blk_mq_end_request(rq, err);
	return BLK_STS_OK;
}

static const struct blk_mq_ops mybrd_mq_ops = {
	.queue_rq = mybrd_queue_rq,
};

static int mybrd_open(struct block_device *bd, fmode_t fmode)
{
	printk("mybrd: mybrd_open()\n");
	return 0;
}

static void mybrd_release(struct gendisk *gd, fmode_t fmode)
{
	printk("mybrd: mybrd_release()\n");
}

int mybrd_ioctl(struct block_device *bd, fmode_t fmode, unsigned cmd, unsigned long arg)
{
	printk("mybrd: mybrd_ioctl()\n");
	return 0;
}

static const struct block_device_operations mybrd_rq_ops = {
	.owner = THIS_MODULE,
	.open            = mybrd_open,
	.release         = mybrd_release,
	.ioctl           = mybrd_ioctl,
};

static void setup_device(struct mybrd_dev *dev)
{
	int ret;

	memset (dev, 0, sizeof (struct mybrd_dev));
	dev->size = nsectors;
	dev->data = vmalloc(dev->size << SECTOR_SHIFT);
	if (dev->data == NULL) {
		printk ("mybrd: vmalloc failure.\n");
		goto vmalloc_err;
	}

	memset(&dev->tag_set, 0, sizeof(dev->tag_set));
	dev->tag_set.ops = &mybrd_mq_ops;
	dev->tag_set.queue_depth = 128;
	dev->tag_set.numa_node = NUMA_NO_NODE;
	dev->tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
	dev->tag_set.cmd_size = 0;
	dev->tag_set.driver_data = dev;
	dev->tag_set.nr_hw_queues = 1;

	ret = blk_mq_alloc_tag_set(&dev->tag_set);
	if (ret) {
		goto tag_set_err;
	}

	dev->gd = blk_mq_alloc_disk(&dev->tag_set, dev);

	blk_queue_logical_block_size(dev->gd->queue, lbs);
	blk_queue_physical_block_size(dev->gd->queue, pbs);
	blk_queue_max_segments(dev->gd->queue, max_segments);
	blk_queue_max_segment_size(dev->gd->queue, max_segment_size);

	if (IS_ERR(dev->gd)) {
		ret = PTR_ERR(dev->gd);
		pr_err("Error allocating a disk\n");
		goto gd_err;
	}

	dev->gd->major = mybrd_major;
	dev->gd->first_minor = 0;
	dev->gd->minors = MYBRD_MINORS;
	snprintf (dev->gd->disk_name, 32, "mybrd");
	dev->gd->fops = &mybrd_rq_ops;
	set_capacity(dev->gd, dev->size);

	ret = add_disk(dev->gd);
	if (ret < 0)
		goto add_disk_err;

	return;

add_disk_err:
	put_disk(dev->gd);
gd_err:
tag_set_err:
	kfree(dev->data);
vmalloc_err:
	return;
}

static int __init device_init(void)
{
	int ret;

	printk("mybrd: device_init()\n");

	if(mybrd_major) {
		ret = register_blkdev(mybrd_major, "mybrd");
		if (ret < 0) {
			printk("mybrd: unable to get major number\n");
			return -EBUSY;
		}
	}
	else {
		mybrd_major = register_blkdev(0, "mybrd");
		if (mybrd_major < 0) {
			printk("mybrd: unable to get major number\n");
			return -EBUSY;
		}
	}

	device = kmalloc(sizeof(struct mybrd_dev), GFP_KERNEL);
	if (device == NULL)
		goto out_unregister;

	setup_device(device);

	return 0;

out_unregister:
	unregister_blkdev(mybrd_major, "mybrd");
	return -ENOMEM;
}

static void device_exit(void)
{
	struct mybrd_dev *dev = device;

	if (dev->gd) {
		del_gendisk(dev->gd);
		put_disk(dev->gd);
	}
	if (dev->data) {
		vfree(dev->data);
	}

	unregister_blkdev(mybrd_major, "mybrd");
	kfree(dev);
}

module_init(device_init);
module_exit(device_exit);
MODULE_LICENSE("GPL");

