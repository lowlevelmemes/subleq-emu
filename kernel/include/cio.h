#ifndef __CIO_H__
#define __CIO_H__

#include <stdint.h>

#define port_out_b(port, value) ({				\
	asm volatile (	"out %1, al"				\
					:							\
					: "a" ((uint8_t)value), "Nd" ((uint16_t)port)	\
					: "memory");						\
})

#define port_out_w(port, value) ({				\
	asm volatile (	"out %1, ax"				\
					:							\
					: "a" ((uint16_t)value), "Nd" ((uint16_t)port)	\
					: "memory");						\
})

#define port_out_d(port, value) ({				\
	asm volatile (	"out %1, eax"				\
					:							\
					: "a" ((uint32_t)value), "Nd" ((uint16_t)port)	\
					: "memory");						\
})

#define port_in_b(port) ({						\
	uint8_t value;								\
	asm volatile (	"in al, %1"					\
					: "=a" (value)				\
					: "Nd" ((uint16_t)port)				\
					: "memory");						\
	value;										\
})

#define port_in_w(port) ({						\
	uint16_t value;								\
	asm volatile (	"in ax, %1"					\
					: "=a" (value)				\
					: "Nd" ((uint16_t)port)				\
					: "memory");						\
	value;										\
})

#define port_in_d(port) ({						\
	uint32_t value;								\
	asm volatile (	"in eax, %1"				\
					: "=a" (value)				\
					: "Nd" ((uint16_t)port)				\
					: "memory");						\
	value;										\
})

#endif
