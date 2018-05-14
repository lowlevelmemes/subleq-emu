#include <stdint.h>
#include <kernel.h>
#include <paging.h>
#include <klib.h>
#include <cio.h>
#include <system.h>
#include <panic.h>
#include <graphics.h>
#include <smp.h>
#include <mouse.h>
#include <subleq.h>
#include <acpi.h>
#include <apic.h>
#include <e820.h>

size_t memory_size;

void kernel_init(void) {
    /* interrupts disabled */

    /* mask all PIC IRQs */
    kprint(KPRN_INFO, "PIC: Masking and remapping the legacy PICs...");
    set_PIC0_mask(0b11111111);
    set_PIC1_mask(0b11111111);
    /* remap PIC where it doesn't bother us */
    map_PIC(0xa0, 0xa8);
    kprint(KPRN_INFO, "PIC: PIC 0 and 1 disabled.");

    /* build descriptor tables */
    load_IDT();

    /* detect memory */
    init_e820();
    if (memory_size < 0x1f000000)
        panic("subleq-emu needs at least 500MiB of RAM to run.", memory_size);

    /* initialise paging */
    init_paging();

    #ifdef _SERIAL_KERNEL_OUTPUT_
      /* initialise kernel serial debug console */
      debug_kernel_console_init();
    #endif

    /* initialise graphics mode */
    init_graphics();

    init_mouse();

    /* set PIT frequency */
    set_pit_freq(KRNL_PIT_FREQ);

    /* initialise ACPI */
    init_acpi();

    /* initialise APIC */
    init_apic();

    /* enable interrupts for the first time */
    kprint(KPRN_INFO, "INIT: ENABLE INTERRUPTS");
    ENABLE_INTERRUPTS;

    /****** END OF EARLY BOOTSTRAP ******/

    init_subleq();
    init_smp();
    subleq_acquire_mem();


    /* pass control to the emulator */
    subleq();

}
