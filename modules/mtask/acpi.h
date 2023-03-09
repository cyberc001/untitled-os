#ifndef ACPI_H
#define ACPI_H

#include <stdint.h>

uint8_t detect_cpus(uint8_t* rsdt, uint8_t* lapic_ids, uint8_t* bsp_lapic_id);

#endif
