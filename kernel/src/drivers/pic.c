#include <kernel.h>
#include <stdint.h>
#include <cio.h>
#include <system.h>

void map_PIC(uint8_t PIC0Offset, uint8_t PIC1Offset) {
	uint8_t mask0;
	uint8_t mask1;

	mask0 = get_PIC0_mask();		// save PIC 0 and 1's masks
	mask1 = get_PIC1_mask();

	port_out_b(0x20, 0x11);			// initialise
	port_out_b(0xA0, 0x11);

	port_out_b(0x21, PIC0Offset);	// set offsets
	port_out_b(0xA1, PIC1Offset);

	port_out_b(0x21, 0x04);			// PIC wiring info
	port_out_b(0xA1, 0x02);

	port_out_b(0x21, 0x01);			// environment info
	port_out_b(0xA1, 0x01);

	set_PIC0_mask(mask0);		// restore masks
	set_PIC1_mask(mask1);
}

void set_PIC0_mask(uint8_t mask) {
    port_out_b(0x21, mask);
    return;
}

void set_PIC1_mask(uint8_t mask) {
    port_out_b(0xA1, mask);
    return;
}

uint8_t get_PIC0_mask(void) {
    return port_in_b(0x21);
}

uint8_t get_PIC1_mask(void) {
    return port_in_b(0xA1);
}
