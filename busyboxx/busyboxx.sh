#!/bin/bash

cd busybox-1.37.0 || exit
make defconfig
sed 's/# CONFIG_STATIC is not set/CONFIG_STATIC=y/g' -i .config
sed 's/CONFIG_TC=y/CONFIG_TC=n/' -i .config
LDFLAGS="--static" make -j$(nproc) busybox || exit
cd ..

cp ../ramdisk.img initrd.img && \
cp ../vmlinux . && \
cp ../arch/x86_64/boot/bzImage .

mkdir -p initrd
cd initrd || exit
mkdir -p bin dev proc sys root
cd root || exit
echo '' > monitor_test.txt
cd ..
cd bin || exit
cp ../../busybox-1.37.0/busybox .
chmod +x busybox
for prog in $(./busybox --list);do
  ln -s ./busybox ./"$prog"
done

cd ..

echo '#!/bin/sh' > init
echo 'mount -t sysfs sysfs /sys' >> init
echo 'mount -t proc proc /proc' >> init
echo 'mount -t devtmpfs udev /dev' >> init
echo 'sysctl -w kernel.printk="2 4 1 7"' >> init
echo 'clear' >> init
echo '/bin/sh' >> init
echo 'poweroff -f' >> init


chmod -R 777 .
find . | cpio -o -H newc  > ../initrd.img
cd ..