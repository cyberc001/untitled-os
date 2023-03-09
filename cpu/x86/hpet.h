#ifndef X86_HPET_H
#define X86_HPET_H

#include <stdint.h>
#include <stddef.h>
#include "bits.h"

// refer to https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/software-developers-hpet-spec-1-0a.pdf
// page 30 for more information

#define HPET_PTR_REG(base_addr, reg) (uint64_t*)((base_addr) + (reg))
#define HPET_READ_REG(base_addr, reg) (*(HPET_PTR_REG(base_addr, reg)))

// For getting register values use HPET_READ_REG.
// For setting register values (ex. HPET_SET_ENABLE_CNF) use HPET_PTR_REG.

#define HPET_GENREG_CAP_ID 			0x0
	#define HPET_COUNTER_CLK_PERIOD(reg) 	GET_BITS((reg), 32, 63)
	#define HPET_COUNT_SIZE_CAP(reg)		((reg) & 0x2000)
#define HPET_GENREG_CONF			0x10
	#define HPET_SET_ENABLE_CNF(reg)		{*(reg) |= 0x1;}
#define HPET_GENREG_INT_STATUS		0x20
#define HPET_GENREG_COUNTER			0xF0

#define HPET_TIMREG_CONF_CAP(tno)		(0x100 + 0x20 * (tno))
#define HPET_TIMREG_COMP(tno)			(0x108 + 0x20 * (tno))
#define HPET_TIMREG_INT_ROUTE(tno)		(0x110 + 0x20 * (tno))

typedef struct {
	char sig[4];
	uint32_t length;
	uint8_t revision;
	uint8_t checksum;

	char oem_id[6];
	char oem_table_id[8];
	char oem_revision[4];
	char creator_id[4];
	char creator_revision[4];

	uint32_t et_block_id; // et == event timer
	struct {
		uint8_t addr_space_id;	// 0 - system memory, 1 - system I/O
		uint8_t reg_bit_width;
		uint8_t reg_bit_offset;
		uint8_t resv;
		uint64_t addr;
	} __attribute__((packed)) base_addr;
	uint8_t hpet_num;
	uint16_t min_clock_tick;
	uint8_t attributes;
} __attribute__((packed)) hpet_desc_table;

int hpet_init(int (*map_phys)(void*, void*, uint64_t, int), uint64_t mem_unit_size);

/* cnt_out should be a valid pointer. */
hpet_desc_table** hpet_get_timer_blocks(size_t* cnt_out);

#endif
