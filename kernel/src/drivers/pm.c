#include <stdint.h>
#include <kernel.h>
#include <klib.h>
#include <cio.h>
#include <acpi.h>

void shutdown(void) {
    asm volatile ("cli");

    kprint(KPRN_INFO, "PM: shutdown request");

    port_out_w((uint16_t)facp->PM1a_CNT_BLK, SLP_TYPa | (1 << 13));
    if (facp->PM1b_CNT_BLK)
        port_out_w((uint16_t)facp->PM1b_CNT_BLK, SLP_TYPb | (1 << 13));

    for (;;) { asm volatile ("hlt"); }
}

void reboot(void) {
    asm volatile ("cli");

    kprint(KPRN_INFO, "PM: reboot request");

    kprint(KPRN_INFO, "PM: attempting PS/2 reset");

    while (port_in_b(0x64) & 0x02);

    port_out_b(0x64, 0xfe);

    port_out_b(0x80, 0x00);
    port_out_b(0x80, 0x00);
    port_out_b(0x80, 0x00);
    port_out_b(0x80, 0x00);
    port_out_b(0x80, 0x00);
    port_out_b(0x80, 0x00);
    port_out_b(0x80, 0x00);
    port_out_b(0x80, 0x00);

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
