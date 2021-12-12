#include "idt.h"

#include <stddef.h>

#ifdef CPU_32BIT

static struct {
	uint16_t size;
	uint32_t offset;
} _idt_ptr;

#endif


void init_idt()
{
	_idt_ptr.size = sizeof(idt_main) - 1;
	#ifdef CPU_32BIT
	_idt_ptr.offset = (uint32_t)idt_main;
	#endif

	for(size_t i = 0; i < sizeof(idt_main) / sizeof(idt_entry); ++i)
		idt_main[i] = (idt_entry){0};

	asm volatile ("lidt (%%eax)" :: "a"(&_idt_ptr));
}

int set_idt_gate(void (*func)(), uint32_t gate, uint8_t type_attributes)
{
	if(gate >= sizeof(idt_main) / sizeof(idt_entry))
		return 0;

	#ifdef CPU_32BIT
	uint32_t offset = (uint32_t)func;
	#endif

	X86_IDT_SET_OFFSET(idt_main[gate], offset);
	idt_main[gate].seg_select = 0x8; // GDT flat model code selector
	idt_main[gate].type_attributes = type_attributes;
	return 1;
}
