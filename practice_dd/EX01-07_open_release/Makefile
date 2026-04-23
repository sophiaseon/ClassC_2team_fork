obj-m := devtest.o

KERNELDIR := ~/work/linux
PWD := $(shell pwd)
CC := aarch64-linux-gnu-gcc

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
	$(CC) -o apptest apptest.c
	cp devtest.ko /nfsroot
	cp apptest /nfsroot

clean:
	rm -f *.o *.ko *.mod *.mod.c modules.order Module.symvers
	rm -f .*.*.cmd .*.*.*.cmd
	rm -rf .tmp_versions
	rm -f apptest
