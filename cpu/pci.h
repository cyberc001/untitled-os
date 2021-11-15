#ifndef CPU_PCI_H
#define CPU_PCI_H

#include "cpu_io.h"

#include <stddef.h>

#define PCI_CONFIG_ADDRESS	0x00
#define PCI_CONFIG_DATA		0x04

typedef union{
	struct {
		uint8_t reg	: 8;
		uint8_t func	: 3;
		uint8_t dev	: 5;
		uint8_t bus	: 8;
		uint8_t rvs	: 7;
		uint8_t enable	: 1;
	} field;
	uint32_t raw;
} pci_address;

typedef struct{
	uint8_t bus;
	uint8_t dev;
	uint8_t func;
} pci_device;


void pci_setup(); // platform-dependant


// Public interface:

// should be called to initialize basic PCI port address
void pci_set_ioaddr(uint32_t addr);

size_t pci_scan_for_device(uint8_t _class, uint8_t _subclass, pci_device* devices, size_t device_cnt);

uint32_t pci_read_bar(pci_device* device, uint8_t id);

#endif
