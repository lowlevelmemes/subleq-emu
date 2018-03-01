#include <stdint.h>
#include <kernel.h>
#include <klib.h>
#include <tty.h>
#include <apic.h>
#include <acpi.h>
#include <graphics.h>
#include <cio.h>

uint64_t last_frame_counter = 0;

extern void *initramfs_addr;

void subleq_redraw_screen(void) {
    asm volatile (
        "1: "
        "lodsd;"
        "rol eax, 8;"
        "bswap eax;"
        "stosd;"
        "loop 1b;"
        :
        : "S" (initramfs_addr + 256*1024*1024),
          "D" (framebuffer),
          "c" (edid_width * edid_height)
        : "rax"
    );
}

volatile uint64_t uptime_raw = 0;
volatile uint64_t uptime_sec = 0;

extern uint64_t _readram(uint64_t);
extern void _writeram(uint64_t, uint64_t);
extern void poll_mouse(void);

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

    if (!(uptime_raw % (KRNL_PIT_FREQ / 20))) {     /* 20 FPS */
        if (last_frame_counter != _readram(335540096 + 32)) {
            last_frame_counter = _readram(335540096 + 32);
            subleq_redraw_screen();
        }
    }

    return;
}
