#ifndef __SYSTEM_H__
#define __SYSTEM_H__

#include <stdint.h>
#include <stddef.h>

typedef struct {
    int cpu_number;
    uint8_t *kernel_stack;
    int current_task;
    int idle_cpu;
    int ts_enable;
} cpu_local_t;

int get_cpu_number(void);
uint64_t get_cpu_kernel_stack(void);
int get_current_task(void);
void set_current_task(int);
int get_idle_cpu(void);
void set_idle_cpu(int);
int get_ts_enable(void);
void set_ts_enable(int);

void map_PIC(uint8_t PIC0Offset, uint8_t PIC1Offset);
void set_PIC0_mask(uint8_t mask);
void set_PIC1_mask(uint8_t mask);
uint8_t get_PIC0_mask(void);
uint8_t get_PIC1_mask(void);

void get_e820(void *);

extern size_t memory_size;
size_t detect_mem(void);

void set_pit_freq(uint32_t frequency);
void sleep(uint64_t time);

void load_IDT(void);

extern volatile uint64_t uptime_raw;
extern volatile uint64_t uptime_sec;

extern uint8_t fxstate[512];


#endif
