#include "acpi.h"
#include "string.h"

#include "cpu/x86/cpuid.h"
#include "dev/uart.h"

#define RSDP_SEARCH_START	((void*)0xE0000)
#define RSDP_SEARCH_END   	((void*)0xFFFFF)

extern int map_phys(void* vaddr, void* paddr, uint64_t usize, int flags);
extern uint64_t get_mem_unit_size();

static uint8_t calc_chksum(void* bytes, size_t ln)
{
	uint8_t sum = 0;
	for(size_t i = 0; i < ln; ++i)
		sum += ((uint8_t*)bytes)[i];
	return sum;
}

rsdp* find_rsdp()
{
	void* rsdp_aligned = RSDP_SEARCH_START - (uintptr_t)RSDP_SEARCH_START % get_mem_unit_size();
	size_t rsdp_sz = (RSDP_SEARCH_END - RSDP_SEARCH_START) + (uintptr_t)RSDP_SEARCH_START % get_mem_unit_size();
	map_phys(rsdp_aligned, rsdp_aligned, (rsdp_sz + (get_mem_unit_size() - 1)) / get_mem_unit_size(), 0);

	for(void* ptr = RSDP_SEARCH_START; ptr < RSDP_SEARCH_END; ptr += 16){
		if(!memcmp("RSD PTR ", ptr, 8)){
			rsdp* _rsdp = ptr;
			if(_rsdp->revision == 0){
				if(calc_chksum(_rsdp, sizeof(rsdp) - 16))
					continue;
			}
			else if(_rsdp->revision == 2){
				if(calc_chksum(_rsdp, sizeof(rsdp)))
					continue;
			}
			else
				continue;
			return _rsdp;
		}
	}
	return NULL;
}

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
			// APIC consists of variable-length entries:
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
