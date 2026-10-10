#include <time.h>
#include <pit.h>
#include <apic.h>
#include <mouse.h>
#include <smp.h>
#include <dawn.h>

volatile uint64_t uptime_raw = 0;

void timer_interrupt(void) {
    uptime_raw++;

    if (dawn_ready) {
        /* raise vector 0x80 on the last AP, which flushes I/O, updates the
         * mouse, and redraws the screen; the others need no ticks, as they
         * wait for Dawn to start them without halting */
        if (cpu_count > 1) {
            lapic_write(LAPIC_ICR1, ((uint32_t)cpu_locals[cpu_count - 1].lapic_id) << 24);
            lapic_write(LAPIC_ICR0, 0x80);
        }

        dawn_writeram(DAWN_RTC, dawn_readram(DAWN_RTC) + (DAWN_FIXED_ONE / KRNL_PIT_FREQ));

        if (cpu_count == 1) {
            dawn_io_flush();

            if (!(uptime_raw % (KRNL_PIT_FREQ / MOUSE_UPDATE_FREQ)))
                mouse_update();

            if (!(uptime_raw % (KRNL_PIT_FREQ / SCREEN_REFRESH_FREQ)))
                dawn_redraw_screen();
        }

    }

    return;
}

void timer_interrupt_ap(void) {
    if (get_cpu_number() == cpu_count - 1) {
        dawn_io_flush();

        if (!(uptime_raw % (KRNL_PIT_FREQ / MOUSE_UPDATE_FREQ)))
            mouse_update();

        if (!(uptime_raw % (KRNL_PIT_FREQ / SCREEN_REFRESH_FREQ)))
            dawn_redraw_screen();
    }

    return;
}
