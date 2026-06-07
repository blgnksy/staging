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

semaphore_test: semaphores_test.c
	gcc -o semaphores_test semaphores_test.c

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	insmod semaphores.ko
	insmod mutexes.ko

uninstall:
	rmmod semaphores.ko
	rmmod mutexes.ko

.PHONY: all clean
