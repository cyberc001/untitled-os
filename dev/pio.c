#include "pio.h"

void pio_setup_drive(ata_drive* drive)
{
	ata_soft_reset(drive);
	drive->read = &pio_read;
	drive->write = &pio_write;
}


size_t pio_read(ata_drive* drive, uint64_t lba, size_t count, void* buf)
{
	// working only with LBA
	ata_select_drive(drive, drive->mode);

	// send a zero byte to error register
	cpu_out8(drive->base + ATA_REG_ERR, 0);

	// send parameters: sector count and LBA
	if(drive->mode == ATA_MODE_LBA48){
		cpu_out8(drive->base + ATA_REG_SECCOUNT0, (count >> 8) & 0xFF);
		cpu_out8(drive->base + ATA_REG_LBA0, lba >> (8 * 3) & 0xFF);
		cpu_out8(drive->base + ATA_REG_LBA1, lba >> (8 * 4) & 0xFF);
		cpu_out8(drive->base + ATA_REG_LBA2, lba >> (8 * 5) & 0xFF);
	}

	// TODO: wtf with LBA3-5 ?
	cpu_out8(drive->base + ATA_REG_SECCOUNT0, count & 0xFF);
	cpu_out8(drive->base + ATA_REG_LBA0, lba >> (8 * 0) & 0xFF);
	cpu_out8(drive->base + ATA_REG_LBA1, lba >> (8 * 1) & 0xFF);
	cpu_out8(drive->base + ATA_REG_LBA2, lba >> (8 * 2) & 0xFF);

	// send read command
	ata_wait(drive);
	cpu_out8(drive->base + ATA_REG_CMD,
		 drive->mode == ATA_MODE_LBA48 ? ATA_CMD_READ_SECTORS_EXT : ATA_CMD_READ_SECTORS);
	ata_wait(drive);

	uint16_t* buf16 = (uint16_t*)buf;
	size_t read_size = count;
	while(count--)
	{
		if(ata_poll(drive))
			return read_size - count;

		for(size_t i = 0; i < ATA_SECTOR_SIZE / 2; ++i){
			buf16[i] = cpu_in16(drive->base + ATA_REG_DATA);
		}
		buf16 += ATA_SECTOR_SIZE / 2;
	}

	return read_size;
}

size_t pio_write(ata_drive* drive, uint64_t lba, size_t count, void* buf)
{
	// working only with LBA
	ata_select_drive(drive, drive->mode);

	// send a zero byte to error register
	cpu_out8(drive->base + ATA_REG_ERR, 0);

	// send parameters: sector count and LBA
	// TODO: choose between LBA48 and LBA24
	cpu_out8(drive->base + ATA_REG_SECCOUNT0, (count >> 8) & 0xFF);
	cpu_out8(drive->base + ATA_REG_LBA0, (lba >> (8 * 3)) & 0xFF);
	cpu_out8(drive->base + ATA_REG_LBA1, (lba >> (8 * 4)) & 0xFF);
	cpu_out8(drive->base + ATA_REG_LBA2, (lba >> (8 * 5)) & 0xFF);
	cpu_out8(drive->base + ATA_REG_SECCOUNT0, count & 0xFF);
	cpu_out8(drive->base + ATA_REG_LBA0, (lba >> (8 * 0)) & 0xFF);
	cpu_out8(drive->base + ATA_REG_LBA1, (lba >> (8 * 1)) & 0xFF);
	cpu_out8(drive->base + ATA_REG_LBA2, (lba >> (8 * 2)) & 0xFF);

	// send write command
	ata_wait(drive);
	cpu_out8(drive->base + ATA_REG_CMD,
		 drive->mode == ATA_MODE_LBA48 ? ATA_CMD_WRITE_SECTORS_EXT : ATA_CMD_WRITE_SECTORS);

	uint16_t* buf16 = (uint16_t*)buf;
	size_t _count = count;
	while(_count--)
	{
		if(ata_poll(drive))
			return count - _count;

		for(size_t i = 0; i < ATA_SECTOR_SIZE / 2; ++i)
			cpu_out16(drive->base + ATA_REG_DATA, buf16[i]);

		buf16 += ATA_SECTOR_SIZE / 2;
	}

	if(ata_poll(drive))
		return 0;
	cpu_out8(drive->base + ATA_REG_CMD, ATA_CMD_CACHE_FLUSH);
	if(ata_poll(drive))
		return 0;

	return count;
}
