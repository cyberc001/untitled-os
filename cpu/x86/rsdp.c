#include "rsdp.h"
#include "string.h"

#define RSDP_SEARCH_START	((void*)0xE0000)
#define RSDP_SEARCH_END   	((void*)0xFFFFF)

static uint8_t calc_chksum(void* bytes, size_t ln)
{
	uint8_t sum = 0;
	for(size_t i = 0; i < ln; ++i)
		sum += ((uint8_t*)bytes)[i];
	return sum;
}

void map_rsdp(int (*map_phys)(void*, void*, uint64_t, int), uint64_t mem_unit_size)
{
	void* rsdp_aligned = RSDP_SEARCH_START - (uintptr_t)RSDP_SEARCH_START % mem_unit_size;
	size_t rsdp_sz = (RSDP_SEARCH_END - RSDP_SEARCH_START) + (uintptr_t)RSDP_SEARCH_START % mem_unit_size;
	map_phys(rsdp_aligned, rsdp_aligned, (rsdp_sz + (mem_unit_size - 1)) / mem_unit_size, 0);
}

rsdp* find_rsdp(uint64_t mem_unit_size)
{
	void* rsdp_aligned = RSDP_SEARCH_START - (uintptr_t)RSDP_SEARCH_START % mem_unit_size;
	size_t rsdp_sz = (RSDP_SEARCH_END - RSDP_SEARCH_START) + (uintptr_t)RSDP_SEARCH_START % mem_unit_size;
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
