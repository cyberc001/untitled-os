#ifndef CPU_INT_H
#define CPU_INT_H

#include <stdint.h>
#include <stddef.h>

static inline void cpu_interrupt_set(int enabled)
{
	if(enabled)	asm volatile ("sti");
	else		asm volatile ("cli");
}

// Generic interrupt types
#define CPU_INT_TYPE_INTERRUPT	0
#define CPU_INT_TYPE_TRAP		1
#define CPU_INT_TYPE_TASK		2

#define CPU_SYSCALL_GATE		0x80

void cpu_interrupt_init();

/* Returns 0 if gate slot is out of bounds */
int cpu_interrupt_set_gate(void* val, uint32_t slot, uint8_t type);

#endif
