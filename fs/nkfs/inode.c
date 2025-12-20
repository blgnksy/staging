//
// Created by blgnksy on 14/12/2025.
//
#include "nkfs.h"

static struct inode *nkfs_alloc_inode(struct super_block *sb)
{
	struct nkfs_inode *inode_nkfs;

	if ((inode_nkfs = kmem_cache_alloc(nkfs_inode_cachep, GFP_KERNEL)) == NULL) 
		return NULL;

	return &inode_nkfs->vfs_inode;
}
static void nkfs_free_inode(struct inode *inode)
{
	struct nkfs_inode *inode_nkfs = container_of(inode, struct nkfs_inode, vfs_inode);

	kmem_cache_free(nkfs_inode_cachep, inode_nkfs);
}
static int nkfs_write_inode(struct inode *inode, struct writeback_control *wbc)
{
	return 0;
}
static void nkfs_evict_inode(struct inode *inode)
{
	/* */
}
