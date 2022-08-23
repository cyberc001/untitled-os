#include "cpu_int.h"
#include "x86/idt.h"

#include <stddef.h>
#include "kernlib/kernmem.h"

void cpu_interrupt_init()
{
	init_idt();
}

int cpu_interrupt_set_gate(void* val, uint32_t slot, uint8_t type)
{
	uint8_t type_attributes = X86_IDT_PRESENT;
	switch(type)
	{
		case CPU_INT_TYPE_INTERRUPT: 	type_attributes |= X86_IDT_GATE_TYPE_INT32; break;
		case CPU_INT_TYPE_TRAP: 		type_attributes |= X86_IDT_GATE_TYPE_TRAP32; break;
		case CPU_INT_TYPE_TASK:			type_attributes |= X86_IDT_GATE_TYPE_TASK; break;
	}
	return set_idt_gate(val, slot, type_attributes);
}
