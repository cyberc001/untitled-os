#ifndef ATA_H
#define ATA_H

#include <stdint.h>
#include <stddef.h>

#define ATA_SECTOR_SIZE		512

// ATA device IDs (used by drivers)
#define ATA_DEVICE_UNKNOWN	0x000
#define ATA_DEVICE_NOT_FOUND	0x001
#define ATA_DEVICE_PATA		0x002
#define ATA_DEVICE_SATA		0x003
#define ATA_DEVICE_PATAPI	0x004
#define ATA_DEVICE_SATAPI	0x005

// default PIO ports
#define ATA_PIO_PRI_PORT_BASE		0x1F0
#define ATA_PIO_PRI_PORT_CONTROL	0x3F6
#define ATA_PIO_SEC_PORT_BASE		0x170
#define ATA_PIO_SEC_PORT_CONTROL	0x376

// ATA registers
#define ATA_REG_DATA		0x00
#define ATA_REG_ERR		0x01
#define ATA_REG_FEATURES	0x01
#define ATA_REG_SECCOUNT0	0x02
#define ATA_REG_LBA0		0x03
#define ATA_REG_LBA1		0x04
#define ATA_REG_LBA2		0x05
#define ATA_REG_HDDEVSEL	0x06
#define ATA_REG_CMD		0x07
#define ATA_REG_STATUS		0x07
#define ATA_REG_SECCOUNT1	0x08
#define ATA_REG_LBA3		0x09
#define ATA_REG_LBA4		0x0A
#define ATA_REG_LBA5		0x0B
#define ATA_REG_CONTROL		0x0C
#define ATA_REG_ALTSTATUS	0x0C
#define ATA_REG_DEVADDRESS	0x0D

// ATA commands
#define ATA_CMD_RESET			0x04
#define ATA_CMD_READ_SECTORS		0x20
#define ATA_CMD_READ_SECTORS_EXT	0x24
#define ATA_CMD_WRITE_SECTORS		0x30
#define ATA_CMD_WRITE_SECTORS_EXT	0x34
#define ATA_CMD_READ_DMA		0xC8
#define ATA_CMD_READ_DMA_EXT		0x25
#define ATA_CMD_WRITE_DMA		0xCA
#define ATA_CMD_WRITE_DMA_EXT		0x35
#define ATA_CMD_CACHE_FLUSH		0xE7
#define ATA_CMD_CACHE_FLUSH_EXT		0xEA
#define ATA_CMD_PACKET			0xA0
#define ATA_CMD_IDENTIFY_PACKET		0xA1
#define ATA_CMD_IDENTIFY		0xEC

#define ATA_DRIVE_LBA48			0x40

#define ATA_STATUS_BSY		0x80    // Busy
#define ATA_STATUS_DRDY		0x40    // Drive ready
#define ATA_STATUS_DF		0x20    // Drive write fault
#define ATA_STATUS_DSC		0x10    // Drive seek complete
#define ATA_STATUS_DRQ		0x08    // Data request ready
#define ATA_STATUS_CORR		0x04    // Corrected data
#define ATA_STATUS_IDX		0x02    // Index
#define ATA_STATUS_ERR		0x01    // ERR

#define ATA_ERR_BBK		0x080   // Bad block
#define ATA_ERR_UNC		0x040   // Uncorrectable data
#define ATA_ERR_MC		0x020   // Media changed
#define ATA_ERR_IDNF		0x010   // ID mark not found
#define ATA_ERR_MCR		0x008   // Media change request
#define ATA_ERR_ABRT		0x004   // Command aborted
#define ATA_ERR_TK0NF		0x002   // Track 0 not found
#define ATA_ERR_AMNF		0x001   // No address mark

#define ATA_IDENT_DEVICETYPE	0
#define ATA_IDENT_CYLINDERS	2
#define ATA_IDENT_HEADS		6
#define ATA_IDENT_SECTORS	12
#define ATA_IDENT_SERIAL	20
#define ATA_IDENT_MODEL		54
#define ATA_IDENT_CAPABILITIES	98
#define ATA_IDENT_FIELDVALID	106
#define ATA_IDENT_MAX_LBA	120
#define ATA_IDENT_COMMANDSETS	164
#define ATA_IDENT_MAX_LBA_EXT	200

#define ATA_CAP_LBA		0x200

// Addressing modes
#define ATA_MODE_CHS		0x10
#define ATA_MODE_LBA28		0x20
#define ATA_MODE_LBA48		0x40


typedef struct _ata_drive{
	uint32_t base, ctrl;

	uint8_t id;
	uint8_t mode;
	uint16_t type;
	uint8_t is_slave;

	uint16_t sig;
	uint16_t cap; // capabilities
	uint32_t cmd_sets;
	uint64_t max_lba;
	char model_name[41];

	/* mode-dependant interface:
	 * read, write:
	 * Should return (count) - (read/written amount) if not all sectors are readable/writable.
	 * Should return 0 in case of an error that could render data unread/unwritten.
	 * Writes/reads of size 0 always return 0 and do nothing.
	 */
	size_t (*read)(struct _ata_drive*, uint64_t sector, size_t count, void* buf);
	size_t (*write)(struct _ata_drive*, uint64_t sector, size_t count, void* buf);
} ata_drive;


/* Return values:
 *	0: no IDE controllers found
 *	>0: amount of drives found
 */
size_t ata_probe(ata_drive drives_out[4]);


void ata_wait(ata_drive* drive);
/* Return values:
 * ATA_ERR_*: drive request returned error
 * 	  -1: DRQ not set
 *	   0: OK
 */
int ata_poll(ata_drive* drive);
void ata_soft_reset(ata_drive* drive);

void ata_select_drive(ata_drive* drive, uint32_t mode);

#endif
