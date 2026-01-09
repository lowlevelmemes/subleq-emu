#include <stdint.h>
#include <idt.h>

/* IDT entry structure (128 bits for 64-bit mode) */
typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

/* IDT pointer structure */
typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_ptr_t;

/* IDT with 256 entries */
static idt_entry_t idt[256] __attribute__((aligned(16)));
static idt_ptr_t idt_ptr;

/* External handlers from isr.asm */
extern void handler_div0(void);
extern void handler_debug(void);
extern void handler_nmi(void);
extern void handler_breakpoint(void);
extern void handler_overflow(void);
extern void handler_bound_range_exceeded(void);
extern void handler_invalid_opcode(void);
extern void handler_device_not_available(void);
extern void handler_double_fault(void);
extern void handler_coprocessor_segment_overrun(void);
extern void handler_invalid_tss(void);
extern void handler_segment_not_present(void);
extern void handler_stack_segment_fault(void);
extern void handler_gpf(void);
extern void handler_pf(void);
extern void handler_x87_exception(void);
extern void handler_alignment_check(void);
extern void handler_machine_check(void);
extern void handler_simd_exception(void);
extern void handler_virtualisation_exception(void);
extern void handler_security_exception(void);
extern void handler_irq_apic(void);
extern void handler_wakeup(void);
extern void irq0_handler(void);
extern void keyboard_isr(void);
extern void mouse_isr(void);

/* Abort handler - just halt */
static void handler_abort(void) {
    for (;;) {
        asm volatile ("cli; hlt");
    }
}

/* Set an IDT entry */
static void set_idt_entry(uint8_t vector, void (*handler)(void), uint16_t selector, uint8_t ist, uint8_t type_attr) {
    uint64_t addr = (uint64_t)handler;
    idt[vector].offset_low = addr & 0xFFFF;
    idt[vector].selector = selector;
    idt[vector].ist = ist & 0x7;
    idt[vector].type_attr = type_attr;
    idt[vector].offset_mid = (addr >> 16) & 0xFFFF;
    idt[vector].offset_high = (addr >> 32) & 0xFFFFFFFF;
    idt[vector].reserved = 0;
}

#define IDT_ATTR_PRESENT    0x80
#define IDT_ATTR_RING0      0x00
#define IDT_ATTR_INT_GATE   0x0E    /* 64-bit interrupt gate */

#define LIMINE_CS           0x28    /* Limine 64-bit code selector */

void idt_init(void) {
    uint8_t attr = IDT_ATTR_PRESENT | IDT_ATTR_RING0 | IDT_ATTR_INT_GATE;
    uint8_t ist = 0;  /* No IST needed */

    /* CPU exceptions (0x00 - 0x1F) */
    set_idt_entry(0x00, handler_div0, LIMINE_CS, ist, attr);
    set_idt_entry(0x01, handler_debug, LIMINE_CS, ist, attr);
    set_idt_entry(0x02, handler_nmi, LIMINE_CS, ist, attr);
    set_idt_entry(0x03, handler_breakpoint, LIMINE_CS, ist, attr);
    set_idt_entry(0x04, handler_overflow, LIMINE_CS, ist, attr);
    set_idt_entry(0x05, handler_bound_range_exceeded, LIMINE_CS, ist, attr);
    set_idt_entry(0x06, handler_invalid_opcode, LIMINE_CS, ist, attr);
    set_idt_entry(0x07, handler_device_not_available, LIMINE_CS, ist, attr);
    set_idt_entry(0x08, handler_double_fault, LIMINE_CS, ist, attr);
    set_idt_entry(0x09, handler_coprocessor_segment_overrun, LIMINE_CS, ist, attr);
    set_idt_entry(0x0A, handler_invalid_tss, LIMINE_CS, ist, attr);
    set_idt_entry(0x0B, handler_segment_not_present, LIMINE_CS, ist, attr);
    set_idt_entry(0x0C, handler_stack_segment_fault, LIMINE_CS, ist, attr);
    set_idt_entry(0x0D, handler_gpf, LIMINE_CS, ist, attr);
    set_idt_entry(0x0E, handler_pf, LIMINE_CS, ist, attr);
    /* 0x0F reserved */
    set_idt_entry(0x10, handler_x87_exception, LIMINE_CS, ist, attr);
    set_idt_entry(0x11, handler_alignment_check, LIMINE_CS, ist, attr);
    set_idt_entry(0x12, handler_machine_check, LIMINE_CS, ist, attr);
    set_idt_entry(0x13, handler_simd_exception, LIMINE_CS, ist, attr);
    /* 0x14 - 0x1C reserved */
    set_idt_entry(0x1D, handler_virtualisation_exception, LIMINE_CS, ist, attr);
    set_idt_entry(0x1E, handler_security_exception, LIMINE_CS, ist, attr);

    /* IRQs via APIC (0x20 - 0x2F) */
    set_idt_entry(0x20, irq0_handler, LIMINE_CS, ist, attr);      /* PIT timer */
    set_idt_entry(0x21, keyboard_isr, LIMINE_CS, ist, attr);      /* Keyboard */
    for (int i = 0x22; i <= 0x2B; i++) {
        set_idt_entry(i, handler_irq_apic, LIMINE_CS, ist, attr);
    }
    set_idt_entry(0x2C, mouse_isr, LIMINE_CS, ist, attr);         /* Mouse */
    for (int i = 0x2D; i <= 0x2F; i++) {
        set_idt_entry(i, handler_irq_apic, LIMINE_CS, ist, attr);
    }

    /* IPI vectors */
    set_idt_entry(0x80, handler_wakeup, LIMINE_CS, ist, attr);    /* Wakeup IPI */
    set_idt_entry(0x81, (void (*)(void))handler_abort, LIMINE_CS, ist, attr);  /* Abort IPI */

    /* Additional APIC vectors (0x90 - 0x97, 0xFF) */
    for (int i = 0x90; i <= 0x97; i++) {
        set_idt_entry(i, handler_irq_apic, LIMINE_CS, ist, attr);
    }
    set_idt_entry(0xFF, handler_irq_apic, LIMINE_CS, ist, attr);  /* Spurious */

    idt_load();
}

void idt_load(void) {
    /* Load IDT */
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint64_t)&idt;
    asm volatile ("lidt %0" :: "m"(idt_ptr));
}
