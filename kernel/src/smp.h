#ifndef __SMP_H__
#define __SMP_H__

#include <stdint.h>

/* Per-CPU local storage - accessed via FS segment
 * Layout must match assembly expectations:
 * - [fs:0]  = cpu_number (4 bytes)
 * - [fs:8]  = lapic_id (4 bytes)
 * - [fs:16] = poke_vector (8 bytes)
 */
typedef struct {
    int cpu_number;              /* offset 0 */
    int _pad0;                   /* offset 4 */
    uint32_t lapic_id;           /* offset 8 */
    uint32_t _pad1;              /* offset 12 */
    volatile uint64_t poke_vector;  /* offset 16 */
} __attribute__((packed)) cpu_local_t;

extern cpu_local_t cpu_locals[];
extern int cpu_count;

void cpu0_init(void);
void smp_init(void);
int get_cpu_number(void);

#endif
