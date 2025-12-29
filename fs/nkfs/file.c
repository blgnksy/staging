//
// Created by blgnksy on 14/12/2025.
//
#include "nkfs.h"

int nkfs_iterate_shared(struct file *file,
                        struct dir_context *ctx)
{
    struct inode *inode;
	struct nkfs_inode *inode_nkfs;

    inode = file_inode(file);
	inode_nkfs = container_of(inode, struct nkfs_inode, vfs_inode);

    if (ctx->pos >= NKFS_MAX_DIR_ENTRIES) 
        return 0;

    if (inode_nkfs->block_no == 0) {
        printk(KERN_INFO "NKFS: unallocated disk block for directory!..\n");
        return 0;
    }
    return 0; 
}