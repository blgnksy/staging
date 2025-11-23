#include <linux/fs.h>
/*
 * Create and initialize a new inode
 */
struct inode *nkfs_get_inode(struct super_block *sb,
                              const struct inode *dir,
                              umode_t mode,
                              dev_t dev)
{
    struct inode *inode = new_inode(sb);
    
    if (!inode)
        return NULL;
    
    // Set inode fields
    inode->i_ino = get_next_ino();  // Or your own inode allocator
    inode_init_owner(inode, dir, mode);
    inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
    
    // Set operations based on file type
    switch (mode & S_IFMT) {
    case S_IFDIR:
        inode->i_op = &nkfs_dir_inode_ops;
        inode->i_fop = &nkfs_dir_ops;
        inc_nlink(inode);  // . entry
        break;
    case S_IFREG:
        inode->i_op = &nkfs_file_inode_ops;
        inode->i_fop = &nkfs_file_ops;
        inode->i_mapping->a_ops = &nkfs_aops;
        break;
    case S_IFLNK:
        inode->i_op = &nkfs_symlink_inode_ops;
        break;
    default:
        init_special_inode(inode, mode, dev);
        break;
    }
    
    return inode;
}

/*
 * Read inode from disk
 */
struct inode *nkfs_iget(struct super_block *sb, unsigned long ino)
{
    struct inode *inode;
    // struct buffer_head *bh;
    // struct nkfs_disk_inode *disk_inode;
    
    inode = iget_locked(sb, ino);
    if (!inode)
        return ERR_PTR(-ENOMEM);
    
    if (!(inode->i_state & I_NEW))
        return inode;
    
    // Read inode from disk
    // bh = sb_bread(sb, inode_block_number);
    // disk_inode = (struct nkfs_disk_inode *)(bh->b_data + offset);
    
    // Fill inode structure from disk
    // inode->i_mode = le16_to_cpu(disk_inode->i_mode);
    // inode->i_uid = le32_to_cpu(disk_inode->i_uid);
    // inode->i_size = le64_to_cpu(disk_inode->i_size);
    // ...
    
    // Set operations
    if (S_ISREG(inode->i_mode)) {
        inode->i_op = &nkfs_file_inode_ops;
        inode->i_fop = &nkfs_file_ops;
        inode->i_mapping->a_ops = &nkfs_aops;
    } else if (S_ISDIR(inode->i_mode)) {
        inode->i_op = &nkfs_dir_inode_ops;
        inode->i_fop = &nkfs_dir_ops;
    }
    
    // brelse(bh);
    unlock_new_inode(inode);
    
    return inode;
}

// ============ Directory Inode Operations ============

/*
 * Lookup a filename in a directory
 */
static struct dentry *nkfs_lookup(struct inode *dir,
                                   struct dentry *dentry,
                                   unsigned int flags)
{
    struct inode *inode = NULL;
    // unsigned long ino;
    
    // Search directory for filename
    // ino = nkfs_find_entry(dir, &dentry->d_name);
    // if (ino) {
    //     inode = nkfs_iget(dir->i_sb, ino);
    //     if (IS_ERR(inode))
    //         return ERR_CAST(inode);
    // }
    
    // Associate dentry with inode (NULL for negative dentry)
    d_add(dentry, inode);
    return NULL;
}

/*
 * Create a regular file
 */
static int nkfs_create(struct inode *dir,
                        struct dentry *dentry,
                        umode_t mode,
                        bool excl)
{
    struct inode *inode;
    
    // Allocate new inode
    inode = nkfs_get_inode(dir->i_sb, dir, mode | S_IFREG, 0);
    if (!inode)
        return -ENOSPC;
    
    // Add directory entry
    // int err = nkfs_add_entry(dir, &dentry->d_name, inode->i_ino);
    // if (err) {
    //     iput(inode);
    //     return err;
    // }
    
    // Update directory timestamps
    dir->i_mtime = dir->i_ctime = current_time(dir);
    mark_inode_dirty(dir);
    
    // Associate dentry with new inode
    d_instantiate(dentry, inode);
    
    return 0;
}

/*
 * Create a directory
 */
static int nkfs_mkdir(struct inode *dir,
                       struct dentry *dentry,
                       umode_t mode)
{
    struct inode *inode;
    
    // Allocate new inode
    inode = nkfs_get_inode(dir->i_sb, dir, mode | S_IFDIR, 0);
    if (!inode)
        return -ENOSPC;
    
    // Add . and .. entries
    // nkfs_make_empty_dir(inode, dir);
    
    // Add directory entry in parent
    // int err = nkfs_add_entry(dir, &dentry->d_name, inode->i_ino);
    // if (err) {
    //     iput(inode);
    //     return err;
    // }
    
    inc_nlink(dir);  // For .. in new directory
    d_instantiate(dentry, inode);
    
    return 0;
}

/*
 * Remove a file
 */
static int nkfs_unlink(struct inode *dir, struct dentry *dentry)
{
    struct inode *inode = d_inode(dentry);
    
    // Remove directory entry
    // int err = nkfs_delete_entry(dir, &dentry->d_name);
    // if (err)
    //     return err;
    
    inode->i_ctime = dir->i_ctime = dir->i_mtime = current_time(inode);
    drop_nlink(inode);
    mark_inode_dirty(inode);
    mark_inode_dirty(dir);
    
    return 0;
}

/*
 * Remove a directory
 */
static int nkfs_rmdir(struct inode *dir, struct dentry *dentry)
{
    struct inode *inode = d_inode(dentry);
    
    // Check if directory is empty
    // if (!nkfs_empty_dir(inode))
    //     return -ENOTEMPTY;
    
    // Remove directory entry
    // int err = nkfs_delete_entry(dir, &dentry->d_name);
    // if (err)
    //     return err;
    
    drop_nlink(inode);  // For .
    drop_nlink(inode);  // Final reference
    drop_nlink(dir);    // For ..
    
    return 0;
}

/*
 * Rename a file or directory
 */
static int nkfs_rename(struct inode *old_dir, struct dentry *old_dentry,
                        struct inode *new_dir, struct dentry *new_dentry,
                        unsigned int flags)
{
    struct inode *old_inode = d_inode(old_dentry);
    struct inode *new_inode = d_inode(new_dentry);
    
    if (flags & ~RENAME_NOREPLACE)
        return -EINVAL;
    
    // Delete old entry
    // nkfs_delete_entry(old_dir, &old_dentry->d_name);
    
    // Add new entry
    // nkfs_add_entry(new_dir, &new_dentry->d_name, old_inode->i_ino);
    
    // If replacing existing file
    // if (new_inode) {
    //     drop_nlink(new_inode);
    // }
    
    // Update .. entry if moving directory
    // if (S_ISDIR(old_inode->i_mode) && old_dir != new_dir) {
    //     nkfs_set_link(old_inode, "..", new_dir->i_ino);
    //     drop_nlink(old_dir);
    //     inc_nlink(new_dir);
    // }
    
    return 0;
}

/*
 * Create a symbolic link
 */
static int nkfs_symlink(struct inode *dir,
                         struct dentry *dentry,
                         const char *symname)
{
    struct inode *inode;
    int len = strlen(symname) + 1;
    
    inode = nkfs_get_inode(dir->i_sb, dir, S_IFLNK | 0777, 0);
    if (!inode)
        return -ENOSPC;
    
    // Store symlink target
    // Either inline in inode or in a block
    // inode->i_link = kmalloc(len, GFP_KERNEL);
    // strcpy(inode->i_link, symname);
    
    inode->i_size = len - 1;
    
    // Add directory entry
    // nkfs_add_entry(dir, &dentry->d_name, inode->i_ino);
    
    d_instantiate(dentry, inode);
    return 0;
}

/*
 * Create a hard link
 */
static int nkfs_link(struct dentry *old_dentry,
                      struct inode *dir,
                      struct dentry *dentry)
{
    struct inode *inode = d_inode(old_dentry);
    
    // Add new directory entry pointing to same inode
    // nkfs_add_entry(dir, &dentry->d_name, inode->i_ino);
    
    inc_nlink(inode);
    inode->i_ctime = current_time(inode);
    mark_inode_dirty(inode);
    
    ihold(inode);
    d_instantiate(dentry, inode);
    
    return 0;
}

/*
 * Get attributes
 */
static int nkfs_getattr(const struct path *path, struct kstat *stat,
                         u32 request_mask, unsigned int flags)
{
    struct inode *inode = d_inode(path->dentry);
    
    generic_fillattr(inode, stat);
    
    return 0;
}

/*
 * Set attributes (chmod, chown, truncate, etc.)
 */
static int nkfs_setattr(struct dentry *dentry, struct iattr *iattr)
{
    struct inode *inode = d_inode(dentry);
    int error;
    
    error = setattr_prepare(dentry, iattr);
    if (error)
        return error;
    
    // Handle truncate
    if (iattr->ia_valid & ATTR_SIZE) {
        error = inode_newsize_ok(inode, iattr->ia_size);
        if (error)
            return error;
        
        truncate_setsize(inode, iattr->ia_size);
        // nkfs_truncate_blocks(inode, iattr->ia_size);
    }
    
    setattr_copy(inode, iattr);
    mark_inode_dirty(inode);
    
    return 0;
}

// Directory inode operations
static const struct inode_operations nkfs_dir_inode_ops = {
    .create     = nkfs_create,
    .lookup     = nkfs_lookup,
    .link       = nkfs_link,
    .unlink     = nkfs_unlink,
    .symlink    = nkfs_symlink,
    .mkdir      = nkfs_mkdir,
    .rmdir      = nkfs_rmdir,
    .rename     = nkfs_rename,
    .setattr    = nkfs_setattr,
    .getattr    = nkfs_getattr,
};

// File inode operations
static const struct inode_operations nkfs_file_inode_ops = {
    .setattr    = nkfs_setattr,
    .getattr    = nkfs_getattr,
};

// Symlink inode operations
static const struct inode_operations nkfs_symlink_inode_ops = {
    .get_link   = simple_get_link,  // Or implement nkfs_get_link
    .setattr    = nkfs_setattr,
    .getattr    = nkfs_getattr,
};