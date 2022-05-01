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

static inline uint64_t cpu_in_msr(uint32_t msr)
{
	uint32_t edx, eax;
	asm volatile ("rdmsr" : "=a"(eax), "=d"(edx) : "c"(msr) : "memory");
	return ((uint64_t)edx << 32) | eax;
}
static inline void cpu_out_msr(uint32_t msr, uint64_t val)
{
	uint32_t edx = val >> 32, eax = (uint32_t)val;
	asm volatile ("wrmsr" :: "a"(eax), "d"(edx), "c"(msr) : "memory");
}

#else
	#error CPU port I/O is not supported for this platform
#endif


// misc operations

// assuming port 0x80 is unused
static inline void cpu_io_wait()
{
	cpu_out8(0x80, 0);
}

static inline uint64_t cpu_rdtsc()
{
	uint32_t edx, eax;
	asm volatile ("rdtsc" : "=a"(eax), "=d"(edx));
	return ((uint64_t)edx << 32) | eax;
}
static inline void delay(uint64_t cycles)
{
	uint64_t end = cpu_rdtsc() + cycles;
	while(cpu_rdtsc() < end)
		;
}

#endif
