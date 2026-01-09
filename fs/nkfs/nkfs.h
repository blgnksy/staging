//
// Created by blgnksy on 14/12/2025.
//

#ifndef STAGING_NKFS_H
#define STAGING_NKFS_H

#include <linux/fs.h>
#include <linux/types.h>

/* Superblock magic number - choose a unique value */
#define NKFS_BLOCK_SIZE         		4096
#define NKFS_MAGIC           			0x4E4B4653
#define NKFS_INODE_BITMAP_LOCATION		1
#define NKFS_DATA_BITMAP_LOCATION		2
#define NKFS_INODE_TABLE_LOCATION		3
#define NKFS_ROOT_INO				 	1
#define NKFS_FILENAME_MAXLEN    		32
#define NKFS_MAX_DIR_ENTRIES		(NKFS_BLOCK_SIZE / sizeof(struct nkfs_disk_dir_entry))

/* Helper macros */
#define NKFS_SB(sb)			((struct nkfs_super_block *)((sb)->s_fs_info))
#define NKFS_DISK_SB(sb)		(NKFS_SB((sb))->sbd)
#define NKFS_DISK_INODE_SIZE		sizeof(struct nkfs_disk_inode)
#define NKFS_DISK_INODE_PER_BLOCK	(NKFS_BLOCK_SIZE / NKFS_DISK_INODE_SIZE)


/* Filesystem version */
#define NKFS_VERSION 1

/* Cache for inode structure */
/* Defined in super.c */

struct nkfs_disk_super_block {
	__le32 magic;
	__le32 block_size;
	__le32 inode_count;
	__le32 block_count;
	__le32 free_inodes;
	__le32 free_blocks;
	__le32 inode_table_block;
	__le32 inode_table_size;
	__le32 data_block_start;
	__u8 padding[4060];
};

struct nkfs_super_block {
	struct nkfs_disk_super_block *sbd;
	struct buffer_head *sb_bh;
	struct buffer_head *inode_bitmap_bh;
	struct buffer_head *data_bitmap_bh;
	unsigned long *inode_bitmap;
	unsigned long *data_bitmap;
	spinlock_t slock;
};

struct nkfs_inode {
	__u32 block_no;
	struct inode vfs_inode;
};

struct nkfs_disk_inode {
	__le32 mode;
	__le32 uid;
	__le32 gid;
	__le32 size;
	__le32 nlink;
	__le32 blocks;
	__le32 block_no;
	__le32 ctime;
	__le32 mtime;
	__le32 atime;
	__u8 padding[24];
};

struct nkfs_disk_dir_entry {
	__le32 inode;
	char name[NKFS_FILENAME_MAXLEN];
};



/* Global variables */
extern struct kmem_cache *nkfs_inode_cachep;
extern const struct file_operations nkfs_dir_inode_fops;
extern const struct inode_operations nkfs_dir_inode_ops;
extern const struct inode_operations nkfs_file_inode_ops;
extern struct file_system_type nkfs_type;

/* Inode operations */
struct inode *nkfs_alloc_inode(struct super_block *sb);
void nkfs_free_inode(struct inode *inode);
int nkfs_write_inode(struct inode *inode, struct writeback_control *wbc);
void nkfs_evict_inode(struct inode *inode);
struct inode *nkfs_iget(struct super_block *sb, unsigned long ino);
struct dentry *nkfs_lookup(struct inode *dir, struct dentry *dentry,
			   unsigned int flags);
struct dentry *nkfs_mkdir(struct mnt_idmap *idmap, struct inode *dir, struct dentry *dentry, umode_t mode);
struct inode *nkfs_new_inode(struct inode *dir, umode_t mode);
int nkfs_iterate_shared(struct file *file, struct dir_context *ctx);

/* Superblock operations */
int nkfs_fill_super(struct super_block *sb, void *data, int silent);
struct dentry *nkfs_mount(struct file_system_type *type, int flags,
			  const char *dev, void *data);
void nkfs_kill_sb(struct super_block *sb);

#endif //STAGING_NKFS_H
