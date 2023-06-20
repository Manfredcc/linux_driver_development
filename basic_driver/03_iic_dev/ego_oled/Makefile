KERNELDIR := /home/ll/03_work/ref_storage/linux_alpha/kernel
KBUILD_CFLAGS += -g
CURRENT_PATH := $(shell pwd)

egoist-objs := ego_oled.o oled_lib.o ssd1306.o
obj-m := egoist.o

build: kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean
