#ifndef X86_GDT_H
#define X86_GDT_H

#include <stdint.h>

#define GDT_MAX_DESCS		0x2000
#define GDT_DESC_SIZE		0x08

#define GDT_DESC_ACCESS		0x01
#define GDT_DESC_READWRITE	0x02
#define GDT_DESC_DC		0x04
#define GDT_DESC_EXECUTABLE	0x08
#define GDT_DESC_CODE_DATA	0x10
#define GDT_DESC_DPL		0x60
#define GDT_DESC_PRESENT	0x80

#define GDT_GRANULARITY_OS	0x10
#define GDT_GRANULARITY_X64	0x20
#define GDT_GRANULARITY_X32	0x40
#define GDT_GRANULARITY_4K	0x80

#define GDT_BASIC_DESC		(GDT_DESC_PRESENT | GDT_DESC_READWRITE | GDT_DESC_CODE_DATA)
#define GDT_BASIC_GRANULARITY	(GDT_GRANULARITY_X64 | GDT_GRANULARITY_4K)

typedef struct
{
	uint16_t limit;
	uint16_t base_low;
	uint8_t base_mid;
	uint8_t flags;
	uint8_t granularity;
	uint8_t  base_high;
} __attribute__((packed)) gdt_desc;

typedef struct
{
	uint16_t limit0;
	uint16_t addr0;
	uint8_t addr1;
	uint8_t type0;
	uint8_t limit1;
	uint8_t addr2;
	uint32_t addr3;
	uint32_t resv0;
} __attribute__((packed)) gdt_tss_desc;

typedef struct {
    uint16_t limit;
    uintptr_t base;
} __attribute__((packed)) gdt_ptr;

int gdt_add_desc(uint64_t base, uint16_t limit, uint8_t access, uint8_t granularity);
void gdt_reload(gdt_ptr* gdtr, uint16_t code, uint16_t data);
uint16_t gdt_install_tss(uint64_t tss);
void gdt_init();

#endif
