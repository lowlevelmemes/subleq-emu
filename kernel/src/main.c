/*
 * subleq-emu kernel entry point
 * Uses Limine boot protocol (base revision 4)
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>

#include <pmm.h>
#include <klib.h>
#include <subleq.h>
#include <dawn.h>
#include <smp.h>
#include <acpi.h>
#include <apic.h>
#include <pit.h>
#include <idt.h>
#include <mouse.h>
#include <cpu.h>

/* Limine base revision - use revision 4 (latest) */
__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(4);

/* Request delimiters */
__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

/* Halt and catch fire */
static void hcf(void) {
    for (;;) {
        asm volatile ("cli; hlt");
    }
}


/* Kernel main entry point */
void kmain(void) {
    /* Check that Limine supports our base revision */
    if (!LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision)) {
        hcf();
    }

    /* Initialise IDT */
    idt_init();

    /* Initialize physical memory manager (sets up hhdm_offset, kernel bases, memmap) */
    pmm_init();

    /* Initialize mouse */
    mouse_init();

    /* Initialize ACPI (uses its own RSDP request) */
    acpi_init();

    /* Initialize Dawn emulator (framebuffer, module, date, pagemap, switches CR3) */
    dawn_init();

    /* Initialize APIC (requires APIC pages to be mapped) */
    apic_init();

    /* Initialize PIT timer */
    set_pit_freq(KRNL_PIT_FREQ);

    /* Start APs via Limine MP (uses its own MP request) */
    smp_init();

    /* Enable interrupts and run emulator */
    asm volatile ("sti");

    /* Pass control to the emulator - this never returns */
    subleq();

    hcf();
}
