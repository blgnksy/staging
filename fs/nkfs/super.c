//
// Created by blgnksy on 14/12/2025.
//
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>

#include "nkfs.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bilgin Aksoy");
MODULE_DESCRIPTION("nkfs");

static struct file_system_type nkfs_type = {
	.owner = THIS_MODULE,
	.name = "nkfs",
	.mount = nkfs_mount,
	.kill_sb = nkfs_kill_sb,
	.fs_flags = FS_REQUIRES_DEV,
};

static const struct super_operations nkfs_super_ops = {
	.alloc_inode = nkfs_alloc_inode,
	.free_inode = nkfs_free_inode,
	.write_inode = nkfs_write_inode,
	.evict_inode = nkfs_evict_inode,
	.statfs = nkfs_statfs,
};

static int nkfs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct nkfs_super_block *nkfs_sb;

	sb->s_magic = NKFS_MAGIC;
	sb_set_blocksize(sb, NKFS_BLOCKSIZE);
	sb->s_maxbytes = NKFS_BLOCKSIZE;
	sb->s_op = &nkfs_super_ops;

	if ((nkfs_sb = kzalloc(sizeof(struct nkfs_super_block), GFP_KERNEL)) ==
	    NULL) {
		printk(KERN_INFO "cannot allocate nkfs super block!..\n");
		return -ENOMEM;
	}
	sb->s_fs_info = nkfs_sb;

	if ((sfs_sb->sb_bh = sb_bread(sb, 0)) == NULL) {
		printk(KERN_INFO "cannot read  nkfs disk super block!..\n");
		goto EXIT;
	}

	sbd = (struct nkfs_disk_super_block *) nkfs_sb->sb_bh->b_data;
	nkfs_sb->sbd = sbd;



	return 0;
}


static struct dentry *nkfs_mount(struct file_system_type *type, int flags,
                                 const char *dev, void *data)
{
	return mount_bdev(type, flags, dev, data, nkfs_fill_super);
}

static int __init nkfs_module_init(void)
{
	int result;

	if ((result = register_filesystem(&nkfs_type)) != 0)
		return result;

	printk(KERN_INFO "nkfs module init\n");

	return 0;
}

static void __exit nkfs_module_exit(void)
{
	printk(KERN_INFO "nkfs module exit\n");
}

module_init(nkfs_module_init);
module_exit(nkfs_module_exit);
