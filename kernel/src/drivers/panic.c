#include <kernel.h>
#include <klib.h>
#include <cio.h>
#include <system.h>
#include <apic.h>

void panic(const char *msg, int code) {
    DISABLE_INTERRUPTS;
    kprint(KPRN_ERR, "!!! KERNEL PANIC !!!");
    kprint(KPRN_ERR, "Panic info: %s", msg);
    kprint(KPRN_ERR, "Error code: %X", (size_t)code);
    /* send abort ipi to all APs */
    lapic_write(APICREG_ICR0, 0x81 | (1 << 18) | (1 << 19));
    kprint(KPRN_ERR, "SYSTEM HALTED");
    SYSTEM_HALT;
}
