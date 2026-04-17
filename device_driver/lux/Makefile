obj-m := luxtest.o

KERNELDIR := ~/work/linux
PWD := $(shell pwd)
CC := aarch64-linux-gnu-gcc

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
	$(CC) -Wall -o apptest apptest.c
	$(CC) -Wall -o ioctl_test ioctl_test.c
	cp luxtest.ko apptest ioctl_test /nfsroot

clean:
	rm -f *.o *.ko *.mod *.mod.c modules.order Module.symvers
	rm -f .*.*.cmd .*.*.*.cmd
	rm -rf .tmp_versions
	rm -f apptest
	rm -f ioctl_test
