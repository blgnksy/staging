#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/writeback.h>
#include <linux/minmax.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kaan Aslan");
MODULE_DESCRIPTION("simplefs");

#define NKFS_BLOCK_SIZE         		4096
#define NKFS_MAGIC           			0x53494D46
#define NKFS_INODE_BITMAP_LOCATION		1
#define NKFS_DATA_BITMAP_LOCATION		2
#define NKFS_INODE_TABLE_LOCATION		3
#define NKFS_ROOT_INO					1
#define NKFS_FILENAME_MAXLEN    		32
        
struct simplefs_disk_super_block {
    __le32 magic;              /* 0x464D4953 ("SIMF") */
    __le32 block_size;         /* 4096 */
    __le32 inode_count;        /* Total inodes */
    __le32 block_count;        /* Total blocks */
    __le32 free_inodes;        /* Free inodes */
    __le32 free_blocks;        /* Free blocks */
    __le32 inode_table_block;  /* Start of the inode table (3) */
    __le32 inode_table_size;   /* Size of the inode table */
    __le32 data_block_start;   /* Start of data blocks */
    __u8   padding[4060];      /* Padding to 4096 bytes */
};

struct simplefs_super_block {
    struct simplefs_disk_super_block *sbd;
    struct buffer_head *sb_bh;
    struct buffer_head *inode_bitmap_bh;
    struct buffer_head *data_bitmap_bh;
    unsigned long *inode_bitmap;
    unsigned long *data_bitmap;
    spinlock_t lock;
};

struct simplefs_inode {
    __u32 block_no;
    struct inode vfs_inode;
};

struct simplefs_disk_inode {
    __le32 mode;            /* File type + permissions */
    __le32 uid;             /* Owner user ID */
    __le32 gid;             /* Owner group ID */
    __le32 size;            /* File size in bytes */
    __le32 nlink;           /* Hard link count */
    __le32 blocks;          /* Block count (0 or 1) */
    __le32 block_no;        /* Data block number */
    __le32 ctime;           /* Creation time */
    __le32 mtime;           /* Modification time */
    __le32 atime;           /* Access time */
    __u8   padding[24];     /* Padding to 64 bytes */
};

struct simplefs_disk_dentry {
    __le32 inode;                           /* İnode number */
    char name[NKFS_FILENAME_MAXLEN];    /* File name */
};

#define NKFS_DISK_DENTRY_SIZE			sizeof(struct simplefs_disk_dentry)
#define NKFS_MAX_DENTRIES				(NKFS_BLOCK_SIZE / NKFS_DISK_DENTRY_SIZE)

#define NKFS_SB(sb)						((struct simplefs_super_block *)(((sb)->s_fs_info)))
#define NKFS_DISK_SB(sb)				(NKFS_SB(sb)->sbd)

#define NKFS_DISK_INODE_SIZE			sizeof(struct simplefs_disk_inode)
#define NKFS_DISK_INODE_PER_BLOCK     	(NKFS_BLOCKSIZE / NKFS_DISK_INODE_SIZE)

static struct dentry *simplefs_mount(struct file_system_type *type, int flags, const char *dev, void *data);
static int simplefs_fill_super(struct super_block *sb, void *data, int silent);
static void simplefs_kill_sb(struct super_block *sb);
static struct inode *simplefs_alloc_inode(struct super_block *sb);
static void simplefs_free_inode(struct inode *inode);
static int simplefs_write_inode(struct inode *inode, struct writeback_control *wbc);
static void simplefs_evict_inode(struct inode *inode);
static struct inode *simplefs_iget(struct super_block *sb, unsigned long ino);
static struct simplefs_disk_inode *simplefs_get_inode_disk(struct super_block *sb, 
        unsigned long ino, struct buffer_head **bhp);
static struct dentry *simplefs_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags);
static int simplefs_iterate_shared (struct file *file, struct dir_context *ctx);
static int simplefs_mkdir(struct mnt_idmap *idmap, struct inode *dir, struct dentry *dentry, umode_t mode);
static int simplefs_alloc_inode_num(struct super_block *sb);
static struct inode *simplefs_new_inode(struct inode *dir, umode_t mode);
static void simplefs_free_inode_num(struct super_block *sb, int ino);
static int simplefs_alloc_data_block(struct super_block *sb);
static void simplefs_free_data_block(struct super_block *sb, int block);
static int simplefs_add_entry(struct inode *dir, struct dentry *dentry, struct inode *inode);
static int simplefs_rmdir(struct inode *dir, struct dentry *dentry);
static int simplefs_remove_entry(struct inode *dir, struct dentry *dentry);
static int simplefs_create(struct mnt_idmap *idmap, struct inode *dir, struct dentry *dentry, umode_t mode, bool excl);
static int simplefs_unlink(struct inode *dir, struct dentry *dentry);
static ssize_t simplefs_read(struct file *filp, char *buf, size_t size, loff_t *off);
static ssize_t simplefs_read(struct file *filp, char *buf, size_t size, loff_t *off);
static ssize_t simplefs_write(struct file *filp, const char *buf, size_t size, loff_t *off);
static loff_t simplefs_llseek(struct file *filp, loff_t off, int whence);

static struct file_system_type simplefs_type = {
    .owner = THIS_MODULE,
    .name = "simplefs",
    .mount = simplefs_mount,
    .kill_sb = simplefs_kill_sb,
    .fs_flags = FS_REQUIRES_DEV,
};

static const struct super_operations simplefs_super_ops = {
    .alloc_inode = simplefs_alloc_inode,
    .free_inode = simplefs_free_inode,
    .write_inode = simplefs_write_inode,
    .evict_inode = simplefs_evict_inode,
    .statfs = simple_statfs,
};

static const struct inode_operations simplefs_dir_inode_ops = {
    .lookup = simplefs_lookup,
    .mkdir = simplefs_mkdir,
    .rmdir = simplefs_rmdir,
    .create = simplefs_create,
    .unlink = simplefs_unlink,
};    

static const struct inode_operations simplefs_file_inode_ops = {
    .setattr = simple_setattr,
    .getattr = simple_getattr,
};

static const struct file_operations simplefs_dir_inode_fops = {
    .iterate_shared = simplefs_iterate_shared
};

static const struct file_operations simplefs_file_inode_fops = {        
    .owner = THIS_MODULE,
    .read = simplefs_read, 
    .write = simplefs_write,
    .llseek = simplefs_llseek,
};

static struct kmem_cache *simplefs_inode_cachep;

static int __init simplefs_module_init(void)
{
    int result;

    if ((simplefs_inode_cachep = kmem_cache_create("simplefs_inode_cache", sizeof(struct simplefs_inode),
            0, SLAB_HWCACHE_ALIGN, NULL)) == NULL) {
        printk(KERN_ERR "cannot allocate slab cache\n");
        return -ENOMEM;
    }

    if ((result = register_filesystem(&simplefs_type)) != 0) {
        printk(KERN_ERR "cannot register file system\n");
        goto EXIT;
    }

    printk(KERN_INFO "simplefs module init\n");

    return 0;
EXIT:
    kmem_cache_destroy(simplefs_inode_cachep);

    return result;
}

static void __exit simplefs_module_exit(void)
{
    unregister_filesystem(&simplefs_type);
    kmem_cache_destroy(simplefs_inode_cachep);

    printk(KERN_INFO "simplefs module exit\n");
}

static struct dentry *simplefs_mount(struct file_system_type *type, int flags, const char *dev, void *data)
{
    return mount_bdev(type, flags, dev, data, simplefs_fill_super);
}

static int simplefs_fill_super(struct super_block *sb, void *data, int silent)
{
    struct simplefs_super_block *sfs_sb;
    struct simplefs_disk_super_block *sbd;
    struct inode *root_inode;
    int result;
    
    sb->s_magic = NKFS_MAGIC;
    sb_set_blocksize(sb, NKFS_BLOCK_SIZE);
    sb->s_maxbytes = NKFS_BLOCK_SIZE;
    sb->s_op = &simplefs_super_ops;
    sb->s_flags |= SB_NOATIME;

    if ((sfs_sb = kzalloc(sizeof(struct simplefs_super_block), GFP_KERNEL))== NULL) {
        printk(KERN_INFO "cannot allocate simplefs super block!..\n");
        return -ENOMEM;
    }
    sb->s_fs_info = sfs_sb;
    spin_lock_init(&sfs_sb->lock);

    if ((sfs_sb->sb_bh = sb_bread(sb, 0)) == NULL) {
        printk(KERN_INFO "cannot read  simplefs disk super block!..\n");
        result = -EINVAL;
        goto EXIT1;
    }

    sbd = (struct simplefs_disk_super_block *) sfs_sb->sb_bh->b_data;
    sfs_sb->sbd = sbd;

    if (le32_to_cpu(sbd->magic) != NKFS_MAGIC) {
        printk(KERN_INFO "invalid magic number for simpls: %08X\n", sbd->magic);
        result = -EINVAL;
        goto EXIT2;
    }
    if (le32_to_cpu(sbd->block_size) != NKFS_BLOCK_SIZE) {
        printk(KERN_INFO "invalid block size for simpls: %08X\n", sbd->block_size);
        result = -EINVAL;
        goto EXIT2;
    }
    if (le32_to_cpu(sbd->inode_table_block) != 3) {
        printk(KERN_INFO "invalid inode table for simpls: %08X\n", sbd->inode_table_block);
        result = -EINVAL;
        goto EXIT2;
    }

    if ((sfs_sb->inode_bitmap_bh = sb_bread(sb, NKFS_INODE_BITMAP_LOCATION)) == NULL) {
        printk(KERN_INFO "cannot read simplefs inode bitmap!..\n");
        result = -EIO;
        goto EXIT2;
    }
    sfs_sb->inode_bitmap = (unsigned long *) sfs_sb->inode_bitmap_bh->b_data;

    if ((sfs_sb->data_bitmap_bh = sb_bread(sb, NKFS_DATA_BITMAP_LOCATION)) == NULL) {
        printk(KERN_INFO "cannot read simplefs data bitmap!..\n");
        result = -EINVAL;
        goto EXIT3;
    }
    sfs_sb->data_bitmap = (unsigned long *) sfs_sb->data_bitmap_bh->b_data;

    root_inode = simplefs_iget(sb, NKFS_ROOT_INO);
    if (IS_ERR(root_inode)) {
        printk(KERN_INFO "cannot read root inode!..\n");
        result = PTR_ERR(root_inode);
        goto EXIT4;
    }

    if ((sb->s_root = d_make_root(root_inode)) == NULL) {
        result = -ENOMEM;
        goto EXIT4;
    }

    return 0;

EXIT4:
    brelse(sfs_sb->data_bitmap_bh);
EXIT3:
    brelse(sfs_sb->inode_bitmap_bh);
EXIT2:
    brelse(sfs_sb->sb_bh);
EXIT1:
    kfree(sfs_sb);

    return result;
}

static void simplefs_kill_sb(struct super_block *sb)
{
   	struct simplefs_super_block *sfs_sb = sb->s_fs_info;

    kill_block_super(sb);
    
    if (sfs_sb) {
        if (sfs_sb->data_bitmap_bh)
            brelse(sfs_sb->data_bitmap_bh);
        if (sfs_sb->inode_bitmap_bh)
            brelse(sfs_sb->inode_bitmap_bh);
        if (sfs_sb->sb_bh)
            brelse(sfs_sb->sb_bh);
        kfree(sfs_sb);
    }
     
    printk(KERN_INFO "unmount super block...\n");
}

static struct inode *simplefs_alloc_inode(struct super_block *sb)
{
    struct simplefs_inode *inode_sfs;

    if ((inode_sfs = kmem_cache_alloc(simplefs_inode_cachep, GFP_KERNEL)) == NULL) 
        return NULL;
    inode_init_once(&inode_sfs->vfs_inode);

    return &inode_sfs->vfs_inode;
}

static void simplefs_free_inode(struct inode *inode) 
{
    struct simplefs_inode *inode_sfs = container_of(inode, struct simplefs_inode, vfs_inode);
    
    kmem_cache_free(simplefs_inode_cachep, inode_sfs);
}

static int simplefs_write_inode(struct inode *inode, struct writeback_control *wbc)
{
    struct simplefs_inode *inode_sfs;
    struct simplefs_disk_inode *disk_inode;
    struct buffer_head *bh;

    inode_sfs = container_of(inode, struct simplefs_inode, vfs_inode);
    disk_inode = simplefs_get_inode_disk(inode->i_sb, inode->i_ino, &bh);

    if (IS_ERR(disk_inode))
        return PTR_ERR(disk_inode);
    
    disk_inode->mode = cpu_to_le32(inode->i_mode);
    disk_inode->uid = cpu_to_le32(i_uid_read(inode));
    disk_inode->gid = cpu_to_le32(i_gid_read(inode));
    disk_inode->size = cpu_to_le32(inode->i_size);
    disk_inode->nlink = cpu_to_le32(inode->i_nlink);
    disk_inode->blocks = cpu_to_le32(inode->i_blocks);
    disk_inode->block_no = cpu_to_le32(inode_sfs->block_no);

    disk_inode->atime = cpu_to_le32(inode_get_atime(inode).tv_sec);
    disk_inode->mtime = cpu_to_le32(inode_get_mtime(inode).tv_sec);
    disk_inode->ctime = cpu_to_le32(inode_get_ctime(inode).tv_sec);
    
    mark_buffer_dirty(bh);
    if (wbc->sync_mode == WB_SYNC_ALL)
        sync_dirty_buffer(bh);
    brelse(bh);
    
    printk(KERN_INFO "simplfs: Wrote inode %lu to disk\n", inode->i_ino);
    
    return 0;
}

static void simplefs_evict_inode(struct inode *inode)
{
    struct simplefs_inode *inode_sfs;

   	inode_sfs = container_of(inode, struct simplefs_inode, vfs_inode);
    
    truncate_inode_pages_final(&inode->i_data);
    clear_inode(inode);
    
    if (inode->i_nlink)
        return;
    
    if (inode_sfs->block_no != 0)
        simplefs_free_data_block(inode->i_sb, inode_sfs->block_no);
    simplefs_free_inode_num(inode->i_sb, inode->i_ino);
    
    printk(KERN_INFO "simplfs: Evicted inode %lu\n", inode->i_ino);
}

static struct inode *simplefs_iget(struct super_block *sb, unsigned long ino)
{
    struct inode *inode;
    struct simplefs_disk_inode *disk_inode;
    struct buffer_head *bh;
    struct simplefs_disk_super_block *sfs_sbd;
    struct simplefs_inode *inode_sfs;

    sfs_sbd = NKFS_DISK_SB(sb);
    if (ino >= sfs_sbd->inode_count) 
        return ERR_PTR(-EINVAL);

    if ((inode = iget_locked(sb, ino)) == NULL) 
        return ERR_PTR(-ENOMEM);

     if (!(inode->i_state & I_NEW))
        return inode;

    disk_inode = simplefs_get_inode_disk(sb, ino, &bh);
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

    inode_sfs = container_of(inode, struct simplefs_inode, vfs_inode);
    inode_sfs->block_no = disk_inode->block_no;

    if (S_ISDIR(inode->i_mode)) {
        inode->i_op = &simplefs_dir_inode_ops;
        inode->i_fop = &simplefs_dir_inode_fops;
    }
    else {
        inode->i_op = &simplefs_file_inode_ops;
        inode->i_fop = &simplefs_file_inode_fops;
    }
    brelse(bh);
    unlock_new_inode(inode);
    
    return inode;
}

static struct simplefs_disk_inode *simplefs_get_inode_disk(struct super_block *sb, 
            unsigned long ino, struct buffer_head **bhpp)
{
    int block_no, block_offset;
    struct buffer_head *bh;
    struct simplefs_disk_inode *disk_inode;
    
    block_no = NKFS_INODE_TABLE_LOCATION + ino * NKFS_DISK_INODE_SIZE / NKFS_BLOCK_SIZE;
    block_offset = ino * NKFS_DISK_INODE_SIZE % NKFS_BLOCK_SIZE;

    if ((bh = sb_bread(sb, block_no)) == NULL) 
        return ERR_PTR(-EIO);

    disk_inode = (struct simplefs_disk_inode *)(bh->b_data + block_offset);
    *bhpp = bh;

    return disk_inode;
}

static struct dentry *simplefs_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags)
{
    struct super_block *sb = dir->i_sb;
    struct simplefs_inode *inode_sfs = container_of(dir, struct simplefs_inode, vfs_inode);
    struct buffer_head *bh;
    struct simplefs_disk_dentry *de;
    struct inode *inode = NULL;
    int i;
    
    if (dentry->d_name.len > NKFS_FILENAME_MAXLEN - 1)
        return ERR_PTR(-ENAMETOOLONG);
    
    if ((bh = sb_bread(sb, inode_sfs->block_no)) == NULL)
        return ERR_PTR(-EIO);
    
    de = (struct simplefs_disk_dentry *)bh->b_data;
    for (i = 0; i < NKFS_MAX_DENTRIES; ++i) {
        if (de[i].inode == 0)			/* deleted entry */
            continue;
        if (strcmp(de[i].name, dentry->d_name.name) == 0) {
            inode = simplefs_iget(sb, le32_to_cpu(de[i].inode));
            if (IS_ERR(inode)) {
                brelse(bh);			
                return (struct dentry *)inode;
            }
            break;
        }
    }
    
    brelse(bh);

    return d_splice_alias(inode, dentry);
}

static int simplefs_iterate_shared(struct file *file, struct dir_context *ctx)
{
    struct inode *inode;
    struct simplefs_inode *inode_sfs;
    struct buffer_head *bh;
    struct simplefs_disk_dentry *de;
    unsigned long ino;
    int i;

    inode = file_inode(file);
    inode_sfs = container_of(inode, struct simplefs_inode, vfs_inode);

    if (ctx->pos >= NKFS_MAX_DENTRIES) 
        return 0;

    if (inode_sfs->block_no == 0) {
        printk(KERN_INFO "unallocated disk block for directory!..\n");
        return 0;
    }

    if ((bh = sb_bread(inode->i_sb, inode_sfs->block_no)) == NULL) {
        printk(KERN_INFO "cannot read directory block from disk!..\n")  ;
        return -EIO;
    }
    de = (struct simplefs_disk_dentry *)bh->b_data;

    for (i = ctx->pos; i < NKFS_MAX_DENTRIES; ++i) {
        if (de[i].inode == 0) {
            ctx->pos = i + 1;
            continue;
        }
        ino = le32_to_cpu(de[i].inode);
        
        if (!dir_emit(ctx, de[i].name, strlen(de[i].name), ino, DT_UNKNOWN)) {
            brelse(bh);
            return 0;
        }
        
        ctx->pos = i + 1;
    }
    brelse(bh);

    return 0;
}

static int simplefs_mkdir(struct mnt_idmap *idmap, struct inode *dir, struct dentry *dentry, umode_t mode)
{
    struct inode *inode;
    struct buffer_head *bh;
    struct simplefs_disk_dentry *entry;
    struct simplefs_inode *inode_sfs;
    int block;
    int result;

    inode = simplefs_new_inode(dir, mode | S_IFDIR);
    if (IS_ERR(inode))
        return PTR_ERR(inode);
    
    if ((block = simplefs_alloc_data_block(dir->i_sb)) < 0) {
        result = block;
        goto EXIT1;
    }

    inode_sfs = container_of(inode, struct simplefs_inode, vfs_inode);
    inode_sfs->block_no = block;
    inode->i_blocks = 1;
    inode->i_size = NKFS_BLOCK_SIZE;

    inode->i_op = &simplefs_dir_inode_ops;
    inode->i_fop = &simplefs_dir_inode_fops;

    if ((bh = sb_bread(dir->i_sb, block)) == NULL) {
        result = -EIO;
        goto EXIT2;
    }
    entry = (struct simplefs_disk_dentry *)bh->b_data;

    entry[0].inode = cpu_to_le32(inode->i_ino);
    strcpy(entry[0].name, ".");

    entry[1].inode = cpu_to_le32(dir->i_ino);
    strcpy(entry[1].name, "..");

    set_nlink(inode, 2);
    mark_buffer_dirty(bh);
    brelse(bh);

    if ((result = simplefs_add_entry(dir, dentry, inode)) != 0)
        goto EXIT2;

    inode_inc_link_count(dir);
    mark_inode_dirty(inode);
    d_instantiate(dentry, inode);

    return 0;
    
EXIT2:
    simplefs_free_data_block(dir->i_sb, block);
EXIT1:
    set_nlink(inode, 0);
    iput(inode);

    return result;
}

static int simplefs_alloc_inode_num(struct super_block *sb)
{
    struct simplefs_super_block *sfs_sb;
    struct simplefs_disk_super_block *sfs_sbd;
    int ino;
    
    sfs_sb = NKFS_SB(sb);
    sfs_sbd = NKFS_DISK_SB(sb);

    spin_lock(&sfs_sb->lock);
    
    if (sfs_sbd->free_inodes == 0) {
        spin_unlock(&sfs_sb->lock);
        return -ENOSPC;
    }
    
    ino = find_first_zero_bit(sfs_sb->inode_bitmap, sfs_sbd->inode_count);
    if (ino >= sfs_sbd->inode_count) {
        spin_unlock(&sfs_sb->lock);
        return -ENOSPC;
    }
       
    set_bit(ino, sfs_sb->inode_bitmap);
    sfs_sbd->free_inodes--;
    mark_buffer_dirty(sfs_sb->inode_bitmap_bh);
    mark_buffer_dirty(sfs_sb->sb_bh);
    
    spin_unlock(&sfs_sb->lock);

    return ino;
}

static struct inode *simplefs_new_inode(struct inode *dir, umode_t mode)
{
    struct super_block *sb = dir->i_sb;
    struct inode *inode;
    int ino;
    
    ino = simplefs_alloc_inode_num(sb);
    if (ino < 0)
        return ERR_PTR(ino);
    
    if ((inode = new_inode(sb)) == NULL) {
        simplefs_free_inode_num(sb, ino);
        return ERR_PTR(-ENOMEM);
    }
    
    inode_init_owner(&nop_mnt_idmap, inode, dir, mode);
    inode->i_ino = ino;
    simple_inode_init_ts(inode);
    
    insert_inode_hash(inode);
    mark_inode_dirty(inode);
    
    printk(KERN_INFO "simplfs: Created new inode %lu\n", inode->i_ino);
    return inode;
}

static void simplefs_free_inode_num(struct super_block *sb, int ino)
{
    struct simplefs_super_block *sfs_sb; 
    struct simplefs_disk_super_block *sfs_sbd;
    
    sfs_sb = NKFS_SB(sb);
    sfs_sbd = NKFS_DISK_SB(sb);
    
    if (ino < 0 || ino >= sfs_sbd->inode_count) {
        printk(KERN_INFO "simplfs: Invalid inode number %d\n", ino);
        return;
    }
    
    spin_lock(&sfs_sb->lock);
    clear_bit(ino, sfs_sb->inode_bitmap);
    sfs_sbd->free_inodes++;
    mark_buffer_dirty(sfs_sb->inode_bitmap_bh);
    mark_buffer_dirty(sfs_sb->sb_bh);
    spin_unlock(&sfs_sb->lock);
    
    printk(KERN_INFO "simplfs: Freed inode %d (free: %u)\n", ino, sfs_sbd->free_inodes);
}

static int simplefs_alloc_data_block(struct super_block *sb)
{
    struct simplefs_super_block *sfs_sb;
    struct simplefs_disk_super_block *sfs_sbd;
    int bit;
    uint32_t max_data_blocks;
    
    sfs_sb = NKFS_SB(sb);
    sfs_sbd = NKFS_DISK_SB(sb);
    max_data_blocks = sfs_sbd->block_count - sfs_sbd->data_block_start;
    
    spin_lock(&sfs_sb->lock);
    
    if (sfs_sbd->free_blocks == 0) {
        spin_unlock(&sfs_sb->lock);
        printk(KERN_ERR "simplfs: No free data blocks\n");
        return -ENOSPC;
    }
    
    bit = find_first_zero_bit(sfs_sb->data_bitmap, max_data_blocks);
    if (bit >= max_data_blocks) {
        spin_unlock(&sfs_sb->lock);
        printk(KERN_ERR "simplfs: No free data block found in bitmap\n");
        return -ENOSPC;
    }
    
    set_bit(bit, sfs_sb->data_bitmap);
    sfs_sbd->free_blocks--;
    mark_buffer_dirty(sfs_sb->data_bitmap_bh);
    mark_buffer_dirty(sfs_sb->sb_bh);
    
    spin_unlock(&sfs_sb->lock);
    
    printk(KERN_INFO "simplfs: Allocated data block %d (free: %u)\n", 
            sfs_sbd->data_block_start + bit, sfs_sbd->free_blocks);
    
    return sfs_sbd->data_block_start + bit;
}

static void simplefs_free_data_block(struct super_block *sb, int block)
{
    struct simplefs_super_block *sfs_sb;
    struct simplefs_disk_super_block *sfs_sbd;
    int bit;
    
    sfs_sb = NKFS_SB(sb);
    sfs_sbd = NKFS_DISK_SB(sb);

    if (block < sfs_sbd->data_block_start || block >= sfs_sbd->block_count) {
        printk(KERN_INFO "simplfs: Invalid block number %d\n", block);
        return;
    }
    bit = block - sfs_sbd->data_block_start;
    spin_lock(&sfs_sb->lock);
    clear_bit(bit, sfs_sb->data_bitmap);
    sfs_sbd->free_blocks++;
    mark_buffer_dirty(sfs_sb->data_bitmap_bh);
    mark_buffer_dirty(sfs_sb->sb_bh);
    spin_unlock(&sfs_sb->lock);
}

static int simplefs_add_entry(struct inode *dir, struct dentry *dentry, struct inode *inode)
{
    struct super_block *sb;
    struct simplefs_inode *inode_dir_sfs;
    struct buffer_head *bh;
    struct simplefs_disk_dentry *entry;
    int i, result;

    sb = dir->i_sb;
    inode_dir_sfs = container_of(dir, struct simplefs_inode, vfs_inode);
    
    if (strlen(dentry->d_name.name) >= NKFS_FILENAME_MAXLEN - 1)
        return -ENAMETOOLONG;
    
    if ((bh = sb_bread(sb, inode_dir_sfs->block_no)) == NULL)
        return -EIO;
    
    result = -ENOSPC;
    entry = (struct simplefs_disk_dentry *)bh->b_data;
    for (i = 0; i < NKFS_MAX_DENTRIES; ++i){
        if (entry[i].inode == 0) {
            entry[i].inode = cpu_to_le32(inode->i_ino);
            strncpy(entry[i].name, dentry->d_name.name, NKFS_FILENAME_MAXLEN);
            entry[i].name[NKFS_FILENAME_MAXLEN - 1] = '\0';
            mark_buffer_dirty(bh);
            dir->i_size = NKFS_BLOCK_SIZE;
            simple_inode_init_ts(dir);
            mark_inode_dirty(dir);
            result = 0;
            break;
        }
    }
    
    brelse(bh);

    return result;
}

static int simplefs_rmdir(struct inode *dir, struct dentry *dentry)
{
    struct inode *inode;
    struct simplefs_inode *inode_sfs;
    struct buffer_head *bh;
    struct simplefs_disk_dentry *entry;
    int i, empty_flag;

    inode = d_inode(dentry);
    inode_sfs = container_of(inode, struct simplefs_inode, vfs_inode);
        
    if ((bh = sb_bread(inode->i_sb, inode_sfs->block_no)) == NULL)
        return -EIO;
    
    empty_flag = 0;
    entry = (struct simplefs_disk_dentry *)bh->b_data;
    for (i = 2; i < NKFS_MAX_DENTRIES; i++) {
        if (entry[i].inode != 0) {
            empty_flag = 1;
            break;
        }
    }
    brelse(bh);
    
    if (empty_flag)
        return -ENOTEMPTY;

    if (simplefs_remove_entry(dir, dentry) != 0) 
        printk(KERN_ERR "cannot delete directory entry dentry %s\n", dentry->d_name.name)  ;
    inode_dec_link_count(dir);
    inode_dec_link_count(inode);
    inode_dec_link_count(inode);
    
    return 0;
}

static int simplefs_remove_entry(struct inode *dir, struct dentry *dentry)
{
    struct super_block *sb;
    struct simplefs_inode *inode_sfs_dir;
    struct buffer_head *bh;
    struct simplefs_disk_dentry *entry;
    int i, result;
    
    sb = dir->i_sb;
    inode_sfs_dir = container_of(dir, struct simplefs_inode, vfs_inode);
    if (!inode_sfs_dir->block_no)
        return -ENOENT;
    
    if ((bh = sb_bread(sb, inode_sfs_dir->block_no)) == NULL)
        return -EIO;
    
    result = -ENOENT;
    entry = (struct simplefs_disk_dentry *)bh->b_data;
    for (i = 0; i < NKFS_MAX_DENTRIES; i++) {
        if (entry[i].inode == 0)
            continue;
        if (strcmp(entry[i].name, dentry->d_name.name) == 0) {
            entry[i].inode = 0;
            mark_buffer_dirty(bh);
            simple_inode_init_ts(dir);
            mark_inode_dirty(dir);
            result = 0;
            break;
        }
    }
    
    brelse(bh);

    return result;
}

static int simplefs_create(struct mnt_idmap *idmap, struct inode *dir, 
        struct dentry *dentry, umode_t mode, bool excl)
{
    struct inode *inode;
    int result;

    inode = simplefs_new_inode(dir, mode | S_IFREG);
    if (IS_ERR(inode))
        return PTR_ERR(inode);

    if ((result = simplefs_add_entry(dir, dentry, inode)) != 0) {
        inode_dec_link_count(inode);
        iput(inode);
        return result;
    }

    inode->i_op = &simplefs_file_inode_ops;
    inode->i_fop = &simplefs_file_inode_fops;

    d_instantiate(dentry, inode);
    
    return 0;
}

static int simplefs_unlink(struct inode *dir, struct dentry *dentry)
{
    struct inode *inode;
    int result;
    
    if ((result = simplefs_remove_entry(dir, dentry)) != 0)
        return result;
    
    inode = d_inode(dentry);
    inode_dec_link_count(inode);

    return 0;
}

static ssize_t simplefs_read(struct file *filp, char *buf, size_t size, loff_t *off)
{
    struct inode *inode;
    struct simplefs_inode *inode_sfs;
    struct buffer_head *bh;
    struct timespec64 now;
    size_t esize;

    inode = file_inode(filp);
    inode_sfs = container_of(inode, struct simplefs_inode, vfs_inode);

    if (*off >= inode->i_size)
        return 0;

    if (inode_sfs->block_no == 0) 
        return 0;

    if ((bh = sb_bread(inode->i_sb, inode_sfs->block_no)) == NULL) 
        return -EIO;
    
    esize = min_t(size_t, inode->i_size - *off, size);
    if (copy_to_user(buf, bh->b_data + *off, esize) != 0) {
        brelse(bh);
        return -EFAULT;
    }
    *off += esize;

    now = current_time(inode);
    inode_set_atime(inode, now.tv_sec, now.tv_nsec);

    mark_inode_dirty(inode);
    brelse(bh);

    return esize;
}

static ssize_t simplefs_write(struct file *filp, const char *buf, size_t size, loff_t *off)
{
    struct inode *inode;
    struct simplefs_inode *inode_sfs;
    int block;
    struct buffer_head *bh;
    size_t esize;
    struct timespec64 now;

    inode = file_inode(filp);
    inode_sfs = container_of(inode, struct simplefs_inode, vfs_inode);

    printk(KERN_INFO "write stats...\n");

    if (*off >= NKFS_BLOCK_SIZE)
        return -EFBIG;

    if (inode_sfs->block_no == 0) {
        if ((block = simplefs_alloc_data_block(inode->i_sb)) < 0) 
            return block;
        printk(KERN_INFO "New block allocated for file: %d\n", block);
        inode_sfs->block_no = block;
        inode->i_blocks = 1;
    }
    if ((bh = sb_bread(inode->i_sb, inode_sfs->block_no)) == NULL) {
        simplefs_free_data_block(inode->i_sb, block);
        return -EIO;
    }
    esize = min_t(size_t, NKFS_BLOCK_SIZE - *off, size);
    if (copy_from_user(bh->b_data + *off, buf, esize) != 0) {
        brelse(bh);
        simplefs_free_data_block(inode->i_sb, block);
        return -EFAULT;
    }

    *off += esize;
    if (*off > inode->i_size)
        inode->i_size = *off;

    now = current_time(inode);
    inode_set_mtime(inode, now.tv_sec, now.tv_nsec);
    inode_set_ctime(inode, now.tv_sec, now.tv_nsec);

    mark_buffer_dirty(bh);
    mark_inode_dirty(inode);
    brelse(bh);

    return esize;
}

static loff_t simplefs_llseek(struct file *filp, loff_t off, int whence)
{
    struct inode *inode;
    loff_t new_pos;

    inode = file_inode(filp);

    switch (whence) {
        case 0:
            new_pos = off;
            break;
        case 1:
            new_pos = filp->f_pos + off;
            break;
        case 2:
            new_pos = inode->i_size + off;
            break;
        default:
            return -EINVAL;
    }
    if (new_pos < 0 || new_pos > inode->i_size)
        return -EINVAL;
    filp->f_pos = new_pos;

    return new_pos;
}

module_init(simplefs_module_init);
module_exit(simplefs_module_exit);
        return -EINVAL;
    filp->f_pos = new_pos;

    return new_pos;
}

module_init(nkfs_module_init);
module_exit(nkfs_module_exit);