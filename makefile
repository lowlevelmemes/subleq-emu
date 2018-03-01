notarget: subleq.iso

clean:
	$(MAKE) clean -C kernel

kernel_target: kernel/initramfs
	$(MAKE) -C kernel

subleq.iso: kernel_target
	mkdir -p isodir/boot/grub
	cp kernel/subleq.bin isodir/boot/subleq.bin
	cp boot/grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o subleq.iso isodir
	rm -rf isodir
