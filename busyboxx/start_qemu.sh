#!/bin/bash

qemu-system-x86_64 -m 4096 -smp 6  -kernel bzImage -initrd initrd.img \
 -s -S #\
# -append "console=ttyS0 nokaslr" -nographic
