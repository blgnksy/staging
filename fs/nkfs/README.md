module init
register file system
mount
fill super block
    inode ops
    file ops
    super ops
    diskten super blogu oku
    kok dosya icin inode
    diskten inode oku

 



dd if=/dev/zero of=my.dat bs=512 count=4096

losetup /dev/loop0 my.dat 

mkdir -p /mnt/my
mkfs.nkfs /dev/loop0
mount -t nkfs /dev/loop0 /mnt/my