#!/bin/sh
set -e
. ./headers.sh

for PROJECT in $PROJECTS; do
  (cd $PROJECT && DESTDIR="$SYSROOT" $MAKE install)
done

dd if=/dev/zero of=disk.img bs=1M count=512 status=none

i686-elf-objcopy -O binary sysroot/boot/myos.kernel sysroot/boot/kernel.bin

KERNEL_START=9
KERNEL_SIZE_MB=1
KERNEL_SECTORS=$(( (1024*1024) / 512 ))

dd if=boot/stage1.bin of=disk.img bs=512 seek=0 conv=notrunc status=none

dd if=boot/stage2.bin of=disk.img bs=512 seek=1 conv=notrunc status=none

dd if=sysroot/boot/kernel.bin of=disk.img \
   bs=512 seek=$KERNEL_START count=$KERNEL_SECTORS \
   conv=notrunc status=none

echo "disk.img RAW criado:"
echo "stage1  -> setor 0"
echo "stage2  -> setor 1"
echo "kernel  -> setores $KERNEL_START até $((KERNEL_START + KERNEL_SECTORS - 1))"