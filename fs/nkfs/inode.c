#include <linux/buffer_head.h>
#include <linux/slab.h>
#include <linux/namei.h>
#include "nkfs.h"

/* Inode Operations */

struct inode *nkfs_alloc_inode(struct super_block *sb)
{
	struct nkfs_inode *inode_nkfs;

	if ((inode_nkfs = kmem_cache_alloc(nkfs_inode_cachep, GFP_KERNEL)) ==
	    NULL)
		return NULL;

	return &inode_nkfs->vfs_inode;
}

void nkfs_free_inode(struct inode *inode)
{
	struct nkfs_inode *inode_nkfs =
		container_of(inode, struct nkfs_inode, vfs_inode);

	kmem_cache_free(nkfs_inode_cachep, inode_nkfs);
}

int nkfs_write_inode(struct inode *inode, struct writeback_control *wbc)
{
	return 0;
}

void nkfs_evict_inode(struct inode *inode)
{
	truncate_inode_pages_final(&inode->i_data);
	clear_inode(inode);
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

struct inode *nkfs_iget(struct super_block *sb, unsigned long ino)
{
	struct inode *inode;
	struct nkfs_disk_inode *disk_inode;
	struct buffer_head *bh;
	struct nkfs_disk_super_block *nkfs_sbd;
	struct nkfs_inode *inode_nkfs;

	nkfs_sbd = NKFS_DISK_SB(sb);
	if (ino >= le32_to_cpu(nkfs_sbd->inode_count))
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
	inode_set_atime(inode, le32_to_cpu(disk_inode->atime), 0);
	inode_set_mtime(inode, le32_to_cpu(disk_inode->mtime), 0);
	inode_set_ctime(inode, le32_to_cpu(disk_inode->ctime), 0);

	inode_nkfs = container_of(inode, struct nkfs_inode, vfs_inode);
	inode_nkfs->block_no = le32_to_cpu(disk_inode->block_no);

	if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &nkfs_dir_inode_ops;
		inode->i_fop = &nkfs_dir_inode_fops;
	} else {
		inode->i_op = &nkfs_file_inode_ops;
	}

	brelse(bh);
	unlock_new_inode(inode);
	
	return inode;
}

struct dentry *nkfs_lookup(struct inode *dir, struct dentry *dentry,
                                  unsigned int flags)
{
	struct super_block *sb = dir->i_sb;
	struct nkfs_inode *inode_nkfs =
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
		if (strncmp(de[i].name, dentry->d_name.name, NKFS_FILENAME_MAXLEN) == 0) {
			inode = nkfs_iget(sb, le32_to_cpu(de[i].inode));
			if (IS_ERR(inode))
                {
                    brelse(bh);
                    return (struct dentry *)inode;
                }
			break;
		}
	}

	brelse(bh);

	return d_splice_alias(inode, dentry);
}

/* Rudimentary new_inode implementation that finds a free inode. 
   Does NOT link it to directory yet (mkdir responsibility, partially). */
struct inode *nkfs_new_inode(struct inode *dir, umode_t mode)
{
    struct inode *inode;
    struct super_block *sb = dir->i_sb;
	struct nkfs_super_block *nkfs_sb = NKFS_SB(sb);
    int ino;

    spin_lock(&nkfs_sb->slock);
    ino = find_next_zero_bit(nkfs_sb->inode_bitmap, le32_to_cpu(nkfs_sb->sbd->inode_count), 0);
    // Note: Bitmap is 0-indexed, but inode numbers usually start at 1. NKFS_ROOT_INO is 1. 0 is reserved.
    
    if (ino >= le32_to_cpu(nkfs_sb->sbd->inode_count)) {
        spin_unlock(&nkfs_sb->slock);
        return ERR_PTR(-ENOSPC);
    }
    
    // Simple improvement: Skip inode 0
    if (ino == 0) {
          ino = find_next_zero_bit(nkfs_sb->inode_bitmap, le32_to_cpu(nkfs_sb->sbd->inode_count), 1);
          if (ino >= le32_to_cpu(nkfs_sb->sbd->inode_count)) {
            spin_unlock(&nkfs_sb->slock);
            return ERR_PTR(-ENOSPC);
          }
    }

    __set_bit(ino, nkfs_sb->inode_bitmap);
    // In a real FS, we should decrement free_inodes count and mark sb dirty
    spin_unlock(&nkfs_sb->slock);
    
    inode = new_inode(sb);
    if (!inode) return ERR_PTR(-ENOMEM);
    
    inode_init_owner(&nop_mnt_idmap, inode, dir, mode);
    inode->i_ino = ino;
    inode->i_blocks = 0;
    simple_inode_init_ts(inode);
    
    if (S_ISDIR(mode)) {
		inode->i_op = &nkfs_dir_inode_ops;
		inode->i_fop = &nkfs_dir_inode_fops;
        inc_nlink(inode); // "."
    } else {
		inode->i_op = &nkfs_file_inode_ops;
    }

    insert_inode_hash(inode);
    mark_inode_dirty(inode);
    
    return inode;
}

struct dentry *nkfs_mkdir(struct mnt_idmap *idmap, struct inode *dir, struct dentry *dentry, umode_t mode)
{
	struct inode *inode;
    struct nkfs_inode *dir_nkfs = container_of(dir, struct nkfs_inode, vfs_inode);
    struct buffer_head *bh;
    struct nkfs_disk_dir_entry *de;
    int i;


	inode = nkfs_new_inode(dir, mode | S_IFDIR);
    if (IS_ERR(inode)) return ERR_CAST(inode);
    
    inode->i_op = &nkfs_dir_inode_ops;
    inode->i_fop = &nkfs_dir_inode_fops;
    
    /* Add entry to parent directory */
    // Note: blocking read of parent directory block.
	if ((bh = sb_bread(dir->i_sb, dir_nkfs->block_no)) == NULL) {
        // Cleanup new inode?
        // iput(inode); // or discard
		return ERR_PTR(-EIO);
    }
    
    de = (struct nkfs_disk_dir_entry *)bh->b_data;
    for (i = 0; i < NKFS_MAX_DIR_ENTRIES; ++i) {
        if (de[i].inode == 0) {
            de[i].inode = cpu_to_le32(inode->i_ino);
            strncpy(de[i].name, dentry->d_name.name, NKFS_FILENAME_MAXLEN);
            mark_buffer_dirty(bh);
            brelse(bh);
            
            d_instantiate(dentry, inode);
            inc_nlink(dir); // ".."
            return NULL;
        }
    }
    
    brelse(bh);
    /* No space in directory block */
    // Simple FS limitation: only 1 block for dir.
    // iput(inode);
    return ERR_PTR(-ENOSPC); 
}

int nkfs_iterate_shared(struct file *file, struct dir_context *ctx)
{
    struct inode *inode = file_inode(file);
    struct nkfs_inode *inode_nkfs = container_of(inode, struct nkfs_inode, vfs_inode);
    struct buffer_head *bh;
    struct nkfs_disk_dir_entry *de;
    int i;
    
    if (ctx->pos >= NKFS_MAX_DIR_ENTRIES) return 0;
    
    if ((bh = sb_bread(inode->i_sb, inode_nkfs->block_no)) == NULL)
        return -EIO;
        
    de = (struct nkfs_disk_dir_entry *)bh->b_data;
    for (i = ctx->pos; i < NKFS_MAX_DIR_ENTRIES; i++) {
        if (de[i].inode != 0) {
            if (!dir_emit(ctx, de[i].name, strnlen(de[i].name, NKFS_FILENAME_MAXLEN), 
                    le32_to_cpu(de[i].inode), DT_UNKNOWN)) { // Type unknown for simplicity
                break;
            }
        }
        ctx->pos++;
    }
    
    brelse(bh);
    return 0;
}

const struct inode_operations nkfs_dir_inode_ops = {
	.lookup = nkfs_lookup,
    .mkdir = nkfs_mkdir
};

const struct inode_operations nkfs_file_inode_ops = {
	NULL
};

const struct file_operations nkfs_dir_inode_fops = {
        .iterate_shared = nkfs_iterate_shared
    };
