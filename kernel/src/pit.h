#ifndef __PIT_H__
#define __PIT_H__

#include <stdint.h>

#define KRNL_PIT_FREQ 1000

void set_pit_freq(uint32_t frequency);
void sleep(uint64_t time);

#endif
