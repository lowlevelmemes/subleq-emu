#ifndef __DAWN_H__
#define __DAWN_H__

#include <stdint.h>
#include <paging.h>

/*
 * Dawn OS Memory-Mapped I/O Locations
 * These are virtual addresses in Dawn's address space (dawn_pagemap)
 */

/* Dawn image is mapped starting at virtual address 0 */
#define DAWN_IMAGE_BASE     0
#define DAWN_IMAGE_SIZE     (256 * 1024 * 1024)

/* Display info block */
#define DAWN_DISPLAY_BASE   335540096   /* 0x13FFEF80 */
#define DAWN_DISPLAY_WIDTH  (DAWN_DISPLAY_BASE + 0)
#define DAWN_DISPLAY_HEIGHT (DAWN_DISPLAY_BASE + 8)
#define DAWN_DISPLAY_BPP    (DAWN_DISPLAY_BASE + 16)
#define DAWN_DISPLAY_FLAGS  (DAWN_DISPLAY_BASE + 24)
#define DAWN_FRAME_COUNTER  (DAWN_DISPLAY_BASE + 32)

/* Mouse buffer - 10 qwords */
#define DAWN_MOUSE_BASE     335542176   /* 0x13FFF7A0 */
#define DAWN_MOUSE_CLICK_L  (DAWN_MOUSE_BASE + 0 * 8)
#define DAWN_MOUSE_CLICK_R  (DAWN_MOUSE_BASE + 1 * 8)
#define DAWN_MOUSE_CLICK_M  (DAWN_MOUSE_BASE + 2 * 8)
#define DAWN_MOUSE_REL_X    (DAWN_MOUSE_BASE + 3 * 8)
#define DAWN_MOUSE_REL_Y    (DAWN_MOUSE_BASE + 4 * 8)
#define DAWN_MOUSE_RESERVED (DAWN_MOUSE_BASE + 5 * 8)
#define DAWN_MOUSE_ABS_X    (DAWN_MOUSE_BASE + 6 * 8)
#define DAWN_MOUSE_ABS_Y    (DAWN_MOUSE_BASE + 7 * 8)
#define DAWN_MOUSE_TOUCH    (DAWN_MOUSE_BASE + 8 * 8)
#define DAWN_MOUSE_SCROLL   (DAWN_MOUSE_BASE + 9 * 8)

/* Keyboard buffer */
#define DAWN_KEYBOARD_BASE  335542256   /* 0x13FFF7F0 */

/* Real-time clock (seconds since Dawn epoch in 32.32 fixed point) */
#define DAWN_RTC            335544304   /* 0x13FFFFF0 */

/* CPU info */
#define DAWN_CPU_NAME       335413288   /* 0x13FE0028 - 48-byte CPU name string */
#define DAWN_CPU_BANK_BASE  334364672   /* 0x13EE0000 - per-CPU status/EIP */

/* Fixed-point constants for Dawn's 32.32 format */
#define DAWN_FIXED_ONE      0x100000000ULL

/* Dawn state */
extern int dawn_ready;

/* CPU feature flags (detected by dawn_init, used by subleq.asm) */
extern uint64_t has_movbe;
extern uint64_t has_avx2;
extern uint64_t has_avx512;

/* Dawn initialization and runtime */
void dawn_init(void);
void dawn_redraw_screen(void);
void dawn_io_write(uint64_t io_loc, uint64_t value);
void dawn_io_flush(void);

/* Big-endian memory access for Dawn's address space */
uint64_t dawn_readram(uint64_t addr);
void dawn_writeram(uint64_t addr, uint64_t value);

#endif
