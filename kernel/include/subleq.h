#ifndef __SUBLEQ_H__
#define __SUBLEQ_H__

void subleq(void);
uint64_t _readram(uint64_t);
void _writeram(uint64_t, uint64_t);
void _strcpyram(uint64_t, const char *);
void init_subleq(void);
void subleq_redraw_screen(void);

extern void *initramfs_addr;

#endif
