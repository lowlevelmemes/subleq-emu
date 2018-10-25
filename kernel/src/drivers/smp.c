#include <kernel.h>
#include <stdint.h>
#include <smp.h>
#include <klib.h>
#include <apic.h>
#include <acpi.h>
#include <system.h>
#include <paging.h>
#include <subleq.h>
#include <panic.h>

#define MAX_CPUS 128
#define CPU_STACK_SIZE 4096

int cpu_count = 1;

typedef struct {
    uint32_t unused0 __attribute__((aligned(16)));
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t unused1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t unused2;
    uint32_t iopb_offset;
} __attribute__((packed)) tss_t;

typedef struct {
    uint8_t stack[CPU_STACK_SIZE] __attribute__((aligned(16)));
} cpu_stack_t;

static cpu_stack_t cpu_stacks[MAX_CPUS];
static cpu_stack_t cpu_int_stacks[MAX_CPUS];
static cpu_local_t cpu_locals[MAX_CPUS];
static tss_t cpu_tss[MAX_CPUS] __attribute__((aligned(16)));

void ap_kernel_entry(void) {
    /* APs jump here after initialisation */

    kprint(KPRN_INFO, "SMP: Started up AP #%u", get_cpu_number());
    kprint(KPRN_INFO, "SMP: AP #%u kernel stack top: %X", get_cpu_number(), get_cpu_kernel_stack());

    lapic_enable();

    asm volatile (
        "mov cr3, rax;"
        "sti;"
        :
        : "a" ((size_t)subleq_pagemap - PHYS_MEM_OFFSET)
    );

    subleq();

    return;
}

static int start_ap(uint8_t target_apic_id, int cpu_number) {
    if (cpu_number == MAX_CPUS) {
        panic("smp: CPU limit exceeded", cpu_count);
    }

    /* create CPU local struct */
    cpu_local_t *cpu_local = &cpu_locals[cpu_number];

    cpu_local->cpu_number = cpu_number;
    cpu_local->kernel_stack = (void *)&cpu_stacks[cpu_number];

    /* prepare TSS */
    tss_t *tss = &cpu_tss[cpu_number];

    tss->rsp0 = (uint64_t)(&cpu_stacks[cpu_number]);
    tss->ist1 = (uint64_t)(&cpu_int_stacks[cpu_number]);

    void *trampoline = prepare_smp_trampoline((void *)ap_kernel_entry,
                                (void *)((size_t)kernel_pagemap - KERNEL_PHYS_OFFSET),
                                &cpu_stacks[cpu_number], cpu_local, tss);

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
        goto success;
    } else {
        /* Send the Startup IPI again */
        lapic_write(APICREG_ICR1, ((uint32_t)target_apic_id) << 24);
        lapic_write(APICREG_ICR0, 0x4600 | (uint32_t)(size_t)trampoline);
        /* wait 1s */
        sleep(1000);
        if (check_ap_flag())
            goto success;
        else
            return -1;
    }

success:
    return 0;
}

static void init_cpu0(void) {
    /* create CPU 0 local struct */
    cpu_local_t *cpu_local = &cpu_locals[0];

    cpu_local->cpu_number = 0;
    cpu_local->kernel_stack = (void *)&cpu_stacks[0];

    tss_t *tss = &cpu_tss[0];

    tss->rsp0 = (uint64_t)(&cpu_stacks[0]);
    tss->ist1 = (uint64_t)(&cpu_int_stacks[0]);

    init_cpu0_local(cpu_local, tss);

    return;
}

void init_smp(void) {
    /* prepare CPU 0 first */
    init_cpu0();

    asm volatile ("sti");

    /* start up the APs and jump them into the kernel */
    for (size_t i = 1; i < local_apic_ptr; i++) {
        kprint(KPRN_INFO, "smp: Starting up AP #%u", i);
        if (start_ap(local_apics[i]->apic_id, cpu_count)) {
            kprint(KPRN_ERR, "smp: Failed to start AP #%u", i);
            continue;
        }
        cpu_count++;
        /* wait a bit */
        sleep(10);
    }

    kprint(KPRN_INFO, "smp: Total CPU count: %u", cpu_count);

    return;
}
