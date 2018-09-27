AS=nasm

.PHONY: all clean

all:
	$(AS) stage2.asm -f bin -o stage2.bin
	$(AS) bootloader.asm -f bin -o bootloader.bin

clean:
	rm -f bootloader.bin stage2.bin
