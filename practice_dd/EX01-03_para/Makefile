obj-m := para.o

KERNELDIR := ~/work/linux
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
	cp para.ko /nfsroot

clean:
	rm -f *.o *.ko *.mod *.mod.c modules.order Module.symvers
	rm -f .*.*.cmd .*.*.*.cmd
	rm -rf .tmp_versions

