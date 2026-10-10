#ifndef __TIME_H__
#define __TIME_H__

#include <stdint.h>

#define MOUSE_UPDATE_FREQ 50
#define SCREEN_REFRESH_FREQ 30

extern volatile uint64_t uptime_raw;

void timer_interrupt(uint64_t rip);
void timer_interrupt_ap(void);

#endif
