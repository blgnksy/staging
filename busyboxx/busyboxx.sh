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
echo '# Minimal but complete PID 1 init for BusyBox-based systems' > init
echo '' > init
echo '###############################################################################' > init
echo '# 1. Environment setup' > init
echo '###############################################################################' > init
echo 'export PATH=/sbin:/bin' > init
echo 'export HOME=/root' > init
echo 'export TERM=linux' > init
echo '' > init
echo '###############################################################################' > init
echo '# 2. Mount essential virtual filesystems' > init
echo '###############################################################################' > init
echo 'mount -t proc  proc  /proc' > init
echo 'mount -t sysfs sysfs /sys' > init
echo '' > init
echo '# Device nodes' > init
echo 'if ! mount -t devtmpfs devtmpfs /dev; then' > init
echo '    mkdir -p /dev' > init
echo '    mknod /dev/console c 5 1' > init
echo '    mknod /dev/null    c 1 3' > init
echo 'fi' > init
echo '' > init
echo '###############################################################################' > init
echo '# 3. Console setup' > init
echo '###############################################################################' > init
echo 'exec </dev/console >/dev/console 2>&1' > init
echo '' > init
echo '###############################################################################' > init
echo '# 4. Kernel log verbosity (optional)' > init
echo '###############################################################################' > init
echo 'sysctl -w kernel.printk="2 4 1 7" >/dev/null 2>&1' > init
echo '' > init
echo '###############################################################################' > init
echo '# 5. Signal handling and zombie reaping' > init
echo '###############################################################################' > init
echo 'trap 'reap' SIGCHLD' > init
echo 'trap 'shutdown' SIGTERM SIGINT' > init
echo '' > init
echo 'reap() {' > init
echo '    while wait -n 2>/dev/null; do :; done' > init
echo '}' > init
echo '' > init
echo '###############################################################################' > init
echo '# 6. Optional: switch from initramfs to real root filesystem' > init
echo '###############################################################################' > init
echo '# Uncomment and adapt if you have a real rootfs' > init
echo '#' > init
echo '# mount /dev/sda1 /newroot || rescue_shell' > init
echo '# exec switch_root /newroot /sbin/init' > init
echo '' > init
echo '###############################################################################' > init
echo '# 7. Emergency rescue shell' > init
echo '###############################################################################' > init
echo 'rescue_shell() {' > init
echo '    echo "Entering emergency shell"' > init
echo '    /bin/sh' > init
echo '}' > init
echo '' > init
echo '###############################################################################' > init
echo '# 8. Shutdown handling' > init
echo '###############################################################################' > init
echo 'shutdown() {' > init
echo '    echo "System shutting down..."' > init
echo '    sync' > init
echo '    poweroff -f' > init
echo '}' > init
echo '' > init
echo '###############################################################################' > init
echo '# 9. Process supervision (interactive shell)' > init
echo '###############################################################################' > init
echo 'echo "System initialized. Starting shell."' > init
echo '' > init
echo 'while true; do' > init
echo '    /bin/sh' > init
echo '    reap' > init
echo '    echo "Shell exited. Restarting..."' > init
echo '    sleep 1' > init
echo 'done' > init
echo '' > init


chmod -R 777 .
find . | cpio -o -H newc  > ../initrd.img
cd ..