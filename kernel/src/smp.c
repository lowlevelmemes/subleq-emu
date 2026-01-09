/*
 * SMP support using Limine MP protocol
 * Much simpler than custom trampoline - Limine handles AP bootstrap
 */

#include <stdint.h>
#include <limine.h>
#include <smp.h>
#include <klib.h>
#include <apic.h>
#include <idt.h>
#include <pmm.h>
#include <subleq.h>
#include <dawn.h>
#include <cpu.h>

/* MP request - only needed by this TU */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_mp_request mp_request = {
    .id = LIMINE_MP_REQUEST_ID,
    .revision = 0,
    .flags = 0
};

#define MAX_CPUS 128

int cpu_count = 1;

cpu_local_t cpu_locals[MAX_CPUS];

/* Set FS base to point to CPU-local structure */
static inline void cpu_local_init(void *cpu_local) {
    uint64_t addr = (uint64_t)cpu_local;
    /* Write to IA32_FS_BASE MSR (0xC0000100) */
    asm volatile (
        "wrmsr"
        :
        : "c" (0xC0000100),
          "a" ((uint32_t)addr),
          "d" ((uint32_t)(addr >> 32))
    );
}

/* Get CPU number from FS-based cpu_local */
int get_cpu_number(void) {
    int cpu_num;
    asm volatile ("movl %%fs:0, %0" : "=r" (cpu_num));
    return cpu_num;
}

/* AP entry point - called by Limine for each AP */
static void ap_entry(struct limine_mp_info *info) {
    uint64_t cpu_num = info->extra_argument;

    /* Switch to dawn page tables FIRST - Limine's tables may not have APIC MMIO mapped */
    asm volatile (
        "movq %0, %%cr3"
        :
        : "r" ((size_t)dawn_pagemap - PHYS_MEM_OFFSET)
        : "memory"
    );

    /* Set up CPU-local storage */
    cpu_local_init(&cpu_locals[cpu_num]);

    /* Load IDT - required before enabling interrupts */
    idt_load();

    /* Enable SIMD (SSE/AVX/AVX-512) on this CPU */
    enable_simd();

    /* Enable LAPIC */
    lapic_enable();

    if (cpu_locals[cpu_num].lapic_id != mp_request.response->bsp_lapic_id) {
        /* Enable interrupts */
        asm volatile ("sti");

        /* Run the SUBLEQ emulator */
        subleq();
    }
}

/* Initialize SMP using Limine MP protocol */
void smp_init(void) {
    struct limine_mp_response *mp = mp_request.response;
    kprint(KPRN_INFO, "smp: smp_init called, mp=%X", (size_t)mp);
    if (!mp) return;

    kprint(KPRN_INFO, "smp: mp->cpu_count=%U", mp->cpu_count);
    cpu_count = 0;

    for (uint64_t i = 0; i < mp->cpu_count; i++) {
        kprint(KPRN_INFO, "smp: Processing CPU %U", i);
        struct limine_mp_info *cpu = mp->cpus[i];

        if (cpu_count >= MAX_CPUS) break;

        /* Set up CPU local struct */
        cpu_local_t *cpu_local = &cpu_locals[cpu_count];
        cpu_local->cpu_number = cpu_count;
        cpu_local->lapic_id = cpu->lapic_id;

        /* BSP is already running */
        if (cpu->lapic_id == mp->bsp_lapic_id) {
            ap_entry(&(struct limine_mp_info){.extra_argument = cpu_count++});
            continue;
        }

        /* Start AP via Limine */
        cpu->extra_argument = (uint64_t)cpu_count;
        __atomic_store_n(&cpu->goto_address, ap_entry, __ATOMIC_SEQ_CST);

        cpu_count++;
    }
}
