#!/bin/sh
set -e
. ./headers.sh

for PROJECT in $PROJECTS; do
  (cd $PROJECT && DESTDIR="$SYSROOT" $MAKE install)
done

dd if=/dev/zero of=disk.img bs=512 count=2880 status=none

dd if=./boot/stage1.bin of=disk.img conv=notrunc bs=512 seek=0 status=none

dd if=./boot/stage2.bin of=disk.img conv=notrunc bs=512 seek=1 status=none

i686-elf-objcopy -O binary sysroot/boot/myos.kernel sysroot/boot/kernel.bin

dd if=./sysroot/boot/kernel.bin of=disk.img conv=notrunc bs=512 seek=9 status=none

echo "disk.img created!"