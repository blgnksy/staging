#!/bin/bash
set -e

# Create ISO directory structure
mkdir -p iso/boot/grub
cp bzImage iso/boot/
cp initrd.img iso/boot/

# Create GRUB config
cat > iso/boot/grub/grub.cfg << 'EOF'
set timeout=5
set default=0

menuentry "Custom Linux" {
    linux /boot/bzImage
    initrd /boot/initrd.img
}
EOF

# Generate the ISO
grub2-mkrescue -o customos.iso iso/

# Cleanup
rm -rf iso/

echo "Created customos.iso"

