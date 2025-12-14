#include <linux/module.h>
#include <linux/buffer_head.h>

#include "nkfs.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bilgin Aksoy");
MODULE_DESCRIPTION("nkfs");

/* MACROS */
#define NKFS_MAX_DIR_ENTRIES		(NKFS_BLOCK_SIZE / sizeof(struct nkfs_disk_dir_entry))
#define NKFS_SB(sb)			((struct nkfs_super_block *)((sb->s_fs_info)))
#define NKFS_DISK_SB(sb)		(NKFS_SB((sb))->sbd)
#define NKFS_DISK_INODE_SIZE		sizeof(struct nkfs_disk_inode)
#define NKFS_DISK_INODE_PER_BLOCK	(NKFS_BLOCKSIZE / NKFS_DISK_INODE_SIZE)


static struct kmem_cache *nkfs_inode_cachep;

static int __init nkfs_module_init(void)
{
	int result;

	if ((nkfs_inode_cachep = kmem_cache_create(
		     "nkfs_inode_cache", sizeof(struct nkfs_inode),
		     0, SLAB_RECLAIM_ACCOUNT | SLAB_ACCOUNT, NULL)) == NULL) {
		printk(KERN_ERR "cannot allocate slab cache\n");
		return -ENOMEM;
	}

	if ((result = register_filesystem(&nkfs_type)) != 0) {
		printk(KERN_ERR "cannot register file system\n");
		goto EXIT;
	}

	printk(KERN_INFO "nkfs module init\n");
EXIT:
	kmem_cache_destroy(nkfs_inode_cachep);

	return result;
}

static void __exit nkfs_module_exit(void)
{
	unregister_filesystem(&nkfs_type);
	kmem_cache_destroy(nkfs_inode_cachep);

	printk(KERN_INFO "nkfs module exit\n");
}

static struct dentry *nkfs_mount(struct file_system_type *type, int flags,
                                 const char *dev, void *data)
{
	return mount_bdev(type, flags, dev, data, nkfs_fill_super);
}

static int nkfs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct nkfs_super_block *sfs_sb;
	struct nkfs_disk_super_block *sbd;
	struct inode *root_inode;
	int retval;

	sb->s_magic = NKFS_MAGIC;
	sb_set_blocksize(sb, NKFS_BLOCK_SIZE);
	sb->s_maxbytes = NKFS_BLOCK_SIZE;
	sb->s_op = &nkfs_super_ops;

	if ((sfs_sb = kzalloc(sizeof(struct nkfs_super_block), GFP_KERNEL)) ==
	    NULL) {
		printk(KERN_INFO "cannot allocate nkfs super block!..\n");
		return -ENOMEM;
	}
	sb->s_fs_info = sfs_sb;

	if ((sfs_sb->sb_bh = sb_bread(sb, 0)) == NULL) {
		printk(KERN_INFO "cannot read  nkfs disk super block!..\n");
		goto EXIT;
	}
	retval = -EINVAL;

	sbd = (struct nkfs_disk_super_block *)sfs_sb->sb_bh->b_data;
	sfs_sb->sbd = sbd;

	if (le32_to_cpu(sbd->magic) != NKFS_MAGIC) {
		printk(KERN_INFO "invalid magic number for simpls: %08X\n",
		       sbd->magic);
		goto EXIT;
	}
	if (le32_to_cpu(sbd->block_size) != NKFS_BLOCK_SIZE) {
		printk(KERN_INFO "invalid block size for simpls: %08X\n",
		       sbd->block_size);
		goto EXIT;
	}
	if (le32_to_cpu(sbd->inode_table_block) != 3) {
		printk(KERN_INFO "invalid inode table for simpls: %08X\n",
		       sbd->inode_table_block);
		goto EXIT;
	}

	if ((sfs_sb->inode_bitmap_bh = sb_bread(sb, NKFS_INODE_BITMAP_LOCATION))
	    == NULL) {
		printk(KERN_INFO "cannot read nkfs inode bitmap!..\n");
		goto EXIT;
	}
	sfs_sb->inode_bitmap = (unsigned long *)sfs_sb->inode_bitmap_bh->b_data;

	if ((sfs_sb->data_bitmap_bh = sb_bread(sb, NKFS_DATA_BITMAP_LOCATION))
	    == NULL) {
		printk(KERN_INFO "cannot read nkfs data bitmap!..\n");
		goto EXIT;
	}
	sfs_sb->data_bitmap = (unsigned long *)sfs_sb->data_bitmap_bh->b_data;

	root_inode = nkfs_iget(sb, NKFS_ROOT_INO);
	if (IS_ERR(root_inode)) {
		printk(KERN_INFO "cannot read root inode!..\n");
		retval = PTR_ERR(root_inode);
		goto EXIT;
	}

EXIT:
	/* ... */

	return retval;
}

static void nkfs_kill_sb(struct super_block *sb)
{
}

static struct inode *nkfs_alloc_inode(struct super_block *sb)
{
	struct nkfs_inode *inode_nkfs;

	if ((inode_nkfs = kmem_cache_alloc(nkfs_inode_cachep, GFP_KERNEL)) ==
	    NULL)
		return NULL;

	return &inode_nkfs->vfs_inode;
}

static void nkfs_free_inode(struct inode *inode)
{
	struct
	nkfs_inode *inode_nkfs = inode_nkfs = container_of(
		                         inode, struct nkfs_inode, vfs_inode);

	kmem_cache_free(nkfs_inode_cachep, inode_nkfs);
}

static int nkfs_write_inode(struct inode *inode, struct writeback_control *wbc)
{
	return 0;
}

static void nkfs_evict_inode(struct inode *inode)
{
}

static struct inode *nkfs_iget(struct super_block *sb, unsigned long ino)
{
	struct inode *inode;
	struct nkfs_disk_inode *disk_inode;
	struct buffer_head *bh;
	struct nkfs_disk_super_block *nkfs_sbd;
	struct nkfs_inode *inode_nkfs;

	nkfs_sbd = NKFS_DISK_SB(sb);
	if (ino >= nkfs_sbd->inode_count)
		return ERR_PTR(-EINVAL);

	if ((inode = iget_locked(sb, ino)) == NULL)
		return ERR_PTR(-ENOMEM);

	if (!(inode->i_state & I_NEW))
		return inode;

	disk_inode = nkfs_get_disk_inode(sb, ino, &bh);
	if (IS_ERR(disk_inode)) {
		iget_failed(inode);
		return (struct inode *)disk_inode;
	}

	inode->i_mode = le32_to_cpu(disk_inode->mode);
	i_uid_write(inode, le32_to_cpu(disk_inode->uid));
	i_gid_write(inode, le32_to_cpu(disk_inode->gid));
	inode->i_size = le32_to_cpu(disk_inode->size);
	set_nlink(inode, le32_to_cpu(disk_inode->nlink));
	inode->i_blocks = le32_to_cpu(disk_inode->blocks);
	inode->i_size = le32_to_cpu(disk_inode->size);
	inode_set_atime(inode, le32_to_cpu(disk_inode->atime), 0);
	inode_set_mtime(inode, le32_to_cpu(disk_inode->mtime), 0);
	inode_set_ctime(inode, le32_to_cpu(disk_inode->ctime), 0);

	inode_nkfs = container_of(inode, struct nkfs_inode, vfs_inode);
	inode_nkfs->block_no = disk_inode->block_no;

	if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &nkfs_dir_inode_ops;
		/* ... */
	} else {
		inode->i_op = &nkfs_file_inode_ops;
		/* ... */
	}

	return inode;
}

static struct nkfs_disk_inode *nkfs_get_disk_inode(struct super_block *sb,
                                                   unsigned long ino,
                                                   struct buffer_head **bhpp)
{
	int block_no, block_offset;
	struct buffer_head *bh;
	struct nkfs_disk_inode *disk_inode;

	block_no = NKFS_INODE_TABLE_LOCATION + ino * NKFS_DISK_INODE_SIZE /
	           NKFS_BLOCK_SIZE;
	block_offset = ino * NKFS_DISK_INODE_SIZE % NKFS_BLOCK_SIZE;

	if ((bh = sb_bread(sb, block_no)) == NULL)
		return ERR_PTR(-EIO);

	disk_inode = (struct nkfs_disk_inode *)(bh->b_data + block_offset);
	*bhpp = bh;

	return disk_inode;
}

static struct dentry *nkfs_lookup(struct inode *dir, struct dentry *dentry,
                                  unsigned int flags)
{
	struct super_block *sb = dir->i_sb;
	struct
	nkfs_inode *inode_nkfs =
		container_of(dir, struct nkfs_inode, vfs_inode);
	struct buffer_head *bh;
	struct nkfs_disk_dir_entry *de;
	struct inode *inode = NULL;
	int i;

	if (dentry->d_name.len > NKFS_FILENAME_MAXLEN)
		return ERR_PTR(-ENAMETOOLONG);

	if ((bh = sb_bread(sb, inode_nkfs->block_no)) == NULL)
		return ERR_PTR(-EIO);

	de = (struct nkfs_disk_dir_entry *)bh->b_data;
	for (i = 0; i < NKFS_MAX_DIR_ENTRIES; ++i) {
		if (de[i].inode == 0) /* deleted entry */
			continue;
		if (strcmp(de[i].name, dentry->d_name.name) == 0) {
			inode = nkfs_iget(sb, le32_to_cpu(de[i].inode));
			if (IS_ERR(inode))
				return (struct dentry *)inode;
			break;
		}
	}

	brelse(bh);

	return d_splice_alias(inode, dentry);
}

module_init(nkfs_module_init);
module_exit(nkfs_module_exit);
