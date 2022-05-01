#ifndef SMP_TRAMPOLINE
#define SMP_TRAMPOLINE

#include <stdint.h>

#define SMP_TRAMP_DATA_OFFSET	0xA00

uint8_t smp_trampoline_start[1];
uint8_t smp_trampoline_end[1];

typedef struct {
	/* x86 structures, loaded after enabling long mode */
	struct {
		uint16_t size;
		uint64_t offset;
	} __attribute__((packed)) idt_ptr;
	struct {
		uint16_t limit;
		uint64_t base;
	} __attribute__((packed)) gdt_ptr;
	uint32_t page_table;

	/* boot report flag: should be initialized with 0, atomically set to 1 by trampoline when AP boots */
	uint8_t boot_flag;

	/* temporary data for real mode below: do not modify */
} __attribute__((packed)) smp_trampoline_data;

#endif
