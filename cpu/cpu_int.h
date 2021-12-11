#ifndef CPU_INT_H
#define CPU_INT_H

#ifdef CPU_I386

static inline void cpu_interrupt_set(int enabled)
{
	if(enabled)	asm volatile ("sti");
	else		asm volatile ("cli");
}

#else
	#error manipulatin CPU interrupts is not supported for this platform
#endif

#endif
