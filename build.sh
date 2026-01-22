#!/bin/sh
set -e
. ./headers.sh

for PROJECT in $PROJECTS; do
  (cd $PROJECT && DESTDIR="$SYSROOT" $MAKE install)
done

# Gera a imagem
dd if=/dev/zero of=disk.img bs=512 count=2880

# Stage 1 (boot sector)
dd if=./boot/stage1.bin of=disk.img conv=notrunc

# Stage 2
dd if=./boot/stage2.bin of=disk.img seek=1 conv=notrunc

# Converte o kernel ELF para binário puro
i686-elf-objcopy -O binary sysroot/boot/myos.kernel sysroot/boot/kernel.bin

# Copia o binário puro para o disco
dd if=./sysroot/boot/kernel.bin of=disk.img seek=9 conv=notrunc