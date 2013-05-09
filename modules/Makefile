KBUILD_EXTRA_SYMBOLS += Module.symvers.smpapi
export KBUILD_EXTRA_SYMBOLS

KERNEL_DIR := $(UCLINUX_KERNEL)

VERSION_H := $(wildcard $(KERNEL_DIR)/include/linux/version.h)
AUTOCONF_H := $(wildcard $(KERNEL_DIR)/include/linux/autoconf.h)
UTSRELEASE_H := $(wildcard $(KERNEL_DIR)/include/linux/utsrelease.h)

EXTRA_CFLAGS += -I$(PWD)/../include

.PHONY: all clean build
obj-m := fshmem.o

all: build

build:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
