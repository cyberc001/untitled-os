#ifndef X86_APIC_H
#define X86_APIC_H

#include <stddef.h>
#include <stdint.h>

#define APIC_REG_SIZE		0x400	// total size of APIC registers.

#define LAPIC_REG_ICR0		0x300
#define LAPIC_REG_ICR1		0x310
#define LAPIC_REG_EOI		0xB0

#define LAPIC_IPI_INIT		0x4500
#define LAPIC_IPI_STARTUP	0x4600

/* Returns 0 if LAPIC is not supported, 1 otherwise. */
int apic_check();

void apic_set_base(void* addr);
void* apic_get_base();

uint32_t lapic_read(uint32_t reg);
void lapic_write(uint32_t reg, uint32_t val);


/* APIC timer */
#define APIC_TIMER_ONESHOT		0
#define APIC_TIMER_PERIODIC		1
void apic_set_timer(int type, size_t us, uint8_t int_gate);

#endif

