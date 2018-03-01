#include <kernel.h>
#include <stdint.h>
#include <cio.h>
#include <klib.h>
#include <system.h>

void set_pit_freq(uint32_t frequency) {
    kprint(KPRN_INFO, "PIT: Setting frequency to %uHz", (uint64_t)frequency);

    uint16_t x = 1193182 / frequency;
    if ((1193182 % frequency) > (frequency / 2))
        x++;
        
    port_out_b(0x40, (uint8_t)(x & 0x00ff));
    port_out_b(0x40, (uint8_t)((x & 0xff00) >> 8));

    kprint(KPRN_INFO, "PIT: Frequency updated");

    return;
}

void sleep(uint64_t time) {
    /* implements sleep in milliseconds */

    uint64_t final_time = uptime_raw + (time * (KRNL_PIT_FREQ / 1000));

    while (uptime_raw < final_time);

    return;
}
