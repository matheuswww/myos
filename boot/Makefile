install: stage_1 stage_2

stage_1:
	nasm -f bin stage1.asm -o stage1.bin

stage_2: stage_1
	nasm -f bin stage2.asm -o stage2.bin

.PHONY: clean

clean:
	rm -f stage1.bin stage2.bin
