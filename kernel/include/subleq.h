#ifndef __SUBLEQ_H__
#define __SUBLEQ_H__

#include <stdint.h>

void subleq(void);
void zero_subleq_memory(void);
void subleq_acquire_mem(void);
uint64_t _readram(uint64_t);
void _writeram(uint64_t, uint64_t);
void _strcpyram(uint64_t, const char *);
void init_subleq(void);
void subleq_redraw_screen(void);

extern void *initramfs_addr;

#endif
