#ifndef X86_IDT_H
#define X86_IDT_H

#include <stdint.h>

#ifdef CPU_32BIT

typedef struct{
	uint16_t offset_lo;
	uint16_t seg_select;
	uint8_t resv0;			// unused, set to 0
	uint8_t type_attributes; 	// gate type, cpu privelege levels, present bit
	uint16_t offset_hi;
} idt_entry;

#endif

idt_entry idt_main[256];


#define X86_IDT_GATE_TYPE_TASK		0x5		// task gate (offset is unused, set to 0)
#define X86_IDT_GATE_TYPE_INT16		0x6		// 16-bit interrupt gate
#define X86_IDT_GATE_TYPE_TRAP16	0x7		// 16-bit trap gate
#define X86_IDT_GATE_TYPE_INT32		0xE		// 32-bit interrupt gate
#define X86_IDT_GATE_TYPE_TRAP32	0xF		// 32-bit trap gate

#define X86_IDT_PRIVELEGE_KERNEL	(0x0 << 5)
#define X86_IDT_PRIVELEGE_USER1		(0x1 << 5)
#define X86_IDT_PRIVELEGE_USER2		(0x2 << 5)
#define X86_IDT_PRIVELEGE_USER3		(0x3 << 5)

#define X86_IDT_PRESENT			(0x1 << 7)	// present bit


/* Zeroes the main IDT and sets IDTR (IDT register) to proper values.
*/
void init_idt();

#endif
