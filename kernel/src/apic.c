#include <stdint.h>
#include <stddef.h>
#include <pmm.h>
#include <acpi.h>
#include <klib.h>
#include <apic.h>
#include <cpu.h>

/** contains pieces of code from https://nemez.net/osdev, credits to Nemes **/

#define IA32_APIC_BASE_MSR 0x1B

static inline size_t get_lapic_base(void) {
    return (rdmsr(IA32_APIC_BASE_MSR) & 0xFFFFFFFFFFFFF000ULL) + PHYS_MEM_OFFSET;
}

uint32_t lapic_read(uint32_t reg) {
    return *((volatile uint32_t *)(get_lapic_base() + reg));
}

void lapic_write(uint32_t reg, uint32_t val) {
    *((volatile uint32_t *)(get_lapic_base() + reg)) = val;
    return;
}

uint32_t ioapic_read(size_t ioapic_num, uint32_t reg) {
    volatile uint32_t *ioapic_base = (volatile uint32_t *)((size_t)io_apics[ioapic_num]->addr + PHYS_MEM_OFFSET);
    *ioapic_base = reg;
    return *(ioapic_base + 4);
}

void ioapic_write(size_t ioapic_num, uint32_t reg, uint32_t val) {
    volatile uint32_t *ioapic_base = (volatile uint32_t *)((size_t)io_apics[ioapic_num]->addr + PHYS_MEM_OFFSET);
    *ioapic_base = reg;
    *(ioapic_base + 4) = val;
    return;
}

static uint32_t get_max_redir(size_t ioapic_num) {
    return (ioapic_read(ioapic_num, 1) & 0xff0000) >> 16;
}

static size_t get_ioapic_from_gsi(uint32_t gsi) {
    for (size_t i = 0; i < io_apic_ptr; i++) {
        if (io_apics[i]->gsib <= gsi && io_apics[i]->gsib + get_max_redir(i) > gsi)
            return i;
    }
    return -1;
}

static void ioapic_redirect(uint8_t irq, uint32_t gsi, uint16_t flags, uint8_t apic) {
    size_t ioapic = get_ioapic_from_gsi(gsi);

    /* offset the interrupt vector to start directly after exception handlers */
    uint64_t redirection = irq + 32;
    /* active low */
    if (flags & 2) {
        redirection |= 1 << 13;
    }
    /* level triggered */
    if (flags & 8) {
        redirection |= 1 << 15;
    }
    /* put the APIC ID of the target APIC (the APIC that will handle this IRQ) */
    redirection |= ((uint64_t)apic) << 56;

    /* IOREGTBL registers start at index 16 and GSI has to be offset by the I/O */
    /* APIC's interrupt base */
    uint32_t ioredtbl = (gsi - io_apics[ioapic]->gsib) * 2 + 16;

    /* write the IOREGTBL as two 32 bit writes */
    ioapic_write(ioapic, ioredtbl + 0, (uint32_t)(redirection));
    ioapic_write(ioapic, ioredtbl + 1, (uint32_t)(redirection >> 32));
}

static void lapic_set_nmi(uint8_t vec, uint16_t flags, uint8_t lint) {
    /* set as NMI and set the desired interrupt vector from the IDT */
    uint32_t nmi = 800 | vec;
    /* active low */
    if (flags & 2) {
        nmi |= 1 << 13;
    }
    /* level triggered */
    if (flags & 8) {
        nmi |= 1 << 15;
    }
    if (lint == 1) {
        lapic_write(LAPIC_LINT1, nmi);
    } else if (lint == 0) {
        lapic_write(LAPIC_LINT0, nmi);
    }
}

static void install_redirs(void) {
    /* install IRQ 0 ISO */
    for (size_t i = 0; i < iso_ptr; i++) {
        if (isos[i]->irq_source == 0) {
            ioapic_redirect(isos[i]->irq_source, isos[i]->gsi, isos[i]->flags, local_apics[0]->apic_id);
            goto irq0_found;
        }
    }
    /* not found in the ISOs, force IRQ 0 */
    ioapic_redirect(0, 0, 0, local_apics[0]->apic_id);
irq0_found:
    /* install IRQ 1 ISO */
    for (size_t i = 0; i < iso_ptr; i++) {
        if (isos[i]->irq_source == 1) {
            ioapic_redirect(isos[i]->irq_source, isos[i]->gsi, isos[i]->flags, local_apics[0]->apic_id);
            goto irq1_found;
        }
    }
    /* install keyboard redirect (IRQ 1) */
    ioapic_redirect(1, 1, 0, local_apics[0]->apic_id);
irq1_found:
    /* install IRQ 12 ISO */
    for (size_t i = 0; i < iso_ptr; i++) {
        if (isos[i]->irq_source == 12) {
            ioapic_redirect(isos[i]->irq_source, isos[i]->gsi, isos[i]->flags, local_apics[0]->apic_id);
            goto irq12_found;
        }
    }
    /* install mouse redirect (IRQ 12) */
    ioapic_redirect(12, 12, 0, local_apics[0]->apic_id);
irq12_found:
    return;
}

static void install_nmis(void) {
    for (size_t i = 0; i < nmi_ptr; i++)
        lapic_set_nmi(0x90 + i, nmis[i]->flags, nmis[i]->lint);
    return;
}

void lapic_enable(void) {
    lapic_write(LAPIC_SVR, lapic_read(LAPIC_SVR) | 0x1FF);
}

void apic_init(void) {
    kprint(KPRN_INFO, "APIC: Installing interrupt source overrides...");
    install_redirs();
    kprint(KPRN_INFO, "APIC: Installing NMIs...");
    install_nmis();
    /* enable lapic */
    kprint(KPRN_INFO, "APIC: Enabling local APIC...");
    lapic_enable();
    kprint(KPRN_INFO, "APIC: Done.");
    return;
}

void eoi(void) {
    lapic_write(LAPIC_EOI, 0);
    return;
}
