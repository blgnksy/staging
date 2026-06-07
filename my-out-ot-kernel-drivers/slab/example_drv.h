#ifndef EXAMPLE_DRV_H
#define EXAMPLE_DRV_H

#include <linux/ioctl.h>

#define DRIVER_NAME "example_drv"

struct my_data {
	int id;
	int value;
};

#define TEST_DRIVER_MAGIC 't'
#define IOCTL_CREATE _IOW(TEST_DRIVER_MAGIC, 0, struct my_data)
#define IOCTL_GET _IOWR(TEST_DRIVER_MAGIC, 1, struct my_data)
#define IOCTL_DESTROY _IO(TEST_DRIVER_MAGIC, 2)

#endif /* EXAMPLE_DRV_H */
