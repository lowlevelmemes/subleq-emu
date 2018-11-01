#include <stdint.h>
#include <kernel.h>
#include <klib.h>
#include <apic.h>
#include <acpi.h>
#include <graphics.h>
#include <cio.h>
#include <mouse.h>
#include <subleq.h>
#include <system.h>
#include <smp.h>

volatile uint64_t uptime_raw = 0;
volatile uint64_t uptime_sec = 0;

void timer_interrupt(void) {

    if (!(++uptime_raw % KRNL_PIT_FREQ))
        uptime_sec++;

    if (subleq_ready) {

        /* raise vector 0x80 for all APs */
        for (int i = 1; i < cpu_count; i++) {
            lapic_write(APICREG_ICR1, ((uint32_t)cpu_locals[i].lapic_id) << 24);
            lapic_write(APICREG_ICR0, 0x80);
        }

        _writeram(335544304, _readram(335544304) + (0x100000000 / KRNL_PIT_FREQ));

        if (cpu_count == 1) {
            subleq_io_flush();

            if (!(uptime_raw % (KRNL_PIT_FREQ / MOUSE_UPDATE_FREQ)))
                mouse_update();

            if (!(uptime_raw % (KRNL_PIT_FREQ / SCREEN_REFRESH_FREQ)))
                subleq_redraw_screen();
        }

    }

    return;
}

void timer_interrupt_ap(void) {
    if (get_cpu_number() == cpu_count - 1) {
        subleq_io_flush();

        if (!(uptime_raw % (KRNL_PIT_FREQ / MOUSE_UPDATE_FREQ)))
            mouse_update();

        if (!(uptime_raw % (KRNL_PIT_FREQ / SCREEN_REFRESH_FREQ)))
            subleq_redraw_screen();
    }

    return;
}
