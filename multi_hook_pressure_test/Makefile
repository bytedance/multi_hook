


MODULE_NAME		:= multi_hook_pressure_test


ifneq ($(KERNELRELEASE),)
obj-m			:= $(MODULE_NAME).o
# $(MODULE_NAME)-m	:= init.o
# $(MODULE_NAME)-m	+= kprobe.o trace.o
# ldflags-y		+= -r -T $(KBUILD_EXTMOD)/kprobe.lds
# ccflags-y		+= -I$(MULTI_HOOK_DIR)
ccflags-y		+= -DDEBUG 
ccflags-y		+= -I$(KBUILD_EXTMOD)/../multi_hook

KBUILD_MODPOST_WARN=1


else
PWD			:= $(shell pwd)
KERNEL_HEAD		:= $(shell uname -r)
KERNEL_DIR		:= /lib/modules/$(KERNEL_HEAD)/build

build:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean


insmod: rmmod build
	sudo insmod $(MODULE_NAME).ko

rmmod:
	-sudo rmmod $(MODULE_NAME)


endif
