#include "cpu_int.h"

#ifdef CPU_I386
#include "x86/idt.h"
#endif

void cpu_interrupt_init()
{
	#ifdef CPU_I386
	init_idt();
	#endif
}

void cpu_interrupt_set_gate(void (*func)(), uint32_t slot, uint8_t type)
{
	#ifdef CPU_I386
	uint8_t type_attributes = X86_IDT_PRESENT;

		#ifdef CPU_32BIT
		switch(type)
		{
			case CPU_INT_TYPE_INTERRUPT: 	type_attributes |= X86_IDT_GATE_TYPE_INT32; break;
			case CPU_INT_TYPE_TRAP: 	type_attributes |= X86_IDT_GATE_TYPE_TRAP32; break;
			case CPU_INT_TYPE_TASK:		type_attributes |= X86_IDT_GATE_TYPE_TASK; break;
		}
		#endif

	set_idt_gate(func, slot, type_attributes);
	#endif
}
