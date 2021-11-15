#include "pci.h"

static uint32_t pci_ioaddr;	// basic PCI port address


void pci_setup()
{
	#ifdef CPU_I386
	pci_set_ioaddr(0xCF8);
	#endif
}


// Public interface:
void pci_set_ioaddr(uint32_t addr)
{
	pci_ioaddr = addr;
}

static inline uint32_t pci_read_dword(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg)
{
	pci_address addr;
	addr.raw = 0;

	addr.field.enable = 1;
	addr.field.bus = bus;
	addr.field.dev = dev;
	addr.field.func = func;
	addr.field.reg = reg & 0xFC;

	cpu_out32(pci_ioaddr + PCI_CONFIG_ADDRESS, addr.raw);
	return cpu_in32(pci_ioaddr + PCI_CONFIG_DATA);
}
static inline void pci_write_dword(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg, uint32_t val)
{
	pci_address addr;
	addr.raw = 0;

	addr.field.enable = 1;
	addr.field.bus = bus;
	addr.field.dev = dev;
	addr.field.func = func;
	addr.field.reg = reg & 0xFC;

	cpu_out32(pci_ioaddr + PCI_CONFIG_ADDRESS, addr.raw);
	cpu_out32(pci_ioaddr + PCI_CONFIG_DATA, val);
}

static inline uint16_t pci_get_vendor(uint8_t bus, uint8_t dev, uint8_t func)
{ return pci_read_dword(bus, dev, func, 0x00) & 0xFFFF; }

static inline uint16_t pci_get_device(uint8_t bus, uint8_t dev, uint8_t func)
{ return (pci_read_dword(bus, dev, func, 0x00) >> 16) & 0xFFFF; }

static inline uint8_t pci_get_class(uint8_t bus, uint8_t dev, uint8_t func)
{ return (pci_read_dword(bus, dev, func, 0x08) >> 24) & 0xFF; }

static inline uint8_t pci_get_subclass(uint8_t bus, uint8_t dev, uint8_t func)
{ return (pci_read_dword(bus, dev, func, 0x08) >> 16) & 0xFF; }

static inline uint8_t pci_get_header_type(uint8_t bus, uint8_t dev, uint8_t func)
{ return (pci_read_dword(bus, dev, func, 0x0C) >> 16) & 0xFF; }

static inline uint32_t pci_get_bar(uint8_t bus, uint8_t dev, uint8_t func, uint8_t id)
{ return pci_read_dword(bus, dev, func, 0x10 + 4 * id); }

// Public interface

// TODO: allow changing bus?
size_t pci_scan_for_device(uint8_t _class, uint8_t _subclass, pci_device* devices, size_t device_cnt)
{
	size_t i = 0;
	uint8_t bus = 0;

	for(uint8_t dev = 0; dev < 32; ++dev)
	{
		for(uint8_t func = 0; func < 8; ++func)
		{
			uint16_t vendor = pci_get_vendor(bus, dev, func);

			if(vendor != 0xFFFF){ // check for a float
				uint32_t dev_class = pci_get_class(bus, dev, func);
				uint32_t dev_subclass = pci_get_subclass(bus, dev, func);
				if(_class != dev_class || _subclass != dev_subclass)
					continue;

				devices[i].bus = bus;
				devices[i].dev = dev;
				devices[i].func = func;
				++i; --device_cnt;
				if(!device_cnt)
					return i;
			}
		}
	}

	return i;
}

uint32_t pci_read_bar(pci_device* device, uint8_t id)
{
	return pci_get_bar(device->bus, device->dev, device->func, id);
}
