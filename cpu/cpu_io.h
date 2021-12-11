#ifndef CPU_IO_H
#define CPU_IO_H

#include <stdint.h>

// port I/O

#ifdef CPU_I386

static inline uint8_t cpu_in8(uint32_t port)
{
	uint8_t ret;
	asm volatile ("inb %%dx, %%al" : "=a"(ret) : "d"(port));
	return ret;
}
static inline uint16_t cpu_in16(uint32_t port)
{
	uint16_t ret;
	asm volatile ("inw %%dx, %%ax" : "=a"(ret) : "d"(port));
	return ret;
}
static inline uint32_t cpu_in32(uint32_t port)
{
	uint32_t ret;
	asm volatile ("inl %%dx, %%eax" : "=a"(ret) : "d"(port));
	return ret;
}

static inline void cpu_out8(uint32_t port, uint8_t val)
{
	asm volatile ("outb %%al, %%dx" :: "d"(port), "a"(val));
}
static inline void cpu_out16(uint32_t port, uint16_t val)
{
	asm volatile ("outw %%ax, %%dx" :: "d"(port), "a"(val));
}
static inline void cpu_out32(uint32_t port, uint32_t val)
{
	asm volatile ("outl %%eax, %%dx" :: "d"(port), "a"(val));
}

#else
	#error CPU port I/O is not supported for this platform
#endif

#endif
