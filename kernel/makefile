CC = x86_64-elf-gcc
OBJCOPY = x86_64-elf-objcopy

C_FILES = $(shell find src -type f -name '*.c')
H_FILES = $(shell find . -type f -name '*.h')
ASM_FILES = $(shell find asm -type f -name '*.asm') $(shell find startup -type f -name '*.asm')
REAL_FILES = $(shell find blobs -type f -name '*.real')
OBJ = $(C_FILES:.c=.o) $(ASM_FILES:.asm=.o)
BINS = $(REAL_FILES:.real=.bin)
INITRAMFS = initramfs

CHARDFLAGS = -std=gnu99 -masm=intel \
    -mno-sse \
    -mno-sse2 \
    -mno-red-zone \
    -mcmodel=large \
    -nostdlib \
    -ffreestanding \
    -I./include/

CLINKFLAGS = -nostdlib \
    -static-libgcc -lgcc \
    -T ./linker.ld

CFLAGS = -Wall -O2

subleq.bin: initramfs_obj.o $(BINS) $(OBJ) $(H_FILES)
	$(CC) $(OBJ) initramfs_obj.o $(CLINKFLAGS) -o $@

initramfs_obj.o: $(INITRAMFS)
	$(OBJCOPY) -B i8086 -v -I binary -O elf64-x86-64 --rename-section .data=.initramfs $(INITRAMFS) $@

%.o: %.c
	$(CC) $(CHARDFLAGS) $(CFLAGS) -c $< -o $@

%.bin: %.real
	nasm $< -f bin -o $@

%.o: %.asm
	nasm $< -f elf64 -o $@

clean:
	rm -f $(OBJ) initramfs_obj.o $(BINS) subleq.bin
