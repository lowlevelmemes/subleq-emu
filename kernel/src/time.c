#include <time.h>
#include <pit.h>
#include <apic.h>
#include <mouse.h>
#include <smp.h>
#include <dawn.h>

volatile uint64_t uptime_raw = 0;
volatile uint64_t uptime_sec = 0;

void timer_interrupt(void) {
    if (!(++uptime_raw % KRNL_PIT_FREQ))
        uptime_sec++;

    if (dawn_ready) {
        /* raise vector 0x80 for all APs */
        for (int i = 1; i < cpu_count; i++) {
            lapic_write(LAPIC_ICR1, ((uint32_t)cpu_locals[i].lapic_id) << 24);
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
