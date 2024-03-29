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

	/* TSS segment number */
	uint16_t tss;

	/* APIC timer set-up function */
	uint64_t ap_timer_set_func;

	/* pointer to pointer to a location to jump to after boot. The pointer should be a valid pointer before booting an AP. The pointer-value should be initialized with 0, and atomically set to a non-zero value after AP has booted */
	uint64_t jmp_loc;
	/* pointer to memory where thread will jump in absence of any running code (halt in the end of AP boot sequence). Written to by AP initialization sequence. Should point to a valid location. */
	uint64_t no_code_fallback_jmp;

	/* temporary data for real mode below: do not modify */
} __attribute__((packed)) smp_trampoline_data;

#endif
