obj-m := devtest.o

KERNELDIR := ~/work/linux
PWD := $(shell pwd)
CC := aarch64-linux-gnu-gcc

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
	$(CC) -Wall -o apptest apptest.c
	$(CC) -Wall -o apptest_nb apptest_nb.c
	cp devtest.ko apptest apptest_nb /nfsroot

clean:
	rm -f *.o *.ko *.mod *.mod.c modules.order Module.symvers
	rm -f .*.*.cmd .*.*.*.cmd
	rm -rf .tmp_versions
	rm -f apptest apptest_nb
