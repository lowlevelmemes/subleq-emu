DEBUG = off

.PHONY: all clean run

all:
	$(MAKE) DEBUG=$(DEBUG) -C kernel
	$(MAKE) -C bootloader
	mv bootloader/bootloader.bin ./subleq.img

clean:
	$(MAKE) clean -C kernel
	$(MAKE) clean -C bootloader

run:
	qemu-system-x86_64 -net none -hda subleq.img -enable-kvm -cpu host -m 2G -smp 4 -debugcon stdio
