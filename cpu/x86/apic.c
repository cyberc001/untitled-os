#include "apic.h"

#include "cpuid.h"
#include "cpu/cpu_io.h"
#include "pit.h"

#define APIC_BASE_MSR			0x1B
#define APIC_BASE_MSR_ENABLE	0x800

int apic_check()
{
	uint32_t eax, ebx, ecx, edx;
	// try to get feature information
	if(!cpuid(0x1, 0x0, &eax, &ebx, &ecx, &edx)) // leaf 0x1 is not supported, can't obtain info
		return 0;
	if(!(edx & CPUID_FEAT_EDX_APIC))
		return 0;
	return 1;
}

void apic_set_base(void* addr)
{
	uint64_t apic_base = cpu_in_msr(0x1B);
	cpu_out_msr(APIC_BASE_MSR, (((uint64_t)addr & 0xFFFFFF000) | (apic_base & 0xFFF)));
}
void* apic_get_base()
{
	return (void*)(cpu_in_msr(0x1B) & 0xFFFFFF000);
}

uint32_t lapic_read(uint32_t reg)
{
	return *(volatile uint32_t *)((uintptr_t)apic_get_base() + reg);
}
void lapic_write(uint32_t reg, uint32_t val)
{
	*(volatile uint32_t *)((uintptr_t)apic_get_base() + reg) = val;
}

void apic_enable_spurious_ints()
{
	lapic_write(LAPIC_REG_SPURIOUS_INT, lapic_read(LAPIC_REG_SPURIOUS_INT) | 0x100);
}


/* APIC timer */
#define APIC_REG_TIMER_LVT			0x320
/* Divide configurations (LSB):
	0000	/2
	0001	/4
	0010	/8
	0011	/16
	1000	/32
	1001	/64
	1010	/128
	1011	/1
*/
#define APIC_REG_TIMER_DIV			0x3E0
#define APIC_REG_TIMER_INIT_COUNT	0x380
#define APIC_REG_TIMER_CUR_COUNT	0x390

/* Universally used parameters for APIC timer interface */
#define APIC_TIMER_DIV				0x2		// :8
#define APIC_TIMER_MEASURE			10		// amount of ms to measure

void apic_set_timer(int type, size_t us, uint8_t int_gate)
{
	static uint32_t ticks_per_us = 0;
	if(!ticks_per_us){ // measure amount of ticks per certain period of time
		lapic_write(APIC_REG_TIMER_DIV, APIC_TIMER_DIV);
		lapic_write(APIC_REG_TIMER_INIT_COUNT, 0xFFFFFFFF); // set initial counter to -1
		pit_sleep_ms(APIC_TIMER_MEASURE);
		lapic_write(APIC_REG_TIMER_LVT, 0x10000); // mask the timer to stop it
		uint32_t tick_amt = 0xFFFFFFFF - lapic_read(APIC_REG_TIMER_CUR_COUNT);
		ticks_per_us = tick_amt / 10000;
	}
	// start the requested timer
	lapic_write(APIC_REG_TIMER_DIV, APIC_TIMER_DIV);
	lapic_write(APIC_REG_TIMER_INIT_COUNT, ticks_per_us * us);
	lapic_write(APIC_REG_TIMER_LVT, (type << 17) | int_gate);

	//for(;;) uart_printf("%u\r\n", lapic_read(APIC_REG_TIMER_CUR_COUNT));
}

