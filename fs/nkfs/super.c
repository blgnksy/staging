#include <linux/module.h>
#include <linux/buffer_head.h>

#include "nkfs.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bilgin Aksoy");
MODULE_DESCRIPTION("nkfs");

/* MACROS */
/* MACROS defined in nkfs.h */

struct kmem_cache *nkfs_inode_cachep;

struct file_system_type nkfs_type = {
	.owner = THIS_MODULE,
	.name = "nkfs",
	.mount = nkfs_mount,
	.kill_sb = nkfs_kill_sb,
	.fs_flags = FS_REQUIRES_DEV,   
};

const struct super_operations nkfs_super_ops = {
	.alloc_inode = nkfs_alloc_inode,
	.free_inode = nkfs_free_inode,
	.write_inode = nkfs_write_inode,
	.evict_inode = nkfs_evict_inode,
	.statfs = simple_statfs,
};




static int __init nkfs_module_init(void)
{
	int result;

	printk(KERN_INFO "NKFS: initializing nkfs module...\n");
	if ((nkfs_inode_cachep = kmem_cache_create(
		     "nkfs_inode_cache", sizeof(struct nkfs_inode),
		     0, _SLAB_HWCACHE_ALIGN, NULL)) == NULL) {
		printk(KERN_ERR "cannot allocate slab cache\n");
		return -ENOMEM;
	}

	printk(KERN_INFO "NKFS: registering nkfs file system...\n");

	if ((result = register_filesystem(&nkfs_type)) != 0) {
		printk(KERN_ERR "cannot register file system\n");
		goto EXIT;
	}

	printk(KERN_INFO "NKFS: nkfs module init\n");
	return 0;
EXIT:
	kmem_cache_destroy(nkfs_inode_cachep);

	return result;
}

static void __exit nkfs_module_exit(void)
{
	unregister_filesystem(&nkfs_type);
	kmem_cache_destroy(nkfs_inode_cachep);

	printk(KERN_INFO "NKFS: nkfs module exit\n");
}

struct dentry *nkfs_mount(struct file_system_type *type, int flags,
                                 const char *dev, void *data)
{
	return mount_bdev(type, flags, dev, data, nkfs_fill_super);
}

int nkfs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct nkfs_super_block *nkfs_sb;
	struct nkfs_disk_super_block *sbd;
	struct inode *root_inode;
	int retval;

	sb->s_magic = NKFS_MAGIC;
	sb_set_blocksize(sb, NKFS_BLOCK_SIZE);
	sb->s_maxbytes = NKFS_BLOCK_SIZE;
	sb->s_op = &nkfs_super_ops;
	sb->s_flags |= SB_NOATIME;

	if ((nkfs_sb = kzalloc(sizeof(struct nkfs_super_block), GFP_KERNEL)) ==
	    NULL) {
		printk(KERN_INFO "NKFS: cannot allocate nkfs super block!..\n");
		return -ENOMEM;
	}
	sb->s_fs_info = nkfs_sb;
	spin_lock_init(&nkfs_sb->slock);

	if ((nkfs_sb->sb_bh = sb_bread(sb, 0)) == NULL) {
		printk(KERN_INFO "NKFS: cannot read  nkfs disk super block!..\n");
		retval = -EINVAL;
		goto EXIT1;
	}
	

	sbd = (struct nkfs_disk_super_block *)nkfs_sb->sb_bh->b_data;
	nkfs_sb->sbd = sbd;

	if (le32_to_cpu(sbd->magic) != NKFS_MAGIC) {
		printk(KERN_INFO "NKFS: invalid magic number for nkfs: %08X\n",
		       sbd->magic);
		goto EXIT1;
	}
	if (le32_to_cpu(sbd->block_size) != NKFS_BLOCK_SIZE) {
		printk(KERN_INFO "NKFS: invalid block size for nkfs: %08X\n",
		       sbd->block_size);
		goto EXIT1;
	}
	if (le32_to_cpu(sbd->inode_table_block) != 3) {
		printk(KERN_INFO "NKFS: invalid inode table for nkfs: %08X\n",
		       sbd->inode_table_block);
		goto EXIT1;
	}

	if ((nkfs_sb->inode_bitmap_bh = sb_bread(sb, NKFS_INODE_BITMAP_LOCATION))
	    == NULL) {
		printk(KERN_INFO "NKFS: cannot read nkfs inode bitmap!..\n");
		goto EXIT1;
	}
	nkfs_sb->inode_bitmap = (unsigned long *)nkfs_sb->inode_bitmap_bh->b_data;

	if ((nkfs_sb->data_bitmap_bh = sb_bread(sb, NKFS_DATA_BITMAP_LOCATION))
	    == NULL) {
		printk(KERN_INFO "NKFS: cannot read nkfs data bitmap!..\n");
		goto EXIT2;
	}
	nkfs_sb->data_bitmap = (unsigned long *)nkfs_sb->data_bitmap_bh->b_data;

	root_inode = nkfs_iget(sb, NKFS_ROOT_INO);
	if (IS_ERR(root_inode)) {
		printk(KERN_INFO "NKFS: cannot read root inode!..\n");
		retval = PTR_ERR(root_inode);
		goto EXIT3;
	}

	if ((sb->s_root = d_make_root(root_inode)) == NULL) {
		printk(KERN_INFO "NKFS: cannot create root dentry!..\n");
		retval = -ENOMEM;
		goto EXIT4;
	}

	return 0;

EXIT4:
	iput(root_inode);
EXIT3:
	brelse(nkfs_sb->data_bitmap_bh);
EXIT2:
	brelse(nkfs_sb->inode_bitmap_bh);
EXIT1:
	kfree(nkfs_sb);

	return retval;
}

void nkfs_kill_sb(struct super_block *sb)
{
	struct nkfs_super_block *nkfs_sb = NKFS_SB(sb);

	kill_block_super(sb);

	if (nkfs_sb) {
		if (nkfs_sb->data_bitmap_bh)
			brelse(nkfs_sb->data_bitmap_bh);
		if (nkfs_sb->inode_bitmap_bh)
			brelse(nkfs_sb->inode_bitmap_bh);
		if (nkfs_sb->sb_bh)
			brelse(nkfs_sb->sb_bh);
		kfree(nkfs_sb);
	}

	printk(KERN_INFO "NKFS: unmount super block...\n");
}

/* Inode functions moved to inode.c */
module_init(nkfs_module_init);
module_exit(nkfs_module_exit);
