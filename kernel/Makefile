DEBUG = off

SYSTEM_CC_TARGET := $(shell gcc -dumpmachine | head -c 6)
ifeq ($(SYSTEM_CC_TARGET), x86_64)
CC = gcc
else
SYSTEM_CC_TARGET := $(shell x86_64-elf-gcc -dumpmachine | head -c 6)
ifeq ($(SYSTEM_CC_TARGET), x86_64)
CC = x86_64-elf-gcc
else
$(error No suitable x86_64 GCC compiler found, please install an x86_64-elf-gcc toolchain)
endif
endif

C_FILES = $(shell find src -type f -name '*.c')
H_FILES = $(shell find . -type f -name '*.h')
ASM_FILES = $(shell find asm -type f -name '*.asm') $(shell find startup -type f -name '*.asm')
REAL_FILES = $(shell find blobs -type f -name '*.real')
OBJ = $(C_FILES:.c=.o) $(ASM_FILES:.asm=.o)
BINS = $(REAL_FILES:.real=.bin)

CHARDFLAGS := \
    -std=gnu99 \
    -masm=intel \
    -fno-pic \
    -mno-80387 \
    -mno-sse \
    -mno-sse2 \
    -mno-red-zone \
    -mcmodel=kernel \
    -ffreestanding \
    -fno-stack-protector \
    -I./include/

ifeq ($(DEBUG), qemu)
CHARDFLAGS := $(CHARDFLAGS) -D_KERNEL_QEMU_OUTPUT_
else ifeq ($(DEBUG), vga)
CHARDFLAGS := $(CHARDFLAGS) -D_KERNEL_VGA_OUTPUT_
endif

CLINKFLAGS := \
    -nostdlib \
    -no-pie \
    -T ./linker.ld

CFLAGS = -Wall -Wextra -O2 -ffast-math -fno-signed-zeros

subleq.bin: $(BINS) $(OBJ) $(H_FILES)
	$(CC) $(OBJ) $(CLINKFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) $(CHARDFLAGS) -c $< -o $@

%.bin: %.real
	nasm $< -f bin -o $@

%.o: %.asm
	nasm $< -f elf64 -o $@

clean:
	rm -f $(OBJ) $(BINS) subleq.bin
