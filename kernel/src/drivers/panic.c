#include <kernel.h>
#include <klib.h>
#include <cio.h>
#include <system.h>
#include <apic.h>
#include <smp.h>

void panic(const char *msg, int code) {
    asm volatile ("cli" ::: "memory");
    kprint(KPRN_ERR, "!!! KERNEL PANIC !!!");
    kprint(KPRN_ERR, "Panic info: %s", msg);
    kprint(KPRN_ERR, "Error code: %X", (size_t)code);
    /* send abort ipi to all APs */
    for (int i = 0; i < cpu_count; i++) {
        if (i == get_cpu_number())
            continue;
        lapic_write(APICREG_ICR1, ((uint32_t)cpu_locals[i].lapic_id) << 24);
        lapic_write(APICREG_ICR0, 0x81);
    }
    kprint(KPRN_ERR, "SYSTEM HALTED");
    for (;;)
        asm volatile ("hlt" ::: "memory");
}
