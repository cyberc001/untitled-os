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
	//lapic_write(0xD0, 0xDEADDEAD);
	//void* new_apic_base = kmalloc_align(APIC_REG_SIZE, 0x1000);
	//memcpy(new_apic_base, apic_get_base(), APIC_REG_SIZE);
	//apic_set_base(new_apic_base);
	//lapic_write(0xD0, 0xBEEFBEEF);

	pic_remap_irqs(0x20, 0x28);
	gdt_init();

	return 0;
}
