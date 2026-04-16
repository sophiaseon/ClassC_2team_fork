#ifndef _MY_IOCTL_H_
#define _MY_IOCTL_H_

#include <linux/ioctl.h>

#define MY_IOCTL_MAGIC 'M'

#define IOCTL_PLAY_TETRIS   _IO(MY_IOCTL_MAGIC, 0)
#define IOCTL_PLAY_CRAZY    _IO(MY_IOCTL_MAGIC, 1)
#define IOCTL_PLAY_MARIO    _IO(MY_IOCTL_MAGIC, 2)

#define IOCTL_STOP          _IO(MY_IOCTL_MAGIC, 3)
#define IOCTL_GET_STATUS    _IOR(MY_IOCTL_MAGIC, 4, int)

#endif
