#include "acpi.h"
#include "string.h"

#include "cpu/x86/cpuid.h"
#include "dev/uart.h"

extern int map_phys(void* vaddr, void* paddr, uint64_t usize, int flags);
extern uint64_t get_mem_unit_size();

uint8_t detect_cpus(uint8_t* rsdt, uint8_t* lapic_ids, uint8_t* bsp_lapic_id)
{
	uint8_t *ent, *ent_end;
	uint32_t ln;

	uint64_t lapic_ptr = 0, ioapic_ptr = 0;
	uint8_t core_num = 0;

	void* rsdt_aligned = rsdt - (uintptr_t)rsdt % get_mem_unit_size();
	size_t rsdt_sz = 4096 + (uintptr_t)rsdt % get_mem_unit_size();
	map_phys(rsdt_aligned, rsdt_aligned, (rsdt_sz + (get_mem_unit_size() - 1)) / get_mem_unit_size(), 0);

	for(ln = *((uint32_t*)(rsdt + 4)) /*uint32_t length field*/, ent_end = rsdt + 36;
		ent_end < rsdt + ln;
		ent_end += rsdt[0] == 'X' ? 8 : 4){
		ent = (uint8_t*)(rsdt[0] == 'X' ? *((uint64_t*)ent_end) : *((uint32_t*)ent_end)); // pointer to XSDT is 8 bytes, pointer to RSDT is 4 bytes
		if(!memcmp(ent, "APIC", 4)){
			lapic_ptr = (uint64_t)(*((uint32_t*)(ent + 0x24)));
			ent_end = ent + *((uint32_t*)(ent + 4));
			// MADT consists of variable-length entries (https://wiki.osdev.org/MADT):
			// ent[0] is entry type, ent[1] is entry length
			for(ent += 44; // skip the ACPI table header
					ent < ent_end; ent += ent[1]){ // iterate on APIC entries
				switch(ent[0]){
					case 0: // processor local APIC
						if(ent[4] & 1)
							lapic_ids[core_num++] = ent[3];
						break;
					case 1: // IOAPIC
						ioapic_ptr = (uint64_t)*((uint32_t*)(ent + 4));
						break;
					case 5: // LAPIC
						lapic_ptr = *((uint64_t*)(ent + 4));
						break;
				}
			}
			break;
		}
	}

	// get "initial APIC ID" (BSP APIC ID)
	uint32_t eax, ebx, ecx, edx;
	cpuid(1, 0, &eax, &ebx, &ecx, &edx);
	*bsp_lapic_id = ebx >> 24;

	return core_num;
}
