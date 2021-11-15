#ifndef PIO_H
#define PIO_H

#include "ata.h"
#include "../cpu/pci.h"


// For wiring all operation functions in an ata drive structure.
void pio_setup_drive(ata_drive* drive);

// For return values check ata_drive structure in ata.h.
size_t pio_read(ata_drive* drive, uint64_t lba, size_t count, void* buf);
size_t pio_write(ata_drive* drive, uint64_t lba, size_t count, void* buf);


#endif
