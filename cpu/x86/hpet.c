#include "hpet.h"
#include "rsdp.h"
#include "string.h"
#include "kernlib/kernmem.h"

static hpet_desc_table** hpet_timer_blocks = NULL;
static size_t hpet_timer_blocks_cnt = 0;

int hpet_init(int (*map_phys)(void*, void*, uint64_t, int), uint64_t mem_unit_size)
{
	rsdp* _rsdp = find_rsdp(mem_unit_size);
	uint8_t* rsdt = RSDP_GET_PTR(_rsdp);

	void* rsdt_aligned = rsdt - (uintptr_t)rsdt % mem_unit_size;
	map_phys(rsdt_aligned, rsdt_aligned, 1, 0);

	uint8_t *ent, *ent_end;
	uint32_t ln;
	for(ln = *((uint32_t*)(rsdt + 4)) /*uint32_t length field*/, ent_end = rsdt + 36;
		ent_end < rsdt + ln;
		ent_end += rsdt[0] == 'X' ? 8 : 4){
		ent = (uint8_t*)(rsdt[0] == 'X' ? *((uint64_t*)ent_end) : *((uint32_t*)ent_end)); // pointer to XSDT is 8 bytes, pointer to RSDT is 4 bytes
		if(!memcmp(ent, "HPET", 4)){
			hpet_desc_table* table = (hpet_desc_table*)ent;
			hpet_timer_blocks = krealloc(hpet_timer_blocks, (++hpet_timer_blocks_cnt) * sizeof(hpet_desc_table*));
			hpet_timer_blocks[hpet_timer_blocks_cnt - 1] = table;

			void* base_addr_aligned = (void*)table->base_addr.addr - (uintptr_t)table->base_addr.addr % mem_unit_size;
			map_phys(base_addr_aligned, base_addr_aligned, 1, 0);

			ent += table->length;
		}
	}

	return 0;
}

hpet_desc_table** hpet_get_timer_blocks(size_t* cnt_out)
{
	*cnt_out = hpet_timer_blocks_cnt;
	return hpet_timer_blocks;
}
