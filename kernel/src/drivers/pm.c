#include <stdint.h>
#include <kernel.h>
#include <klib.h>
#include <cio.h>

void shutdown(void) {
    asm volatile ("cli");

    kprint(KPRN_INFO, "PM: shutdown request");

    for (;;) { asm volatile ("hlt"); }
}

void reboot(void) {
    asm volatile ("cli");

    kprint(KPRN_INFO, "PM: reboot request");

    kprint(KPRN_INFO, "PM: attempting PS/2 reset");

    while (port_in_b(0x64) & 0x02);

    port_out_b(0x64, 0xfe);

    kprint(KPRN_INFO, "PM: PS/2 reset failed, triple faulting");

    uint64_t dummy_idt_ptr[2] = {0,0};

    asm volatile (
        "lidt [rax];"
        "nop;"
        "nop;"
        "int 0x80;"
        "nop;"
        "nop;"
        :
        : "a" (dummy_idt_ptr)
    );

    for (;;) { asm volatile ("hlt"); }
}
