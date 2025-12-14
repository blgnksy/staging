//
// Created by blgnksy on 14/12/2025.
//

#ifndef STAGING_NKFS_H
#define STAGING_NKFS_H

#include <linux/fs.h>
#include <linux/types.h>

/* Superblock magic number - choose a unique value */
#define NKFS_MAGIC		0x4E4B4653  /* "NKFS" in hex */
#define NKFS_BLOCKSIZE		4096


/* Filesystem version */
#define NKFS_VERSION 1

/* Basic superblock structure (on-disk format) */
struct nkfs_super_block {
	struct nkfs_disk_super_block *sbd;
	struct buffer_head *sb_bh;
	struct buffer_head *inode_bitmap_bh;
	struct buffer_head *data_bitmap_bh;
	unsigned long *inode_bitmap;
	unsigned long *data_bitmap;
	spinlock_t lock;
};

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
	__u8   padding[4060];
};

/* In-memory superblock info */
struct nkfs_sb_info {
	struct nkfs_super_block *sb;
	unsigned long block_size;
	unsigned long inode_size;
};

/* Basic inode structure (on-disk format) */
struct nkfs_inode {
	__le32 mode;
	__le32 uid;
	__le32 gid;
	__le32 size;
	__le32 blocks;
	__le32 atime;
	__le32 mtime;
	__le32 ctime;
	__le32 nlink;
};

/* In-memory inode info */
struct nkfs_inode_info {
	__le32 vfs_inode;
	struct inode vfs_inode;
};

/* Function declarations */
/* Inode operations */
static struct inode *nkfs_alloc_inode(struct super_block *sb);
static void nkfs_free_inode(struct inode *inode);
static int nkfs_write_inode(struct inode *inode, struct writeback_control *wbc);
static void nkfs_evict_inode(struct inode *inode);


/* File operations */
static const struct file_operations nkfs_file_ops;
static const struct file_operations nkfs_dir_ops;


/* Super operations */
static const struct super_operations nkfs_super_ops;

/* Mount function */
struct dentry *nkfs_mount(struct file_system_type *fs_type,
			  int flags, const char *dev_name, void *data);

/* Helper functions */
struct inode *nkfs_new_inode(struct inode *dir, umode_t mode);
void nkfs_write_super(struct super_block *sb);
int nkfs_sync_fs(struct super_block *sb, int wait);



#endif //STAGING_NKFS_H
