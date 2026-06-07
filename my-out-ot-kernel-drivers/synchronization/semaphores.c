// ...existing code...
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/semaphore.h>
#include "common.h"

#define CLASS_NAME "semaphores_class"

static dev_t dev_number;
static struct cdev my_cdev;
static struct class *my_class;
static struct device *my_device;
static const struct file_operations fops;

static int counter;
static DEFINE_SEMAPHORE(counter_lock, 3); // Initial count is 3

/**
 * The open operation is the driver callback invoked when userspace calls open(2) on the device node (e.g. /dev/mydev).
 * In VFS terms: 𝑜𝑝𝑒𝑛(2) → open(2)→ VFS → your file_operations.open (dev_open).
 * Purpose
 * - Associate per-open state with the struct file (use file->private_data).
 * - Map the inode/cdev to your device instance (minor/major → device structure).
 * - Check/claim the device (return -EBUSY if exclusive).
 * - Initialize hardware/software resources for that open.
 * - Respect flags like O_NONBLOCK, O_RDONLY/O_WRONLY.
 * - Return 0 on success or a negative errno on failure.
 * Key points / gotchas
 * - Set file->private_data so other ops (read/write/ioctl/release) can access device state.
 * - Ensure .owner = THIS_MODULE in struct file_operations so the module refcount is handled automatically; otherwise 
 * use try_module_get/put.
 * - Avoid sleeping in atomic contexts; don't call blocking functions if open can be invoked from atomic paths.
 * - Handle concurrent opens (use mutex/semaphore to enforce exclusive access or allow multiple opens).
 * - Match resources in release() — every allocation/claim in open should be undone in release.
 * - Return appropriate errno codes (e.g. -EBUSY, -ENOMEM).
 */
static int dev_open(struct inode *inode, struct file *file)
{
    pr_info("semaphores: device opened\n");
    return 0;
}

/* File operation for closing */
static int dev_release(struct inode *inode, struct file *file)
{
    pr_info("semaphores: device closed\n");
    return 0;
}

/* File operation for ioctl (When user space calls ioctl) */
static long dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int tmp;
    long ret = 0;

    if (_IOC_TYPE(cmd) != SEMAPHORES_IOC_MAGIC)
        return -ENOTTY;

    switch (cmd) {
    case SEMAPHORES_IOC_SET_COUNTER:
        if (copy_from_user(&tmp, (int __user *)arg, sizeof(tmp)))
            return -EFAULT;

        /* mutex_lock_interruptible returns 0 if the lock was successfully acquired or %-EINTR if a
         * signal arrived. The system programmer should return -ERESTARTSYS in such cases to inform the
         * calling process that the system call was interrupted by a signal and may be restarted.
         */
        if (down_interruptible(&counter_lock) != 0)
            return -ERESTARTSYS;

        counter = tmp;
        up(&counter_lock);
        pr_info("semaphores: counter set to %d\n", counter);
        break;

    case SEMAPHORES_IOC_GET_COUNTER:
        if (down_interruptible(&counter_lock) != 0)
            return -ERESTARTSYS;

        tmp = counter;
        up(&counter_lock);
        if (copy_to_user((int __user *)arg, &tmp, sizeof(tmp)))
            return -EFAULT;
        pr_info("semaphores: counter %d returned\n", tmp);
        break;

    case SEMAPHORES_IOC_RESET:
        if (down_interruptible(&counter_lock) != 0)
            return -ERESTARTSYS;
            
        counter = 0;
        up(&counter_lock);
        pr_info("semaphores: counter reset\n");
        break;

    default:
        /**
         * ENOTTY — "Not a typewriter"
         *
         * For system programmers:
         * - Indicates that an ioctl or other device-specific operation is inappropriate
         *   for the given file descriptor or device. It is commonly returned when a
         *   request code is not implemented for that device type (for example, a
         *   terminal-specific ioctl invoked on a pipe, socket, regular file, or a
         *   device that does not support that command).
         * - In kernel/driver code, return -ENOTTY to signal an unsupported ioctl/command
         *   for that device. User-space libraries and programs should validate requests
         *   (e.g., check isatty()) before issuing device-specific ioctls.
         *
         * For users and applications:
         * - Observed as errno == ENOTTY; libc may print "Inappropriate ioctl for device"
         *   (the historical message is "Not a typewriter").
         * - Typically means the program tried to perform a terminal-specific or device-
         *   specific operation on a file descriptor that does not support it.
         * - Handling: avoid making the inappropriate ioctl (use portable APIs or fall
         *   back to alternative methods), check the file type before the operation,
         *   and present a clear error message to the user.
         *
         * Notes:
         * - The literal phrase "Not a typewriter" is historical; modern descriptions
         *   prefer "Inappropriate ioctl for device".
         * - ENOTTY differs from EINVAL (invalid argument) and ENOTSUP/ENOSYS (operation
         *   not supported at all), though in practice errno usage can vary; callers
         *   should document and handle the specific error codes their interfaces may return.
         */
        ret = -ENOTTY;
        break;
    }

    return ret;
}

static const struct file_operations fops = {
    .owner = THIS_MODULE, 
    .open = dev_open,
    .release = dev_release,
    .unlocked_ioctl = dev_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl = dev_ioctl,
#endif
};

int init_module(void)
{
    int ret;

    pr_info("semaphores: init\n");

    /* allocate device number */
    ret = alloc_chrdev_region(&dev_number, 0, 1, SEMAPHORES_DEVICE_NAME);
    if (ret) {
        pr_err("semaphores: failed to alloc chrdev region\n");
        return ret;
    }

    cdev_init(&my_cdev, &fops);
    my_cdev.owner = THIS_MODULE;

    ret = cdev_add(&my_cdev, dev_number, 1);
    if (ret) {
        pr_err("semaphores: cdev_add failed\n");
        unregister_chrdev_region(dev_number, 1);
        return ret;
    }

    my_class = class_create(CLASS_NAME);
    if (IS_ERR(my_class)) {
        pr_err("semaphores: class_create failed\n");
        cdev_del(&my_cdev);
        unregister_chrdev_region(dev_number, 1);
        return PTR_ERR(my_class);
    }

    my_device = device_create(my_class, NULL, dev_number, NULL, SEMAPHORES_DEVICE_NAME);
    if (IS_ERR(my_device)) {
        pr_err("semaphores: device_create failed\n");
        class_destroy(my_class);
        cdev_del(&my_cdev);
        unregister_chrdev_region(dev_number, 1);
        return PTR_ERR(my_device);
    }

    counter = 0;

    pr_info("semaphores: device /dev/%s ready (major %d minor %d)\n",
        SEMAPHORES_DEVICE_NAME, MAJOR(dev_number), MINOR(dev_number));
    return 0;
}

void cleanup_module(void)
{
    device_destroy(my_class, dev_number);
    class_destroy(my_class);
    cdev_del(&my_cdev);
    unregister_chrdev_region(dev_number, 1);
    pr_info("semaphores: unloaded\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bilgin Aksoy");
MODULE_DESCRIPTION("Semaphore example with ioctl");
