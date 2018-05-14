#ifndef __SMP_H__
#define __SMP_H__

void init_smp(void);

void *prepare_smp_trampoline(void *, void *, void *, void *, void *);
int check_ap_flag(void);
void init_cpu0_local(void *, void *);

extern int cpu_count;

#endif
