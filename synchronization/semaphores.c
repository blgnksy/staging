// ...existing code...
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include "semaphores.h"

#define CLASS_NAME "semaphores_class"

static dev_t dev_number;
static struct cdev my_cdev;
static struct class *my_class;
static struct device *my_device;
static const struct file_operations fops;

static int counter;
static DEFINE_MUTEX(counter_lock);

/* File operation for opening */
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
        
        if (mutex_lock_interruptible(&counter_lock) == -EINTR)
            return -ERESTARTSYS;

        counter = tmp;
        mutex_unlock(&counter_lock);
        pr_info("semaphores: counter set to %d\n", counter);
        break;

    case SEMAPHORES_IOC_GET_COUNTER:
        if (mutex_lock_interruptible(&counter_lock) == -EINTR)
            return -ERESTARTSYS;

        tmp = counter;
        mutex_unlock(&counter_lock);
        if (copy_to_user((int __user *)arg, &tmp, sizeof(tmp)))
            return -EFAULT;
        pr_info("semaphores: counter %d returned\n", tmp);
        break;

    case SEMAPHORES_IOC_RESET:
        if (mutex_lock_interruptible(&counter_lock) == -EINTR)
            return -ERESTARTSYS;
            
        counter = 0;
        mutex_unlock(&counter_lock);
        pr_info("semaphores: counter reset\n");
        break;

    default:
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

    mutex_init(&counter_lock);
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
