#ifndef X86_APIC_H
#define X86_APIC_H

#include <stddef.h>
#include <stdint.h>

#define APIC_BASE				0xFEE00000

#define APIC_REG_SIZE			0x400	// total size of APIC registers.

#define LAPIC_REG_ID			0x20
#define LAPIC_REG_EOI			0xB0
#define LAPIC_REG_SPURIOUS_INT	0xF0
#define LAPIC_REG_ICR0			0x300
#define LAPIC_REG_ICR1			0x310
#define LAPIC_REG_LINT0			0x350
#define LAPIC_REG_LINT1			0x360

#define LAPIC_IPI_INIT			0x4500
#define LAPIC_IPI_STARTUP		0x4600

/* Returns 0 if LAPIC is not supported, 1 otherwise. */
int apic_check();

uint32_t lapic_read(uint32_t reg);
void lapic_write(uint32_t reg, uint32_t val);

void apic_enable_spurious_ints(); // enables spurious interrupts on local APIC

/* APIC timer */
void apic_timer_init();
#define APIC_TIMER_ONESHOT		0
#define APIC_TIMER_PERIODIC		1
void apic_set_timer(int type, size_t ms, uint8_t int_gate); // !! precision is up to 10ms !!

#endif

