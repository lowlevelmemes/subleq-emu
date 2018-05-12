#include <stdint.h>
#include <kernel.h>
#include <klib.h>
#include <apic.h>
#include <acpi.h>
#include <graphics.h>
#include <cio.h>
#include <mouse.h>
#include <subleq.h>

volatile uint64_t uptime_raw = 0;
volatile uint64_t uptime_sec = 0;

void timer_interrupt(void) {

    if (!(++uptime_raw % KRNL_PIT_FREQ))
        uptime_sec++;

    /* raise vector 32 for all APs */
    lapic_write(APICREG_ICR0, 0x20 | (1 << 18) | (1 << 19));

    _writeram(335544304, _readram(335544304) + (0x100000000 / KRNL_PIT_FREQ));

    if (!(uptime_raw % (KRNL_PIT_FREQ / MOUSE_POLL_FREQ)))
        poll_mouse();

    if (!(uptime_raw % (KRNL_PIT_FREQ / SCREEN_REFRESH_FREQ)))
        subleq_redraw_screen();

    return;
}
