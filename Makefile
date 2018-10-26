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
