#include "ext2.h"

#include "../cstdlib/string.h"
#include "../kernlib/kernmem.h"

#include "../bios/bios_io.h"

// ---------------------
// Public read interface
// ---------------------

void fs_ext2_read_sb(ata_drive* drive, fs_ext2_sb* sb)
{
	char buf[ATA_SECTOR_SIZE];
	drive->read(drive, 2, 1, buf);
	memcpy(sb, buf, sizeof(fs_ext2_sb) - sizeof(fs_ext2_sb_ext));
	if(sb->ver_major >= 1){
		memcpy((void*)sb + (sizeof(fs_ext2_sb) - sizeof(fs_ext2_sb_ext)),
			buf + (sizeof(fs_ext2_sb) - sizeof(fs_ext2_sb_ext)),
			sizeof(fs_ext2_sb_ext));
	}
	else{ // fill extension structure with some default values
		sb->first_unres_inode = 11;
		sb->inode_size = 128;

		sb->parent_block_group = 0;
		sb->opt_features = sb->req_features = sb->write_features = 0;
		sb->fs_id[0] = sb->volume_name[0] = sb->last_mnt_path[0] = '\0';
		sb->compr_alg = 0;
		sb->prealloc_file_blocks = sb->prealloc_dir_blocks = 0;
		sb->reserved = 0;
		sb->journal_id[0] = '\0';
		sb->journal_inode = sb->journal_device = sb->orphan_head = 0;
	}
}

void fs_ext2_read_blkgrp_table(ata_drive* drive, fs_ext2_sb* sb, fs_ext2_blkgrp_table* table)
{
	fs_ext2_blkgrp* rbuf = kmalloc(FS_EXT2_SB_BLOCKSIZE(*sb));

	// Seeking to the beginning of the block after superblock
	uint64_t table_lba;
	if(FS_EXT2_SB_BLOCKSIZE(*sb) <= 1024)
		table_lba = 2 * FS_EXT2_SB_BLOCKSECTORS(*sb);
	else
		table_lba = FS_EXT2_SB_BLOCKSECTORS(*sb);

	uint32_t epb = FS_EXT2_SB_BLOCKSIZE(*sb) / sizeof(fs_ext2_blkgrp) ; // block group entries per block
	for(uint32_t b = 0; b < table->length;)
	{
		uint32_t db = (b + epb) > table->length ? table->length - b : epb;
		drive->read(drive, table_lba, FS_EXT2_SB_BLOCKSECTORS(*sb), rbuf);
		table_lba += FS_EXT2_SB_BLOCKSECTORS(*sb);

		memcpy(table->groups + b, rbuf, db * sizeof(fs_ext2_blkgrp));
		b += db;
	}

	kfree(rbuf);
}

void fs_ext2_read_inode(ata_drive* drive, fs_ext2_sb* sb, fs_ext2_blkgrp_table* bt,
				uint32_t inode_num, fs_ext2_inode* _out)
{
	uint32_t blkgrp = (inode_num - 1) / sb->inodes_per_group;
	uint32_t tind = (inode_num - 1) % sb->inodes_per_group;
	uint32_t baddr = (tind * sb->inode_size) / FS_EXT2_SB_BLOCKSIZE(*sb);

	void* buf = kmalloc(FS_EXT2_SB_BLOCKSIZE(*sb));
	drive->read(drive, (bt->groups[blkgrp].inode_table_addr + baddr) * FS_EXT2_SB_BLOCKSECTORS(*sb),
			FS_EXT2_SB_BLOCKSECTORS(*sb), buf);
	memcpy(_out, buf + sb->inode_size * tind, sb->inode_size);

	kfree(buf);
}


uint32_t fs_ext2_get_inode_pointer(ata_drive* drive, fs_ext2_sb* sb, fs_ext2_inode* inode,
				uint32_t bind, void* buf)
{
	#define DBP_PER_BLOCK (FS_EXT2_SB_BLOCKSIZE(*sb) / sizeof(inode->dbp[0]))

	if(bind < 12) { // accessing a DBP
		return inode->dbp[bind];
	}
	else if(bind < 12 + DBP_PER_BLOCK){ // accessing a SIBP
		if(!inode->sibp) return 0;
		drive->read(drive, inode->sibp * FS_EXT2_SB_BLOCKSECTORS(*sb),
				FS_EXT2_SB_BLOCKSECTORS(*sb), buf); // read the block SIBP is pointing to

		uint32_t sibp_addr = bind - 12;
		return ((uint32_t*)buf)[sibp_addr];
	}
	else if(bind < 12 + DBP_PER_BLOCK + DBP_PER_BLOCK * DBP_PER_BLOCK){ // accessing a DIBP
		if(!inode->dibp) return 0;
		drive->read(drive, inode->dibp * FS_EXT2_SB_BLOCKSECTORS(*sb),
				FS_EXT2_SB_BLOCKSECTORS(*sb), buf); // read block DIBP is pointing to

		uint32_t dibp_addr = (bind - 12 - DBP_PER_BLOCK) / DBP_PER_BLOCK; // address of SIBP block which contains the needed DBP
		uint32_t baddr = ((uint32_t*)buf)[dibp_addr];
		if(!baddr) return 0;
		drive->read(drive, baddr * FS_EXT2_SB_BLOCKSECTORS(*sb),
				FS_EXT2_SB_BLOCKSECTORS(*sb), buf); // read block SIBP is pointing to

		uint32_t sibp_addr = (bind - 12 - DBP_PER_BLOCK) % DBP_PER_BLOCK; // address of DBP inside the located SIBP
		return ((uint32_t*)buf)[sibp_addr];
	}
	else{ // accessing a TIBP
		if(!inode->tibp) return 0;
		drive->read(drive, inode->tibp * FS_EXT2_SB_BLOCKSECTORS(*sb),
				FS_EXT2_SB_BLOCKSECTORS(*sb), buf); // read the block TIBP is pointing to

		uint32_t tibp_addr = (bind - 12 - DBP_PER_BLOCK - DBP_PER_BLOCK * DBP_PER_BLOCK) / DBP_PER_BLOCK * DBP_PER_BLOCK; // address of DIBP block which contains the needed SIBP
		uint32_t baddr = ((uint32_t*)buf)[tibp_addr];
		if(!baddr) return 0;
		drive->read(drive, baddr * FS_EXT2_SB_BLOCKSECTORS(*sb),
				FS_EXT2_SB_BLOCKSECTORS(*sb), buf); // read the block DIBP is pointing to

		uint32_t dibp_addr = (bind - 12 - DBP_PER_BLOCK - DBP_PER_BLOCK * DBP_PER_BLOCK) / DBP_PER_BLOCK; // address of SIBP block which contains the needed DBP
		baddr = ((uint32_t*)buf)[dibp_addr];
		if(!baddr) return 0;
		drive->read(drive, baddr * FS_EXT2_SB_BLOCKSECTORS(*sb),
				FS_EXT2_SB_BLOCKSECTORS(*sb), buf); // read the block SIBP is pointing to

		uint32_t sibp_addr = (bind - 12 - DBP_PER_BLOCK - DBP_PER_BLOCK * DBP_PER_BLOCK) % DBP_PER_BLOCK; // address of DBP inside the located SIBP
		return ((uint32_t*)buf)[sibp_addr];
	}
	return 0;
}

int fs_ext2_read_inode_data(ata_drive* drive, fs_ext2_sb* sb, fs_ext2_inode* inode,
				uint32_t bind, void* bout)
{
	uint32_t baddr = fs_ext2_get_inode_pointer(drive, sb, inode, bind, bout);
	if(!baddr)
		return 0;

	drive->read(drive, baddr * FS_EXT2_SB_BLOCKSECTORS(*sb),
				FS_EXT2_SB_BLOCKSECTORS(*sb), bout);
	return 1;
}


void fs_ext2_iterate_dir_start(ata_drive* drive, fs_ext2_sb* sb, fs_ext2_inode* inode,
					fs_ext2_dir_iterator* it, void* pbuf)
{
	it->dir = inode;

	it->byteaddr = 0;
	it->blki = 0;
	it->blkbuf = pbuf;
	it->is_valid = 1;
	fs_ext2_read_inode_data(drive, sb, inode, 0, pbuf);
}

int fs_ext2_iterate_dir_next(ata_drive* drive, fs_ext2_sb* sb, fs_ext2_dir_iterator* it)
{
	#define DIRENT_SZ_WO_NAME (sizeof(it->cur) - sizeof(it->cur.name))
	if(!it->is_valid)
		return 0;
	memcpy(&it->cur, it->blkbuf + it->byteaddr, DIRENT_SZ_WO_NAME); // copy directory entry info

	if(it->cur.inode_num)
	{ // copy entry name only if inode is not 0
		uint32_t bcpy = it->cur.name_length_lo; // bytes to copy after dirent structure
		if(!(sb->req_features & FS_EXT2_SB_RWFEATURE_DIRS_HAVE_TYPE_FIELD))
			bcpy |= ((uint32_t)it->cur.name_length_hi) << (sizeof(it->cur.name_length_lo) * 8);
		uint32_t name_bcpy = bcpy <= sizeof(it->cur.name) ? bcpy : sizeof(it->cur.name); // bytes to copy in direntry name

		memcpy(it->cur.name, it->blkbuf + it->byteaddr + DIRENT_SZ_WO_NAME, // copy entry name
			name_bcpy);
		it->cur.name[name_bcpy] = '\0';
	}

	it->byteaddr += it->cur.entry_sz;
	if(it->byteaddr >= FS_EXT2_SB_BLOCKSIZE(*sb)){ // extended past current block, changing to another one
		it->byteaddr = 0;
		it->blki++;
		if(!fs_ext2_read_inode_data(drive, sb, it->dir, it->blki, it->blkbuf)){
			it->is_valid = 0;
			return 1;
		}
	}
	return 1;
	#undef DIRENT_SZ_WO_NAME
}

uint32_t fs_ext2_find_grp_unalloc_block(ata_drive* drive, fs_ext2_sb* sb,
						fs_ext2_blkgrp_table* bt, uint32_t blkgrp)
{
	void* buf = kmalloc(FS_EXT2_SB_BLOCKSIZE(*sb));

	for(size_t bmb = bt->groups[blkgrp].block_bitmap_addr;
		bmb < bt->groups[blkgrp].inode_bitmap_addr;
		++bmb)
	{
		drive->read(drive, bmb * FS_EXT2_SB_BLOCKSECTORS(*sb),
				FS_EXT2_SB_BLOCKSECTORS(*sb), buf);
		for(uint32_t bind = 0; bind < FS_EXT2_SB_BLOCKSIZE(*sb); ++bind)
		{
			uint8_t b = ((uint8_t*)buf)[bind];
			if(b != 255){
				uint8_t bsh = 0;
				while(b & 1) { b >>= 1; bsh++; }

				kfree(buf);
				return (bind * 8 + (7 - bsh));
			}
		}
	}

	kfree(buf);
	return (uint32_t)-1;
}
uint32_t fs_ext2_find_unalloc_block(ata_drive* drive, fs_ext2_sb* sb,
					fs_ext2_blkgrp_table* bt, uint32_t blkgrp_start)
{
	uint32_t bi; for(bi = blkgrp_start; bi < bt->length; ++bi)
	{
		uint32_t ubi = fs_ext2_find_grp_unalloc_block(drive, sb, bt, bi);
		if(ubi != (uint32_t)-1)
			return ubi;
	}
	for(uint32_t bi2 = 0; bi2 < bi; ++bi2)
	{
		uint32_t ubi = fs_ext2_find_grp_unalloc_block(drive, sb, bt, bi2);
		if(ubi != (uint32_t)-1)
			return ubi;
	}
	return (uint32_t)-1;
}


// ---------------------
// Public write interface
// ---------------------

void fs_ext2_write_sb(ata_drive* drive, fs_ext2_sb* sb)
{
	char buf[ATA_SECTOR_SIZE];
	drive->read(drive, 2, 1, buf);

	if(sb->ver_major >= 1)
		memcpy(buf, sb, sizeof(fs_ext2_sb));
	else
		memcpy(buf, sb, sizeof(fs_ext2_sb) - sizeof(fs_ext2_sb_ext));

	drive->write(drive, 2, 1, buf);
}

void fs_ext2_write_blkgrp_table_index(ata_drive* drive, fs_ext2_sb* sb,
					fs_ext2_blkgrp_table* bt, uint32_t i)
{
	// Seeking to the beginning of the block after superblock
	uint32_t table_lba;
	if(FS_EXT2_SB_BLOCKSIZE(*sb) <= 1024)
		table_lba = 2 * FS_EXT2_SB_BLOCKSECTORS(*sb);
	else
		table_lba = FS_EXT2_SB_BLOCKSECTORS(*sb);

	uint32_t eps = ATA_SECTOR_SIZE / sizeof(fs_ext2_blkgrp) ; // block group entries per sector
	uint32_t baddr = table_lba + ATA_SECTOR_SIZE * (i / eps);
	uint32_t tbaddr = (i / eps) * eps;

	drive->write(drive, baddr, 1, bt->groups + tbaddr);
}

void fs_ext2_mark_alloc_inode(ata_drive* drive, fs_ext2_sb* sb,
					fs_ext2_blkgrp_table* bt, uint32_t inode_num)
{
	uint32_t blkgrp = (inode_num - 1) / sb->inodes_per_group;
	uint32_t tind = (inode_num - 1) % sb->inodes_per_group; // index withing block group

	void* buf = kmalloc(ATA_SECTOR_SIZE);

	uint32_t bmp_addr = bt->groups[blkgrp].inode_bitmap_addr * FS_EXT2_SB_BLOCKSECTORS(*sb)
				+ tind / 8 / ATA_SECTOR_SIZE;
	drive->read(drive, bmp_addr, 1, buf);
	uint32_t bmp_byte = (tind / 8) % ATA_SECTOR_SIZE,
		 bmp_bit = tind % 8;
	((uint8_t*)buf)[bmp_byte] |= 1 << (7 - bmp_bit);
	drive->write(drive, bmp_addr, 1, buf);

	kfree(buf);
}

void fs_ext2_mark_alloc_block(ata_drive* drive, fs_ext2_sb* sb,
					fs_ext2_blkgrp_table* bt, uint32_t block_num)
{
	uint32_t blkgrp = (block_num - 1) / sb->blocks_per_group;
	uint32_t tind = (block_num - 1) % sb->blocks_per_group; // index withing block group

	void* buf = kmalloc(ATA_SECTOR_SIZE);

	uint32_t bmp_addr = bt->groups[blkgrp].block_bitmap_addr * FS_EXT2_SB_BLOCKSECTORS(*sb)
				+ tind / 8 / ATA_SECTOR_SIZE;
	drive->read(drive, bmp_addr, 1, buf);
	uint32_t bmp_byte = (tind / 8) % ATA_SECTOR_SIZE,
		 bmp_bit = tind % 8;
	((uint8_t*)buf)[bmp_byte] |= 1 << (7 - bmp_bit);
	drive->write(drive, bmp_addr, 1, buf);

	kfree(buf);
}

void fs_ext2_mark_unalloc_inode(ata_drive* drive, fs_ext2_sb* sb,
					fs_ext2_blkgrp_table* bt, uint32_t inode_num)
{
	uint32_t blkgrp = (inode_num - 1) / sb->inodes_per_group;
	uint32_t tind = (inode_num - 1) % sb->inodes_per_group; // index withing block group

	void* buf = kmalloc(ATA_SECTOR_SIZE);

	uint32_t bmp_addr = bt->groups[blkgrp].inode_bitmap_addr * FS_EXT2_SB_BLOCKSECTORS(*sb)
				+ tind / 8 / ATA_SECTOR_SIZE;
	drive->read(drive, bmp_addr, 1, buf);
	uint32_t bmp_byte = (tind / 8) % ATA_SECTOR_SIZE,
		 bmp_bit = tind % 8;
	((uint8_t*)buf)[bmp_byte] &= ~(1 << (7 - bmp_bit));
	drive->write(drive, bmp_addr, 1, buf);

	kfree(buf);
}

void fs_ext2_mark_unalloc_block(ata_drive* drive, fs_ext2_sb* sb,
					fs_ext2_blkgrp_table* bt, uint32_t block_num)
{
	uint32_t blkgrp = (block_num - 1) / sb->blocks_per_group;
	uint32_t tind = (block_num - 1) % sb->blocks_per_group; // index withing block group

	void* buf = kmalloc(ATA_SECTOR_SIZE);

	uint32_t bmp_addr = bt->groups[blkgrp].block_bitmap_addr * FS_EXT2_SB_BLOCKSECTORS(*sb)
				+ tind / 8 / ATA_SECTOR_SIZE;
	drive->read(drive, bmp_addr, 1, buf);
	uint32_t bmp_byte = (tind / 8) % ATA_SECTOR_SIZE,
		 bmp_bit = tind % 8;
	((uint8_t*)buf)[bmp_byte] &= ~(1 << (7 - bmp_bit));
	drive->write(drive, bmp_addr, 1, buf);

	kfree(buf);
}


void fs_ext2_write_inode(ata_drive* drive, fs_ext2_sb* sb, fs_ext2_blkgrp_table* bt,
				uint32_t inode_num, fs_ext2_inode* _in)
{
	uint32_t blkgrp = (inode_num - 1) / sb->inodes_per_group;
	uint32_t tind = (inode_num - 1) % sb->inodes_per_group; // index withing block group
	uint32_t saddr = (tind * sb->inode_size) / ATA_SECTOR_SIZE;

	void* buf = kmalloc(ATA_SECTOR_SIZE);

	drive->read(drive, bt->groups[blkgrp].inode_table_addr * FS_EXT2_SB_BLOCKSECTORS(*sb) + saddr,
			1, buf);
	memcpy(buf + sb->inode_size * tind, _in, sb->inode_size);
	drive->write(drive, bt->groups[blkgrp].inode_table_addr * FS_EXT2_SB_BLOCKSECTORS(*sb) + saddr,
			1, buf);

	kfree(buf);
}

void fs_ext2_write_inode_block_pointer(ata_drive* drive, fs_ext2_sb* sb, fs_ext2_blkgrp_table* bt,
					fs_ext2_inode* inode, uint32_t inode_num, uint32_t bind, uint32_t ptr)
{
	#define DBP_PER_BLOCK (FS_EXT2_SB_BLOCKSIZE(*sb) / sizeof(inode->dbp[0]))

	if(bind < 12) { // accessing a DBP
		inode->dbp[bind] = ptr;
		fs_ext2_write_inode(drive, sb, bt, inode_num, inode);
	}
	else if(bind < 12 + DBP_PER_BLOCK){ // accessing a SIBP
		uint32_t sibp_addr = bind - 12;
		void* buf = kmalloc(ATA_SECTOR_SIZE);

		drive->read(drive, inode->sibp * FS_EXT2_SB_BLOCKSECTORS(*sb)
				+ (sibp_addr * sizeof(uint32_t) / ATA_SECTOR_SIZE),
				ATA_SECTOR_SIZE, buf); // read the block SIBP is pointing to
		((uint32_t*)buf)[sibp_addr * sizeof(uint32_t) % ATA_SECTOR_SIZE] = ptr;
		drive->write(drive, inode->sibp * FS_EXT2_SB_BLOCKSECTORS(*sb)
				+ (sibp_addr * sizeof(uint32_t) / ATA_SECTOR_SIZE),
				ATA_SECTOR_SIZE, buf);

		kfree(buf);
	}
	else if(bind < 12 + DBP_PER_BLOCK + DBP_PER_BLOCK * DBP_PER_BLOCK){ // accessing a DIBP
		void* buf = kmalloc(ATA_SECTOR_SIZE);
		uint32_t dibp_addr = (bind - 12 - DBP_PER_BLOCK) / DBP_PER_BLOCK; // address of SIBP block which contains the needed DBP
		uint32_t sibp_addr = (bind - 12 - DBP_PER_BLOCK) % DBP_PER_BLOCK; // address of DBP inside the located SIBP

		drive->read(drive, inode->dibp * FS_EXT2_SB_BLOCKSECTORS(*sb),
				FS_EXT2_SB_BLOCKSECTORS(*sb), buf); // read block DIBP is pointing to

		uint32_t baddr = ((uint32_t*)buf)[dibp_addr];
		drive->read(drive, baddr * FS_EXT2_SB_BLOCKSECTORS(*sb)
				+ (sibp_addr * sizeof(uint32_t) / ATA_SECTOR_SIZE),
				ATA_SECTOR_SIZE, buf); // read the block SIBP is pointing to
		((uint32_t*)buf)[sibp_addr * sizeof(uint32_t) % ATA_SECTOR_SIZE] = ptr;
		drive->write(drive, baddr * FS_EXT2_SB_BLOCKSECTORS(*sb)
				+ (sibp_addr * sizeof(uint32_t) / ATA_SECTOR_SIZE),
				ATA_SECTOR_SIZE, buf);

		kfree(buf);
	}
	else{ // accessing a TIBP
		void* buf = kmalloc(ATA_SECTOR_SIZE);
		uint32_t tibp_addr = (bind - 12 - DBP_PER_BLOCK - DBP_PER_BLOCK * DBP_PER_BLOCK) / DBP_PER_BLOCK * DBP_PER_BLOCK; // address of DIBP block which contains the needed SIBP
		uint32_t dibp_addr = (bind - 12 - DBP_PER_BLOCK - DBP_PER_BLOCK * DBP_PER_BLOCK) / DBP_PER_BLOCK; // address of SIBP block which contains the needed DBP
		uint32_t sibp_addr = (bind - 12 - DBP_PER_BLOCK - DBP_PER_BLOCK * DBP_PER_BLOCK) % DBP_PER_BLOCK; // address of DBP inside the located SIBP

		drive->read(drive, inode->tibp * FS_EXT2_SB_BLOCKSECTORS(*sb),
				FS_EXT2_SB_BLOCKSECTORS(*sb), buf); // read the block TIBP is pointing to
		uint32_t baddr = ((uint32_t*)buf)[tibp_addr];
		drive->read(drive, baddr * FS_EXT2_SB_BLOCKSECTORS(*sb),
				FS_EXT2_SB_BLOCKSECTORS(*sb), buf); // read the block DIBP is pointing to

		baddr = ((uint32_t*)buf)[dibp_addr];
		drive->read(drive, baddr * FS_EXT2_SB_BLOCKSECTORS(*sb)
				+ (sibp_addr * sizeof(uint32_t) / ATA_SECTOR_SIZE),
				ATA_SECTOR_SIZE, buf); // read the block SIBP is pointing to
		((uint32_t*)buf)[sibp_addr * sizeof(uint32_t) % ATA_SECTOR_SIZE] = ptr;
		drive->write(drive, baddr * FS_EXT2_SB_BLOCKSECTORS(*sb)
				+ (sibp_addr * sizeof(uint32_t) / ATA_SECTOR_SIZE),
				ATA_SECTOR_SIZE, buf);

		kfree(buf);
	}
}

int fs_ext2_write_inode_data(ata_drive* drive, fs_ext2_sb* sb, fs_ext2_inode* inode,
				uint32_t bind, void* bin)
{
	#define DBP_PER_BLOCK (FS_EXT2_SB_BLOCKSIZE(*sb) / sizeof(inode->dbp[0]))

	if(bind < 12) { // accessing a DBP
		uint32_t baddr = inode->dbp[bind];
		if(!baddr) return 0;
		drive->write(drive, baddr * FS_EXT2_SB_BLOCKSECTORS(*sb),
				FS_EXT2_SB_BLOCKSECTORS(*sb), bin);
	}
	else if(bind < 12 + DBP_PER_BLOCK){ // accessing a SIBP
		if(!inode->sibp) return 0;
		drive->read(drive, inode->sibp * FS_EXT2_SB_BLOCKSECTORS(*sb),
				FS_EXT2_SB_BLOCKSECTORS(*sb), bin); // read the block SIBP is pointing to

		uint32_t sibp_addr = bind - 12;
		uint32_t baddr = ((uint32_t*)bin)[sibp_addr];
		if(!baddr) return 0;
		drive->write(drive, baddr * FS_EXT2_SB_BLOCKSECTORS(*sb),
				FS_EXT2_SB_BLOCKSECTORS(*sb), bin);
	}
	else if(bind < 12 + DBP_PER_BLOCK + DBP_PER_BLOCK * DBP_PER_BLOCK){ // accessing a DIBP
		if(!inode->dibp) return 0;
		drive->read(drive, inode->dibp * FS_EXT2_SB_BLOCKSECTORS(*sb),
				FS_EXT2_SB_BLOCKSECTORS(*sb), bin); // read block DIBP is pointing to

		uint32_t dibp_addr = (bind - 12 - DBP_PER_BLOCK) / DBP_PER_BLOCK; // address of SIBP block which contains the needed DBP
		uint32_t baddr = ((uint32_t*)bin)[dibp_addr];
		if(!baddr) return 0;
		drive->read(drive, baddr * FS_EXT2_SB_BLOCKSECTORS(*sb),
				FS_EXT2_SB_BLOCKSECTORS(*sb), bin); // read block SIBP is pointing to

		uint32_t sibp_addr = (bind - 12 - DBP_PER_BLOCK) % DBP_PER_BLOCK; // address of DBP inside the located SIBP
		baddr = ((uint32_t*)bin)[sibp_addr];
		if(!baddr) return 0;
		drive->write(drive, baddr * FS_EXT2_SB_BLOCKSECTORS(*sb),
				FS_EXT2_SB_BLOCKSECTORS(*sb), bin);
	}
	else{ // accessing a TIBP
		if(!inode->tibp) return 0;
		drive->read(drive, inode->tibp * FS_EXT2_SB_BLOCKSECTORS(*sb),
				FS_EXT2_SB_BLOCKSECTORS(*sb), bin); // read the block TIBP is pointing to

		uint32_t tibp_addr = (bind - 12 - DBP_PER_BLOCK - DBP_PER_BLOCK * DBP_PER_BLOCK) / DBP_PER_BLOCK * DBP_PER_BLOCK; // address of DIBP block which contains the needed SIBP
		uint32_t baddr = ((uint32_t*)bin)[tibp_addr];
		if(!baddr) return 0;
		drive->read(drive, baddr * FS_EXT2_SB_BLOCKSECTORS(*sb),
				FS_EXT2_SB_BLOCKSECTORS(*sb), bin); // read the block DIBP is pointing to

		uint32_t dibp_addr = (bind - 12 - DBP_PER_BLOCK - DBP_PER_BLOCK * DBP_PER_BLOCK) / DBP_PER_BLOCK; // address of SIBP block which contains the needed DBP
		baddr = ((uint32_t*)bin)[dibp_addr];
		if(!baddr) return 0;
		drive->read(drive, baddr * FS_EXT2_SB_BLOCKSECTORS(*sb),
				FS_EXT2_SB_BLOCKSECTORS(*sb), bin); // read the block SIBP is pointing to

		uint32_t sibp_addr = (bind - 12 - DBP_PER_BLOCK - DBP_PER_BLOCK * DBP_PER_BLOCK) % DBP_PER_BLOCK; // address of DBP inside the located SIBP
		baddr = ((uint32_t*)bin)[sibp_addr];
		if(!baddr) return 0;
		drive->write(drive, baddr * FS_EXT2_SB_BLOCKSECTORS(*sb),
				FS_EXT2_SB_BLOCKSECTORS(*sb), bin);
	}
	return 1;
}

// -------------
// GFS functions
// -------------

typedef struct{
	fs_ext2_sb sb;
	fs_ext2_blkgrp_table bt;
} gfs_ext2_gdata;

static int fs_ext2_create(file_system* fs, const char* path, int flags);


void fs_ext2_gfs_init(file_system* fs)
{
	fs->gdata = kmalloc(sizeof(gfs_ext2_gdata));
	gfs_ext2_gdata* gdat = (gfs_ext2_gdata*)fs->gdata;

	fs_ext2_read_sb(fs->drive, &gdat->sb);

	gdat->bt = (fs_ext2_blkgrp_table){kmalloc(FS_EXT2_SB_BLOCKGROUPS_TOTAL(gdat->sb) * sizeof(fs_ext2_blkgrp)),
			FS_EXT2_SB_BLOCKGROUPS_TOTAL(gdat->sb)};
	fs_ext2_read_blkgrp_table(fs->drive, &gdat->sb, &gdat->bt);

	/*fs->fd_size*/

	fs->create = &fs_ext2_create;
}

static int fs_ext2_find_final_inode(file_system* fs, const char* path,
					fs_ext2_inode* _out, uint32_t* _num_out)
{
	gfs_ext2_gdata* gdat = (gfs_ext2_gdata*)fs->gdata;

	char* path_buf = kmalloc(strlen(path) + 1);
	strcpy(path_buf, path);

	fs_ext2_inode cur_dir;
	uint32_t cur_inode_num = FS_EXT2_ROOT_INODE;

	fs_ext2_dir_iterator it;
	void* it_buf = kmalloc(FS_EXT2_SB_BLOCKSIZE(gdat->sb));
	const char* fcur;
	while( (fcur = fs_next_file(&path_buf)) )
	{
		fs_ext2_read_inode(fs->drive, &gdat->sb, &gdat->bt, cur_inode_num, &cur_dir);
		fs_ext2_iterate_dir_start(fs->drive, &gdat->sb, &cur_dir, &it, it_buf);

		cur_inode_num = 0;
		while(fs_ext2_iterate_dir_next(fs->drive, &gdat->sb, &it))
		{
			if(!strcmp(it.cur.name, fcur)){
				cur_inode_num = it.cur.inode_num;
				break;
			}
		}

		if(!cur_inode_num){
			kfree(it_buf); kfree(path_buf);
			return FS_ERR_DOESNT_EXIST;
		}
	}

	// make sure to read root directory if it's the sole filesystem object in the path
	if(cur_inode_num == FS_EXT2_ROOT_INODE)
		fs_ext2_read_inode(fs->drive, &gdat->sb, &gdat->bt, cur_inode_num, &cur_dir);
	*_out = cur_dir;
	*_num_out = cur_inode_num;
	kfree(it_buf); kfree(path_buf);
	return 0;
}

static int fs_ext2_occupy_vacant_dirent(file_system* fs, fs_ext2_inode* dir, uint32_t dir_num,
					uint32_t de_inode, const char* de_name)
{
	gfs_ext2_gdata* gdat = (gfs_ext2_gdata*)fs->gdata;

	#define DIRENT_SZ_WO_NAME (sizeof(it.cur) - sizeof(it.cur.name))
	fs_ext2_dir_iterator it;
	void* it_buf = kmalloc(FS_EXT2_SB_BLOCKSIZE(gdat->sb));
	fs_ext2_iterate_dir_start(fs->drive, &gdat->sb, dir, &it, it_buf);
	while(fs_ext2_iterate_dir_next(fs->drive, &gdat->sb, &it))
	{
		uint16_t nameln = it.cur.name_length_lo;
		if(!(gdat->sb.req_features & FS_EXT2_SB_RWFEATURE_DIRS_HAVE_TYPE_FIELD))
			nameln |= (uint16_t)it.cur.name_length_hi << (sizeof(it.cur.name_length_lo) * 8);
		size_t gap = it.cur.entry_sz - nameln - DIRENT_SZ_WO_NAME;

		if(gap >= DIRENT_SZ_WO_NAME + strlen(de_name) + 1)
		{ // found a fitting gap for a directory entry
			// shrink the entry to minimal size
			it.cur.entry_sz = nameln + DIRENT_SZ_WO_NAME;
			memcpy(it.blkbuf + it.byteaddr, &it.cur, DIRENT_SZ_WO_NAME);
			memcpy(it.blkbuf + it.byteaddr + DIRENT_SZ_WO_NAME, it.cur.name, nameln);

			// form a directory entry
			fs_ext2_dirent de_in;
			uint16_t nameln = strlen(de_name);
			de_in.inode_num = de_inode;
			if(gdat->sb.req_features & FS_EXT2_SB_RWFEATURE_DIRS_HAVE_TYPE_FIELD){
				nameln = (nameln <= 255 ? nameln : 255);
				de_in.name_length_lo = (uint8_t)nameln;
			}
			else{
				de_in.name_length_lo = (uint8_t)(nameln & 0xFF);
				de_in.name_length_hi = (uint8_t)((nameln >> 8) & 0xFF);
			}

			// writing directory entry to block buffer
			uint32_t de_addr = it.byteaddr + DIRENT_SZ_WO_NAME + nameln;
			memcpy(it.blkbuf + de_addr, &de_in, DIRENT_SZ_WO_NAME);
			memcpy(it.blkbuf + de_addr + DIRENT_SZ_WO_NAME, de_name, nameln);

			// finally flushing changes to both entries to disk
			fs_ext2_write_inode_data(fs->drive, &gdat->sb, dir, it.blki, it.blkbuf);
			return 1;
		}
	}
	// no fitting gap was found, allocating and writing next block
	uint32_t new_blk_grp = fs_ext2_get_inode_pointer(fs->drive, &gdat->sb, dir, it.blki, it_buf) / gdat->sb.blocks_per_group;
	uint32_t new_blk = fs_ext2_find_unalloc_block(fs->drive, &gdat->sb, &gdat->bt,
					new_blk_grp);
	fs_ext2_write_inode_block_pointer(fs->drive, &gdat->sb, &gdat->bt, dir,
						dir_num, it.blki, new_blk);

	kfree(it_buf);
	return 0;
	#undef DIRENT_SZ_WO_NAME
}


static int fs_ext2_create(file_system* fs, const char* path, int flags)
{
	gfs_ext2_gdata* gdat = (gfs_ext2_gdata*)fs->gdata;

	fs_ext2_inode fnode; uint32_t fnode_num;

	char* path_buf = kmalloc(strlen(path) + 1);
	strcpy(path_buf, path);
	// intense wiretearing to get the file name out
	char* i; for(i = path_buf + strlen(path); i >= path_buf; --i)
		if(*i == '/') { *i = '\0'; break; }
	// if operating on root directory, just remove the whole thing, we are not UNIX
	if(i < path_buf) path_buf[0] = '\0';

	int err = fs_ext2_find_final_inode(fs, path_buf, &fnode, &fnode_num);
	kfree(path_buf);
	if(err)
		return err;

	fs_ext2_occupy_vacant_dirent(fs, &fnode, fnode_num, 13, "test");

	return 0;
}
