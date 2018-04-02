#include <stdint.h>
#include <kernel.h>
#include <klib.h>
#include <tty.h>
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

    poll_mouse();

    _writeram(335544304, _readram(335544304) + (0x100000000 / KRNL_PIT_FREQ));

    if (tty_needs_refresh != -1) {
        if (!(uptime_raw % TTY_REDRAW_LIMIT)) {
            tty_refresh(tty_needs_refresh);
            tty_needs_refresh = -1;
        }
    }

    if (!(uptime_raw % (KRNL_PIT_FREQ / 40))) {     /* 40 FPS */
        subleq_redraw_screen();
    }

    return;
}
