#include "my_ioctl.h"
#include "fcntl.h"

int main() {
    int fd = open("/dev/mybuzzer", O_RDWR);

    // 재생 - cray arc
    ioctl(fd, IOCTL_PLAY_CRAZY); // IOCTL_PLAY_TERIS, IOCTL_PLAY_MARIO
    // 멈춤
    ioctl(fd, IOCTL_STOP);

    close(fd);
}
