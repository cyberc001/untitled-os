#include "apic.h"

#include "cpuid.h"
#include "cpu/cpu_io.h"

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
