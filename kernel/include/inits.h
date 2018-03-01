#ifndef __INITS_H__
#define __INITS_H__

#include <kernel.h>


#ifdef _SERIAL_KERNEL_OUTPUT_
  void debug_kernel_console_init(void);
#endif

void init_tty(void);
void init_acpi(void);
void init_apic(void);
void init_cpu0(void);

// driver inits

void init_pcspk(void);
void init_tty_drv(void);
void init_streams(void);
void init_com(void);
void init_stty(void);
void init_graphics(void);
void init_initramfs(void);
void init_fb(void);
void init_ata(void);

void keyboard_init(void);

// end driver inits
// fs inits

void install_devfs(void);
void install_echfs(void);

// end fs inits




#endif
