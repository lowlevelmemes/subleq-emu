#ifndef __CIO_H__
#define __CIO_H__

#include <stdint.h>

#define BB                      asm volatile ("xchg bx, bx")
#define DISABLE_INTERRUPTS      asm volatile ("cli")
#define ENABLE_INTERRUPTS       asm volatile ("sti")
#define ENTER_IDLE              \
    asm volatile (              \
                    "sti;"      \
                    "1:"        \
                    "mov esp, 0xeffff0;"    \
                    "hlt;"      \
                    "jmp 1b;"   \
                 )
#define SYSTEM_HALT              \
    asm volatile (              \
                    "1:"        \
                    "cli;"      \
                    "hlt;"      \
                    "jmp 1b;"   \
                 )

#define port_out_b(port, value) ({				\
	asm volatile (	"out dx, al"				\
					:							\
					: "a" (value), "d" (port)	\
					: );						\
})

#define port_out_w(port, value) ({				\
	asm volatile (	"out dx, ax"				\
					:							\
					: "a" (value), "d" (port)	\
					: );						\
})

#define port_out_d(port, value) ({				\
	asm volatile (	"out dx, eax"				\
					:							\
					: "a" (value), "d" (port)	\
					: );						\
})

#define port_in_b(port) ({						\
	uint8_t value;								\
	asm volatile (	"in al, dx"					\
					: "=a" (value)				\
					: "d" (port)				\
					: );						\
	value;										\
})

#define port_in_w(port) ({						\
	uint16_t value;								\
	asm volatile (	"in ax, dx"					\
					: "=a" (value)				\
					: "d" (port)				\
					: );						\
	value;										\
})

#define port_in_d(port) ({						\
	uint32_t value;								\
	asm volatile (	"in eax, dx"				\
					: "=a" (value)				\
					: "d" (port)				\
					: );						\
	value;										\
})

#endif
