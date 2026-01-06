#ifndef __APIC_H__
#define __APIC_H__

#include <stdint.h>
#include <stddef.h>

/* Local APIC register offsets */
#define LAPIC_EOI       0x0B0   /* End of Interrupt */
#define LAPIC_SVR       0x0F0   /* Spurious Interrupt Vector */
#define LAPIC_ICR0      0x300   /* Interrupt Command Register (low) */
#define LAPIC_ICR1      0x310   /* Interrupt Command Register (high) */
#define LAPIC_LINT0     0x350   /* Local Interrupt 0 */
#define LAPIC_LINT1     0x360   /* Local Interrupt 1 */

void apic_init(void);

void lapic_enable(void);
uint32_t lapic_read(uint32_t reg);
void lapic_write(uint32_t reg, uint32_t val);
uint32_t ioapic_read(size_t ioapic_num, uint32_t reg);
void ioapic_write(size_t ioapic_num, uint32_t reg, uint32_t val);
void eoi(void);

#endif
