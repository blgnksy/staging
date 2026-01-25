# List module object(s)
obj-m += semaphores.o
obj-m += mutexes.o

# If module has multiple .c files:
# semaphores-objs := semaphores.o helper.o

# Path to custom kernel source
KDIR := /workspaces/staging
PWD  := $(CURDIR)

all:
	# Prepare kernel headers (if not done yet)
	$(MAKE) -C $(KDIR) M=$(PWD) modules_prepare
	# Build external module
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	sudo insmod semaphores.ko

uninstall:
	sudo rmmod semaphores.ko

.PHONY: all clean
