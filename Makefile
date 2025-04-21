obj-m += Block_Device_Driver.o

#KDIR = /lib/modules/6.8.0-57-generic/build
#all:
#	make -C $(KDIR)  M=$(shell pwd) modules
#clean:
#	make -C $(KDIR)  M=$(shell pwd) clean
	


KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
EXTRA_CFLAGS += -g -DDEBUG

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	sudo rmmod Block_Device_Driver 2>/dev/null || true
	sudo rm -f /dev/myblockdisk
