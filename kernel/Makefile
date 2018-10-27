DEBUG = off

CC = x86_64-elf-gcc

C_FILES = $(shell find src -type f -name '*.c')
H_FILES = $(shell find . -type f -name '*.h')
ASM_FILES = $(shell find asm -type f -name '*.asm') $(shell find startup -type f -name '*.asm')
REAL_FILES = $(shell find blobs -type f -name '*.real')
OBJ = $(C_FILES:.c=.o) $(ASM_FILES:.asm=.o)
BINS = $(REAL_FILES:.real=.bin)

CHARDFLAGS = -std=gnu99 -masm=intel \
    -mno-sse \
    -mno-sse2 \
    -mno-red-zone \
    -mcmodel=kernel \
    -nostdlib \
    -ffreestanding \
    -I./include/

ifeq ($(DEBUG), qemu)
CHARDFLAGS := $(CHARDFLAGS) -D_KERNEL_QEMU_OUTPUT_
else ifeq ($(DEBUG), vga)
CHARDFLAGS := $(CHARDFLAGS) -D_KERNEL_VGA_OUTPUT_
endif

CLINKFLAGS = -nostdlib \
    -T ./linker.ld

CFLAGS = -Wall -Wextra -O2

subleq.bin: $(BINS) $(OBJ) $(H_FILES)
	$(CC) $(OBJ) $(CLINKFLAGS) -o $@

%.o: %.c
	$(CC) $(CHARDFLAGS) $(CFLAGS) -c $< -o $@

%.bin: %.real
	nasm $< -f bin -o $@

%.o: %.asm
	nasm $< -f elf64 -o $@

clean:
	rm -f $(OBJ) $(BINS) subleq.bin
