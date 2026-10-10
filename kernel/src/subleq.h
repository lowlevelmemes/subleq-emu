#ifndef __SUBLEQ_H__
#define __SUBLEQ_H__

/*
 * SUBLEQ CPU loop (subleq.c) and interpreter (subleq.asm)
 */

#include <stdint.h>

/* Run this CPU as Dawn's CPU of the same number; never returns */
void subleq(void);

/* Run count instructions, a multiple of 4, from eip; returns the next eip */
uint64_t subleq_interp(uint64_t eip, uint64_t count);

#endif
