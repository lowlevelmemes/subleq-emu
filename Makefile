DEBUG = off

.PHONY: notarget kernel_target clean

notarget: subleq.img

clean:
	$(MAKE) clean -C kernel
	$(MAKE) clean -C bootloader

kernel_target:
	$(MAKE) DEBUG=$(DEBUG) -C kernel

subleq.img: kernel_target
	$(MAKE) -C bootloader
	mv bootloader/bootloader.bin ./subleq.img

run:
	qemu-system-x86_64 -net none -hda subleq.img -enable-kvm -cpu host -m 2G -smp 4 -debugcon stdio
