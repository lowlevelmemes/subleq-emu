#ifndef __CPU_H__
#define __CPU_H__

#include <stdint.h>

/* Port I/O functions */

static inline void port_out_b(uint16_t port, uint8_t value) {
    asm volatile ("outb %%al, %1"
                  :
                  : "a" (value), "Nd" (port)
                  : "memory");
}

static inline void port_out_w(uint16_t port, uint16_t value) {
    asm volatile ("outw %%ax, %1"
                  :
                  : "a" (value), "Nd" (port)
                  : "memory");
}

static inline void port_out_d(uint16_t port, uint32_t value) {
    asm volatile ("outl %%eax, %1"
                  :
                  : "a" (value), "Nd" (port)
                  : "memory");
}

static inline uint8_t port_in_b(uint16_t port) {
    uint8_t value;
    asm volatile ("inb %1, %%al"
                  : "=a" (value)
                  : "Nd" (port)
                  : "memory");
    return value;
}

static inline uint16_t port_in_w(uint16_t port) {
    uint16_t value;
    asm volatile ("inw %1, %%ax"
                  : "=a" (value)
                  : "Nd" (port)
                  : "memory");
    return value;
}

static inline uint32_t port_in_d(uint16_t port) {
    uint32_t value;
    asm volatile ("inl %1, %%eax"
                  : "=a" (value)
                  : "Nd" (port)
                  : "memory");
    return value;
}

/* MSR access */
static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    asm volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t value) {
    asm volatile ("wrmsr" :: "c"(msr), "a"((uint32_t)value), "d"((uint32_t)(value >> 32)));
}

/* CPU initialization */
void enable_simd(void);

#endif
