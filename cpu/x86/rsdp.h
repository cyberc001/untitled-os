#ifndef X86_RSDP_H
#define X86_RSDP_H

#include <stddef.h>
#include <stdint.h>

#define RSDP_GET_PTR(_rsdp) ((void*)((_rsdp)->revision < 2 ? (uintptr_t)(_rsdp)->rsdt_addr : (_rsdp)->xsdt_addr))

typedef struct rsdp rsdp;
struct rsdp{
	char sig[8];
	uint8_t checksum;
	char oem_id[6];
	uint8_t revision;
	uint32_t rsdt_addr;
	uint32_t length;
	uint64_t xsdt_addr;
	uint8_t ext_checksum;
	uint8_t resv[3];
};

void map_rsdp(int (*map_phys)(void*, void*, uint64_t, int), uint64_t mem_unit_size); // called in kernel_main() during identity mapping
rsdp* find_rsdp(uint64_t mem_unit_size);

#endif
