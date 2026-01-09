#ifndef __SMP_H__
#define __SMP_H__

#include <stdint.h>

/* Per-CPU local storage - accessed via FS segment
 * Layout must match assembly expectations.
 */
typedef struct {
    uint64_t cpu_number;
    uint64_t lapic_id;
} cpu_local_t;

extern cpu_local_t cpu_locals[];
extern int cpu_count;

void cpu0_init(void);
void smp_init(void);
int get_cpu_number(void);

#endif
