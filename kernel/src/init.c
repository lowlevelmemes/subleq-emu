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

void get_time(int *seconds, int *minutes, int *hours,
              int *days, int *months, int *years);

uint64_t get_jdn(int days, int months, int years) {
    return (1461 * (years + 4800 + (months - 14)/12))/4 + (367 * (months - 2 - 12 * ((months - 14)/12)))/12 - (3 * ((years + 4900 + (months - 14)/12)/100))/4 + days - 32075;
}

uint64_t get_dawn_epoch(int seconds, int minutes, int hours,
                        int days, int months, int years) {

    uint64_t jdn_current = get_jdn(days, months, years);
    uint64_t jdn_2016 = get_jdn(1, 1, 2016);

    uint64_t jdn_diff = jdn_current - jdn_2016;

    return (jdn_diff * (60 * 60 * 24)) + hours*3600 + minutes*60 + seconds;
}

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

    int hours, minutes, seconds, days, months, years;
    get_time(&seconds, &minutes, &hours, &days, &months, &years);
    kprint(KPRN_INFO, "Current time: %u/%u/%u %u:%u:%u", years, months, days, hours, minutes, seconds);

    uint64_t dawn_epoch = get_dawn_epoch(seconds, minutes, hours, days, months, years);
    kprint(KPRN_INFO, "Dawn epoch: %U", dawn_epoch);

    init_mouse();

    /* set PIT frequency */
    set_pit_freq(KRNL_PIT_FREQ);

    /* initialise ACPI */
    init_acpi();

    /* initialise APIC */
    init_apic();

    /****** END OF EARLY BOOTSTRAP ******/

    init_subleq();

    /* set the date to current time */
    _writeram(335544304, (dawn_epoch + uptime_sec) * 0x100000000);

    /* pass control to the emulator */
    subleq();

}
