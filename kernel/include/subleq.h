#ifndef __SUBLEQ_H__
#define __SUBLEQ_H__

#include <stdint.h>

void subleq(void);
void subleq_io_flush(void);
void subleq_io_write(uint64_t, uint64_t);
uint64_t _readram(uint64_t);
void _writeram(uint64_t, uint64_t);
void init_subleq(void);
void subleq_redraw_screen(void);

extern int subleq_ready;

#endif
