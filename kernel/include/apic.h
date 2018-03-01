#ifndef __APIC_H__
#define __APIC_H__

#define APICREG_ICR0 0x300
#define APICREG_ICR1 0x310

void lapic_enable(void);
uint32_t lapic_read(uint32_t reg);
void lapic_write(uint32_t reg, uint32_t val);
uint32_t ioapic_read(size_t ioapic_num, uint32_t reg);
void ioapic_write(size_t ioapic_num, uint32_t reg, uint32_t val);


#endif
