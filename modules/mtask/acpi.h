#ifndef ACPI_H
#define ACPI_H

#include <stdint.h>

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

rsdp* find_rsdp();
uint8_t detect_cpus(uint8_t* rsdt, uint8_t* lapic_ids, uint8_t* bsp_lapic_id);

#endif
