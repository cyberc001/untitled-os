#include "ata.h"

#include "../cstdlib/string.h"
#include "../cpu/cpu_io.h"

#include "../cpu/pci.h"

#include "pio.h"

void ata_wait(ata_drive* drive)
{
	for(size_t i = 0; i < 4; ++i)
		cpu_in8(drive->base + ATA_REG_STATUS);
}

int ata_poll(ata_drive* drive)
{
	ata_wait(drive);

	uint8_t status;
	while( (status = cpu_in8(drive->base + ATA_REG_STATUS)) & ATA_STATUS_BSY)
		;

	if( (status & ATA_STATUS_ERR) || (status & ATA_STATUS_DF) ){
		ata_wait(drive);
		uint8_t err = cpu_in8(drive->base + ATA_REG_ERR);
		return err;
	}
	if( !(status & ATA_STATUS_DRQ) ){
		return -1;
	}

	return 0;
}

void ata_soft_reset(ata_drive* drive)
{
	cpu_out8(drive->ctrl, ATA_CMD_RESET);
	ata_wait(drive);
	cpu_out8(drive->ctrl, 0);
}

void ata_select_drive(ata_drive* drive, uint32_t mode)
{
	static ata_drive* last_sel_drive = NULL;
	static uint32_t last_sel_mode = (uint32_t)-1;

	if(drive != last_sel_drive || mode != last_sel_mode)
	{
		cpu_out8(drive->base + ATA_REG_HDDEVSEL,
				(mode & ATA_MODE_LBA48 ? 0xE0 : 0xA0) | (drive->is_slave << 4) | (mode & 0xF));

		// TODO: will a for loop be better? or at least misc macro?
		ata_wait(drive); ata_wait(drive); ata_wait(drive); ata_wait(drive); ata_wait(drive);

		last_sel_drive = drive;
		last_sel_mode = mode;
	}
}


static void ata_detect_drive(ata_drive* drive)
{
	if(!drive->is_slave){
		ata_soft_reset(drive);
		ata_wait(drive);
	}
	ata_select_drive(drive, 0);
	ata_poll(drive);

	uint16_t type = cpu_in8(drive->base + ATA_REG_LBA1) | ((uint16_t)cpu_in8(drive->base + ATA_REG_LBA2) << 8);
	switch(type){
		case 0x0000: type = ATA_DEVICE_PATA; break;
		case 0xC33C: type = ATA_DEVICE_SATA; break;
		case 0xEB14: type = ATA_DEVICE_PATAPI; break;
		case 0x9669: type = ATA_DEVICE_SATAPI; break;
		default: drive->type = ATA_DEVICE_UNKNOWN; return;
	}

	drive->type = type;

	uint8_t identify_command = 0;
	if(type == ATA_DEVICE_PATAPI || type == ATA_DEVICE_SATAPI)
		identify_command = ATA_CMD_IDENTIFY_PACKET;
	else
		identify_command = ATA_CMD_IDENTIFY;

	char ide_ident[ATA_SECTOR_SIZE];
	memset(ide_ident, 0, sizeof(ide_ident));

	cpu_out8(drive->base + ATA_REG_CMD, identify_command);
	ata_poll(drive);

	for(size_t i = 0; i < ATA_SECTOR_SIZE; i += 2)
	{
		uint16_t _idnt = cpu_in16(drive->base + ATA_REG_DATA);
		ide_ident[i] = _idnt & 0xFFFF; // TODO: isn't 0xFFFF 2 bytes?
		ide_ident[i+1] = (_idnt >> 8) & 0xFFFF;
	}

	drive->sig = *(uint16_t*)(ide_ident + ATA_IDENT_DEVICETYPE);
	drive->cap = *(uint16_t*)(ide_ident + ATA_IDENT_CAPABILITIES);
	drive->cmd_sets = *(uint32_t*)(ide_ident + ATA_IDENT_COMMANDSETS);

	if(drive->cmd_sets & (1 << 26)){
		// supports 48-bit LBA
		uint32_t max_lba_hi = *(uint32_t*)(ide_ident + ATA_IDENT_MAX_LBA);
		uint32_t max_lba_lo = *(uint32_t*)(ide_ident + ATA_IDENT_MAX_LBA_EXT);
		drive->max_lba = ((uint64_t)max_lba_hi << 32) | max_lba_lo;
		drive->mode = ATA_MODE_LBA48;
	}
	else{
		drive->max_lba = *(uint32_t*)(ide_ident + ATA_IDENT_MAX_LBA);
		drive->mode = ATA_MODE_LBA28;
	}

	for(size_t i = 0; i < 40; i += 2){
		drive->model_name[i] = ide_ident[ATA_IDENT_MODEL + i + 1];
		drive->model_name[i + 1] = ide_ident[ATA_IDENT_MODEL + i];
	}
	drive->model_name[40] = '\0';
}

size_t ata_probe(ata_drive drives_out[4])
{
	pci_device ide;
	int count = pci_scan_for_device(0x01, 0x01, &ide, 1);

	if(!count)
		return 0;

	uint32_t base, ctrl;
	base = pci_read_bar(&ide, 0);
	ctrl = pci_read_bar(&ide, 1);

	if(base == 0 || base == 1)
		base = ATA_PIO_PRI_PORT_BASE;
	if(ctrl == 0 || ctrl == 1)
		ctrl = ATA_PIO_PRI_PORT_CONTROL;

	// first channel:
	// master device
	drives_out[0].id = 0;
	drives_out[0].base = base;
	drives_out[0].ctrl = ctrl;
	drives_out[0].is_slave = 0;
	// slave device
	drives_out[1].id = 1;
	drives_out[1].base = base;
	drives_out[1].ctrl = ctrl;
	drives_out[1].is_slave = 1;

	// second channel:
	base = pci_read_bar(&ide, 2);
	ctrl = pci_read_bar(&ide, 3);

	// master device
	drives_out[2].id = 2;
	drives_out[2].base = base;
	drives_out[2].ctrl = ctrl;
	drives_out[2].is_slave = 0;
	// slave device
	drives_out[3].id = 3;
	drives_out[3].base = base;
	drives_out[3].ctrl = ctrl;
	drives_out[3].is_slave = 1;

	size_t dev_detected = 0;
	for(size_t i = 0; i < 4; ++i)
	{
		ata_detect_drive(&drives_out[i]);

		switch(drives_out[i].type)
		{
			case ATA_DEVICE_PATA:
			pio_setup_drive(&drives_out[i]);
			++dev_detected;
			break;
		}
	}
	return dev_detected;
}
