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
} __attribute__((packed)) idt_entry;

#define X86_IDT_GET_OFFSET(idt_ent) ( (idt_ent).offset_lo | ((uint32_t)(idt_ent).offset_hi << 16) )
#define X86_IDT_SET_OFFSET(idt_ent, offset)\
{\
	(idt_ent).offset_lo = (offset) & 0xFFFF;\
	(idt_ent).offset_hi = ((offset) >> 16) & 0xFFFF;\
}

#endif

#define IDT_VECTOR_SIZE 256
idt_entry idt_main[IDT_VECTOR_SIZE];


#define X86_IDT_GATE_TYPE_TASK			0x5		// task gate (offset is unused, set to 0)
#define X86_IDT_GATE_TYPE_INT16			0x6		// 16-bit interrupt gate
#define X86_IDT_GATE_TYPE_TRAP16		0x7		// 16-bit trap gate
#define X86_IDT_GATE_TYPE_INT32			0xE		// 32-bit interrupt gate
#define X86_IDT_GATE_TYPE_TRAP32		0xF		// 32-bit trap gate

#define X86_IDT_PRIVELEGE_KERNEL		(0x0 << 5)
#define X86_IDT_PRIVELEGE_USER1			(0x1 << 5)
#define X86_IDT_PRIVELEGE_USER2			(0x2 << 5)
#define X86_IDT_PRIVELEGE_USER3			(0x3 << 5)

#define X86_IDT_PRESENT				(0x1 << 7)	// present bit


/* Zeroes the main IDT and sets IDTR (IDT register) to proper values.
*/
void init_idt();

/* Return values:
 * 0 - gate index out of range
 * 1 - OK
*/
int set_idt_gate(void (*func)(), uint32_t gate, uint8_t type_attributes);

#endif
