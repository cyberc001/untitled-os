#include "gdt.h"
#include <stddef.h>

static unsigned char gdt[GDT_MAX_SIZE];
static gdt_ptr gdt_main;
static uint32_t gdt_last_off = 0;

void gdt_init()
{
	gdt_main.limit = GDT_MAX_SIZE - 1;
	gdt_main.base = (uintptr_t)&gdt[0];

	uint32_t kernel_code_off, kernel_data_off;

	// mimic stivale2 descriptors (CPL = 0):
	gdt_add_desc(0, 0, 0, 0);
	gdt_add_desc(0, 0xFFFF, GDT_BASIC_DESC | GDT_DESC_EXECUTABLE, 0); // 16-bit code
	gdt_add_desc(0, 0xFFFF, GDT_BASIC_DESC, 0); // 16-bit data
	gdt_add_desc(0, 0xFFFF, GDT_BASIC_DESC | GDT_DESC_EXECUTABLE, GDT_GRANULARITY_X32 | GDT_GRANULARITY_4K); // 32-bit code
	gdt_add_desc(0, 0xFFFF, GDT_BASIC_DESC, GDT_GRANULARITY_X32 | GDT_GRANULARITY_4K); // 32-bit data
	kernel_code_off = gdt_add_desc(0, 0, GDT_BASIC_DESC | GDT_DESC_EXECUTABLE, GDT_BASIC_GRANULARITY); // 64-bit code
	kernel_data_off = gdt_add_desc(0, 0, GDT_BASIC_DESC, GDT_BASIC_GRANULARITY); // 64-bit data (USED BY IDT, SHOULD BE PRESENT AT FIXED LOCATION!)
	// descriptors for user-space (CPL = 3):
	gdt_add_desc(0, 0, GDT_BASIC_DESC | GDT_DESC_EXECUTABLE | GDT_DESC_DPL, GDT_BASIC_GRANULARITY); // 64-bit code
	gdt_add_desc(0, 0, GDT_BASIC_DESC | GDT_DESC_DPL, GDT_BASIC_GRANULARITY); // 64-bit data
	gdt_reload(&gdt_main, kernel_code_off, kernel_data_off);
}


uint32_t gdt_add_desc(uint64_t base, uint16_t limit, uint8_t access, uint8_t granularity)
{
	if(gdt_last_off >= GDT_MAX_SIZE - sizeof(gdt_desc) - 1)
		return (uint32_t)-1;

	gdt_desc* gdte = (gdt_desc*)(gdt + gdt_last_off);

	gdte->base_low = base & 0xFFFF;
	gdte->base_mid = (base >> 16) & 0xFF;
	gdte->base_high = (base >> 24) & 0xFF;

	gdte->limit = limit;

	gdte->flags = access;
	gdte->granularity = granularity;

	gdt_last_off += sizeof(gdt_desc);
	return gdt_last_off - sizeof(gdt_desc);
}

uint32_t gdt_add_tss_desc(uint64_t rsp[3], uint64_t ist[7])
{
	if(gdt_last_off >= GDT_MAX_SIZE - sizeof(gdt_tss_desc) - 1)
		return (uint32_t)-1;

	gdt_tss_desc* tssd = (gdt_tss_desc*)(gdt + gdt_last_off);

	for(size_t i = 0; i < 3; ++i)
		tssd->rsp[i] = rsp[i];
	for(size_t i = 0; i < 7; ++i)
		tssd->ist[i] = ist[i];

	tssd->resv0 = tssd->resv1 = tssd->resv2 = tssd->resv3 = 0;

	tssd->io_map = sizeof(gdt_tss_desc); // no I/O map

	gdt_last_off += sizeof(gdt_tss_desc);
	return gdt_last_off - sizeof(gdt_tss_desc);
}
