#ifndef x86_TSS_H
#define x86_TSS_H

#ifdef CPU_I386

typedef struct{
	uint16_t link; uint16_t resv0;
	uint32_t esp0;
	uint16_t ss0; uint16_t resv1;
	uint32_t esp1;
	uint16_t ss1; uint16_t resv2;
	uint32_t esp2;
	uint16_t ss2; uint16_t resv3;
	uint32_t cr3;

	uint32_t eip;
	uint32_t eflags;
	uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
	uint16_t es; uint16_t resv4;
	uint16_t cs; uint16_t resv5;
	uint16_t ss; uint16_t resv6;
	uint16_t ds; uint16_t resv7;
	uint16_t fs; uint16_t resv8;
	uint16_t gs; uint16_t resv9;
	uint16_t ldtr; uint16_t resv10;
	uint16_t resv11; uint16_t iopb_off;
} tss_entry;

#endif

#endif
