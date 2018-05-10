#include <kernel.h>
#include <stdint.h>
#include <klib.h>
#include <apic.h>
#include <acpi.h>
#include <system.h>
#include <paging.h>
#include <subleq.h>

#define CPU_STACK_SIZE 4096

void ap_kernel_entry(void) {
    /* APs jump here after initialisation */

    kprint(KPRN_INFO, "SMP: Started up AP #%u", get_cpu_number());
    kprint(KPRN_INFO, "SMP: AP #%u kernel stack top: %x", get_cpu_number(), get_cpu_kernel_stack());

    lapic_enable();

    asm volatile ("sti");

    subleq();

    return;
}

void init_cpu0_local(void *);

void init_cpu0(void) {
    /* allocate a new stack for CPU 0 */
    uint8_t *kernel_stack = kalloc(CPU_STACK_SIZE);
    kernel_stack += CPU_STACK_SIZE - 0x10;

    /* create CPU 0 local struct */
    cpu_local_t *cpu_local = kalloc(sizeof(cpu_local_t));
    cpu_local->cpu_number = 0;
    cpu_local->kernel_stack = kernel_stack;
    cpu_local->current_task = 0;
    cpu_local->idle_cpu = 1;

    init_cpu0_local(cpu_local);

    return;
}

static int start_ap(uint8_t, int);

void init_aps(void) {
    /* start up the APs and place them in a "sleep" state until further notice */

    for (size_t i = 1; i < local_apic_ptr; i++) {
        kprint(KPRN_INFO, "SMP: Starting up AP #%u", i);
        if (start_ap(local_apics[i]->apic_id, i)) {
            kprint(KPRN_ERR, "SMP: Failed to start AP #%u", i);
            continue;
        }
        /* wait a bit */
        sleep(10);
    }

    return;
}

void *prepare_smp_trampoline(void *, void *, uint8_t *, cpu_local_t *);
int check_ap_flag(void);

extern void *kernel_pagemap;

static int start_ap(uint8_t target_apic_id, int cpu_number) {
    /* allocate a new stack for the CPU */
    uint8_t *kernel_stack = kalloc(CPU_STACK_SIZE);
    kernel_stack += CPU_STACK_SIZE - 0x10;

    /* create CPU local struct */
    cpu_local_t *cpu_local = kalloc(sizeof(cpu_local_t));
    cpu_local->cpu_number = cpu_number;
    cpu_local->kernel_stack = kernel_stack;
    cpu_local->current_task = 0;
    cpu_local->idle_cpu = 1;

    void *trampoline = prepare_smp_trampoline(ap_kernel_entry, &kernel_pagemap, kernel_stack, cpu_local);

    /* Send the INIT IPI */
    lapic_write(APICREG_ICR1, ((uint32_t)target_apic_id) << 24);
    lapic_write(APICREG_ICR0, 0x4500);
    /* wait 10ms */
    sleep(10);
    /* Send the Startup IPI */
    lapic_write(APICREG_ICR1, ((uint32_t)target_apic_id) << 24);
    lapic_write(APICREG_ICR0, 0x4600 | (uint32_t)(size_t)trampoline);
    /* wait 1ms */
    sleep(1);
    if (check_ap_flag()) {
        return 0;
    } else {
        /* Send the Startup IPI again */
        lapic_write(APICREG_ICR1, ((uint32_t)target_apic_id) << 24);
        lapic_write(APICREG_ICR0, 0x4600 | (uint32_t)(size_t)trampoline);
        /* wait 1s */
        sleep(1000);
        if (check_ap_flag())
            return 0;
        else
            return -1;
    }
}
