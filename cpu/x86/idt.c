#include "idt.h"
#include "isr.h"

#include "../../dev/uart.h"
#include <stddef.h>

static struct {
	uint16_t size;
	uint64_t offset;
} __attribute__((packed)) _idt_ptr;

void init_idt()
{
	_idt_ptr.size = sizeof(idt_entry) * IDT_VECTOR_SIZE - 1;
	_idt_ptr.offset = (uint64_t)idt_main;

	for(size_t i = 0; i < IDT_VECTOR_SIZE; ++i)
		idt_main[i] = (idt_entry){0, 0, 0, 0, 0, 0, 0};

	// load ISR stubs for CPU exceptions and IRQs
	for(size_t i = 0; i < ISR_STUB_TABLE_SIZE; ++i){
		set_idt_gate((void*)isr_stub_table[i], i, 0x8E);
	}

	asm volatile ("lidt %0" :: "m"(_idt_ptr));
}

int set_idt_gate(void* val, uint64_t gate, uint8_t type_attributes)
{
	if(gate >= IDT_VECTOR_SIZE)
		return 0;

	uint64_t offset = (uint64_t)val;

	X86_IDT_SET_OFFSET(idt_main[gate], offset);
	idt_main[gate].seg_select = 0x28;
	idt_main[gate].type_attributes = type_attributes;
	idt_main[gate].ist = 1;
	return 1;
}
