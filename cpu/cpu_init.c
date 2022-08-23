#include "cpu_init.h"

#include "x86/pic.h"
#include "x86/gdt.h"

#include "x86/cpuid.h"
#include "x86/apic.h"

#include "kernlib/kernmem.h"
#include "string.h"

int cpu_init()
{
	// TODO add proper arch #ifdefs
	if(!cpuid_check())
		return CPU_INITERR_NOCPUID;
	if(!apic_check())
		return CPU_INITERR_NOAPIC;

	apic_timer_init();
	pic_remap_irqs(0x20, 0x28);
	gdt_init();

	return 0;
}
