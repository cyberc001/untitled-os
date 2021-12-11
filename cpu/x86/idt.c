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
