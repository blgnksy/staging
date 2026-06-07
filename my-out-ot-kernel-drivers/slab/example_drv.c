#include "example_drv.h"
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <stdio.h>
#include <sys/types.h>

// Cached node
struct cache_node {
	struct list_head list;
	struct my_data data;
};

// Slab cache
static struct kmem_cache *my_cache;

// The registry for the cached objects. Caches object of type my_data
static LIST_HEAD(node_list);
static int node_count;
static DEFINE_MUTEX(node_lock);

// Character device driver global variables
static dev_t g_dev;
static struct cdev g_cdev;
static struct class *example_class;

// Supported file operations
static int test_driver_open(struct inode *inodep, struct file *filp)
{
	return 0;
}

static int test_driver_release(struct inode *inodep, struct file *filp)
{
	return 0;
}

static ssize_t test_driver_read(struct file *filp, char *buf, size_t size, loff_t *off)
{
	return 0;
}

static ssize_t test_driver_write(struct file *filp, const char *buf, size_t size, loff_t *off)
{
	return 0;
}

static void free_all_nodes(void)
{
	struct cache_node *node, *tmp;

	mutex_lock(&node_lock);
	list_for_each_entry_safe(node, tmp, &node_list, list) {
		list_del(&node->list);
		kmem_cache_free(my_cache, node);
	}
	node_count = 0;
	mutex_unlock(&node_lock);
}

static long ioctl_create(unsigned long arg)
{
	struct my_data user;
	struct cache_node *node;

    // Fill user with the arguments from user space
	if (copy_from_user(&user, (void __user *)arg, sizeof(user)))
		return -EFAULT;

    // Allocate memory for object
	node = kmem_cache_alloc(my_cache, GFP_KERNEL);
	if (!node)
		return -ENOMEM;

    // The list node points itself
	INIT_LIST_HEAD(&node->list);
	node->data = user;

	mutex_lock(&node_lock);
	list_add_tail(&node->list, &node_list);
	node_count++;
	mutex_unlock(&node_lock);

	pr_info("%s: created object id=%d value=%d -> node count: %d\n",
		DRIVER_NAME, user.id, user.value, node_count);
	return 0;
}

static long ioctl_get(unsigned long arg)
{
	struct my_data user;
	struct cache_node *node;
	int found = 0;

	if (copy_from_user(&user, (void __user *)arg, sizeof(user)))
		return -EFAULT;

	mutex_lock(&node_lock);
	list_for_each_entry(node, &node_list, list) {
		if (node->data.id == user.id) {
			user.value = node->data.value;
			found = 1;
			pr_info("%s: retrieved object id=%d value=%d\n",
				DRIVER_NAME, node->data.id, node->data.value);
			break;
		}
	}
	mutex_unlock(&node_lock);

	if (!found)
		return -ENOENT;

	if (copy_to_user((void __user *)arg, &user, sizeof(user)))
		return -EFAULT;

	return 0;
}

static long ioctl_destroy(unsigned long arg)
{
	free_all_nodes();
	pr_info("%s: destroyed all cached objects\n", DRIVER_NAME);
	return 0;
}

// Dispatches the request to the corresponding io operation
static long test_driver_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long result;

	switch (cmd) {
	case IOCTL_CREATE:
		result = ioctl_create(arg);
		break;
	case IOCTL_GET:
		result = ioctl_get(arg);
		break;
	case IOCTL_DESTROY:
		result = ioctl_destroy(arg);
		break;
	default:
		result = -ENOTTY;
	}

	return result;
}

static struct file_operations g_fops = {
	.owner = THIS_MODULE,
	.open = test_driver_open,
	.read = test_driver_read,
	.write = test_driver_write,
	.release = test_driver_release,
	.unlocked_ioctl = test_driver_ioctl,
};

static int __init example_init(void)
{
	int result;

	pr_info("%s: loaded\n", DRIVER_NAME);

	my_cache = kmem_cache_create("my_cache", sizeof(struct cache_node), 0,
		SLAB_HWCACHE_ALIGN, NULL);
	if (!my_cache) {
		pr_err("%s: failed to create slab cache\n", DRIVER_NAME);
		return -ENOMEM;
	}

    // Allocate character device
	if ((result = alloc_chrdev_region(&g_dev, 0, 1, DRIVER_NAME)) < 0) {
		pr_err("%s: unable to allocate char device region\n", DRIVER_NAME);
		goto error_cache;
	}

    // Initialize and regoster the device
	cdev_init(&g_cdev, &g_fops);
	if ((result = cdev_add(&g_cdev, g_dev, 1)) < 0) {
		pr_err("%s: cdev_add failed\n", DRIVER_NAME);
		goto error_chrdev;
	}

    // Device driver class
	example_class = class_create(DRIVER_NAME);
	if (IS_ERR(example_class)) {
		result = PTR_ERR(example_class);
		pr_err("%s: class_create failed\n", DRIVER_NAME);
		goto error_cdev;
	}

    // Device driver creation
	if (IS_ERR(device_create(example_class, NULL, g_dev, NULL, DRIVER_NAME))) {
		result = -EINVAL;
		pr_err("%s: device_create failed\n", DRIVER_NAME);
		goto error_class;
	}

	return 0;

error_class:
	class_destroy(example_class);
error_cdev:
	cdev_del(&g_cdev);
error_chrdev:
	unregister_chrdev_region(g_dev, 1);
error_cache:
	kmem_cache_destroy(my_cache);
	return result;
}

static void __exit example_exit(void)
{
	free_all_nodes();
	if (example_class)
		device_destroy(example_class, g_dev);
	if (example_class)
		class_destroy(example_class);
	cdev_del(&g_cdev);
	unregister_chrdev_region(g_dev, 1);
	kmem_cache_destroy(my_cache);
	pr_info("%s: unloaded\n", DRIVER_NAME);
}

module_init(example_init);
module_exit(example_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bilgin Aksoy");
MODULE_DESCRIPTION("Slab cache example driver using kmem_cache_alloc/free");
