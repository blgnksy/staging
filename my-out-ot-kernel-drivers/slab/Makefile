# Minimal out-of-tree kernel module build
obj-m := example_drv.o
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

.ONESHELL:

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

load:
	@if ! lsmod | grep -q '^example_drv\b'; then \
		sudo insmod example_drv.ko; \
		echo "example_drv loaded"; \
	else \
		echo "example_drv already loaded"; \
	fi

test: load
	sudo python3 test.py

unload:
	sudo rmmod example_drv
