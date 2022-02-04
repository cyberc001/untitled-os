#include "gdt.h"
#include <stddef.h>

static gdt_desc gdt[GDT_MAX_DESCS];
static gdt_ptr gdt_main;
static uint16_t gdt_last_ind;

void gdt_init()
{
	gdt_main.limit = (sizeof(gdt_desc) * GDT_MAX_DESCS) - 1;
	gdt_main.base = (uintptr_t)&gdt[0];

	gdt_add_desc(0, 0, 0, 0);
	gdt_add_desc(0, 0, GDT_BASIC_DESC | GDT_DESC_EXECUTABLE, GDT_BASIC_GRANULARITY);
	gdt_add_desc(0, 0, GDT_BASIC_DESC, GDT_BASIC_GRANULARITY);
	gdt_add_desc(0, 0, GDT_BASIC_DESC | GDT_DESC_DPL, GDT_BASIC_GRANULARITY);
	gdt_add_desc(0, 0, GDT_BASIC_DESC | GDT_DESC_DPL | GDT_DESC_EXECUTABLE, GDT_BASIC_GRANULARITY);
	gdt_add_desc(0, 0, 0, 0);

	gdt_reload(&gdt_main, GDT_OFFSET_KERNEL_CODE, GDT_OFFSET_KERNEL_DATA);
}

int gdt_add_desc(uint64_t base, uint16_t limit, uint8_t access, uint8_t granularity)
{
	if(gdt_last_ind >= GDT_MAX_DESCS)
		return 0;

	gdt[gdt_last_ind].base_low = base & 0xFFFF;
	gdt[gdt_last_ind].base_mid = (base >> 16) & 0xFF;
	gdt[gdt_last_ind].base_high = (base >> 24) & 0xFF;

	gdt[gdt_last_ind].limit = limit;

	gdt[gdt_last_ind].flags = access;
	gdt[gdt_last_ind].granularity = granularity;

	gdt_last_ind++;
	return 1;
}

#define TSS_SIZE 0x70

uint16_t gdt_install_tss(uint64_t tss)
{
	uint8_t tss_type = GDT_DESC_ACCESS | GDT_DESC_EXECUTABLE | GDT_DESC_PRESENT;

	gdt_tss_desc* tss_desc = (gdt_tss_desc*)&gdt[gdt_last_ind];

	if(gdt_last_ind >= GDT_MAX_DESCS)
        	return 0;

	tss_desc->limit0 = TSS_SIZE & 0xFFFF;
	tss_desc->addr0 = tss & 0xFFFF;
	tss_desc->addr1 = (tss & 0xFF0000) >> 16;
	tss_desc->type0 = tss_type;
	tss_desc->limit1 = (TSS_SIZE & 0xF0000) >> 16;
	tss_desc->addr2 = (tss & 0xFF000000) >> 24;
	tss_desc->addr3 = tss >> 32;
	tss_desc->resv0 = 0;

	gdt_last_ind += 2;
	return (gdt_last_ind - 2) * GDT_DESC_SIZE;
}
