#include "idt.h"
#include "isr.h"

#include "../../dev/uart.h"
#include <stddef.h>

#ifdef CPU_32BIT

static struct {
	uint16_t size;
	uint32_t offset;
} __attribute__((packed)) _idt_ptr;

#endif


void init_idt()
{
	_idt_ptr.size = sizeof(idt_entry) * IDT_VECTOR_SIZE - 1;
	#ifdef CPU_32BIT
	_idt_ptr.offset = (uint32_t)idt_main;
	#endif

	for(size_t i = 0; i < IDT_VECTOR_SIZE; ++i)
		idt_main[i] = (idt_entry){0, 0, 0, 0, 0};

	for(size_t i = 0; i < ISR_STUB_TABLE_SIZE; ++i){
		set_idt_gate((void (*)())isr_stub_table[i], i, 0x8E);
	}

	asm volatile ("lidt %0" :: "m"(_idt_ptr));
}

int set_idt_gate(void (*func)(), uint32_t gate, uint8_t type_attributes)
{
	if(gate >= IDT_VECTOR_SIZE)
		return 0;

	#ifdef CPU_32BIT
	uint32_t offset = (uint32_t)func;
	#endif

	X86_IDT_SET_OFFSET(idt_main[gate], offset);
	idt_main[gate].seg_select = 0x8; // GDT flat model code selector
	idt_main[gate].type_attributes = type_attributes;
	return 1;
}
