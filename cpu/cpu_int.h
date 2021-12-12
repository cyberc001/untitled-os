#ifndef CPU_INT_H
#define CPU_INT_H

#include <stdint.h>

#ifdef CPU_I386

static inline void cpu_interrupt_set(int enabled)
{
	if(enabled)	asm volatile ("sti");
	else		asm volatile ("cli");
}

#else
	#error manipulating CPU interrupts is not supported for this platform
#endif


// Generic interrupt types
#define CPU_INT_TYPE_INTERRUPT	0
#define CPU_INT_TYPE_TRAP	1
#define CPU_INT_TYPE_TASK	2


void cpu_interrupt_init();

void cpu_interrupt_set_gate(void (*func)(), uint32_t slot, uint8_t type);

#endif
