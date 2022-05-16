#include "cpu_int.h"
#include "x86/idt.h"

#include <stddef.h>
#include "kernlib/kernmem.h"

static syscall_func_t* syscall_table = NULL;
size_t syscall_cnt = 0;

void syscall_dud() { /* do nothing */ }

void syscall_gate()
{
	uint64_t service_idx;
	asm volatile("mov %%rax, %0" : "=m" (service_idx));

	if(service_idx > syscall_cnt)
		return;
	syscall_table[service_idx]();
}


void cpu_interrupt_init()
{
	init_idt();

	// set up system calls interrupt
	cpu_interrupt_set_gate(syscall_gate, CPU_SYSCALL_GATE, CPU_INT_TYPE_INTERRUPT);
}

int cpu_interrupt_set_gate(void (*func)(), uint32_t slot, uint8_t type)
{
	uint8_t type_attributes = X86_IDT_PRESENT;

	switch(type)
	{
		case CPU_INT_TYPE_INTERRUPT: 	type_attributes |= X86_IDT_GATE_TYPE_INT32; break;
		case CPU_INT_TYPE_TRAP: 	type_attributes |= X86_IDT_GATE_TYPE_TRAP32; break;
		case CPU_INT_TYPE_TASK:		type_attributes |= X86_IDT_GATE_TYPE_TASK; break;
	}

	return set_idt_func_call(func, slot, type_attributes);
}


int cpu_interrupt_set_service(syscall_func_t func, size_t service_idx)
{
	if(service_idx >= CPU_SYSCALL_MAX)
		return 0;

	if(service_idx > syscall_cnt){
		syscall_table = krealloc(syscall_table, service_idx + 1);
		// fill newly allocated space with duds
		for(size_t i = syscall_cnt; i < service_idx; ++i)
			syscall_table[i] = syscall_dud;
		syscall_cnt = service_idx + 1;
	}

	syscall_table[service_idx] = func;

	return 1;
}
