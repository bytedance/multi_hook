

MODULE := multi_hook
obj-m += $(MODULE).o
CFLAGS_$(MODULE).o := -DDEBUG
$(MODULE)-y := multi_hook_lib.o multi_hook_manager.o const_func_hook.o
KERNDIR	:= /lib/modules/$(shell uname -r)/build


.PHONY: insmod rmmod build clean
default: build

build: $(MODULE).ko
$(MODULE).ko: multi_hook_lib.c multi_hook_manager.c multi_hook.h const_func_hook.c const_func_hook.h
	$(MAKE) -C $(KERNDIR) M=$(CURDIR) modules

clean:
	rm -f *.ko *.o  *.o.d *.mod.o *.mod.c *.order *.symvers .multi_hook*
	rm -rf .tmp_versions

insmod: rmmod build
	sudo insmod $(MODULE).ko dyndbg=+p

rmmod:
	-sudo rmmod $(MODULE)
