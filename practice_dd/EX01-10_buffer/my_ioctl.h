#include <linux/ioctl.h>

#define MY_IOCTL_MAGIC 'k'

#define MY_IOCTL_CMD_CLEAR_BUF	_IO(MY_IOCTL_MAGIC, 1)
#define MY_IOCTL_CMD_GET_FREE_BUF_SIZE	_IOR(MY_IOCTL_MAGIC, 2, int)

