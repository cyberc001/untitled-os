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
	it->is_valid = fs_ext2_read_inode_data(drive, sb, inode, 0, pbuf);
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
		it->blki++;
		if(!fs_ext2_read_inode_data(drive, sb, it->dir, it->blki, it->blkbuf)){
			it->blki--;
			it->byteaddr -= it->cur.entry_sz;
			it->is_valid = 0;
			return 1;
		}
		it->byteaddr = 0;
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
				while(b & 0x80) { b <<= 1; bsh++; }

				kfree(buf);
				return (bind * 8 + bsh);
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
			return bt->groups[bi].inode_table_addr + (sb->inodes_per_group * sb->inode_size / FS_EXT2_SB_BLOCKSIZE(*sb))
				+ bi * sb->blocks_per_group + ubi;
	}
	for(uint32_t bi2 = 0; bi2 < bi; ++bi2)
	{
		uint32_t ubi = fs_ext2_find_grp_unalloc_block(drive, sb, bt, bi2);
		if(ubi != (uint32_t)-1)
			return bt->groups[bi].inode_table_addr + (sb->inodes_per_group * sb->inode_size / FS_EXT2_SB_BLOCKSIZE(*sb))
				+ bi * sb->blocks_per_group + ubi;
	}
	return (uint32_t)-1;
}

uint32_t fs_ext2_find_grp_unalloc_inode(ata_drive* drive, fs_ext2_sb* sb,
					fs_ext2_blkgrp_table* bt, uint32_t blkgrp)
{
	void* buf = kmalloc(FS_EXT2_SB_BLOCKSIZE(*sb));

	for(size_t bmb = bt->groups[blkgrp].inode_bitmap_addr;
		bmb < bt->groups[blkgrp].inode_bitmap_addr +
		(sb->inodes_per_group / (8 * FS_EXT2_SB_BLOCKSIZE(*sb))) +
		!!(sb->inodes_per_group % (8 * FS_EXT2_SB_BLOCKSIZE(*sb)));
		++bmb)
	{
		drive->read(drive, bmb * FS_EXT2_SB_BLOCKSECTORS(*sb),
				FS_EXT2_SB_BLOCKSECTORS(*sb), buf);
		for(uint32_t bind = 0; bind < FS_EXT2_SB_BLOCKSIZE(*sb); ++bind)
		{
			if(blkgrp * sb->inodes_per_group + bind * 8 <= sb->first_unres_inode)
				continue;

			uint8_t b = ((uint8_t*)buf)[bind];
			if(b != 255){
				uint8_t bsh = 0;
				while(b & 0x80 ||
				blkgrp * sb->inodes_per_group + bind * 8 + bsh <= sb->first_unres_inode)
				{
					b <<= 1;
					++bsh;
				}

				kfree(buf);
				return (bind * 8 + bsh) + 1;
			}
		}
	}

	kfree(buf);
	return 0;
}
uint32_t fs_ext2_find_unalloc_inode(ata_drive* drive, fs_ext2_sb* sb,
					fs_ext2_blkgrp_table* bt, uint32_t blkgrp_start)
{
	uint32_t bi; for(bi = blkgrp_start; bi < bt->length; ++bi)
	{
		uint32_t inum = fs_ext2_find_grp_unalloc_inode(drive, sb, bt, bi);
		if(inum != 0)
			return bi * sb->inodes_per_group + inum;
	}
	for(uint32_t bi2 = 0; bi2 < bi; ++bi2)
	{
		uint32_t inum = fs_ext2_find_grp_unalloc_inode(drive, sb, bt, bi2);
		if(inum != 0)
			return bi * sb->inodes_per_group + inum;
	}
	return 0;
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
	uint32_t blkgrp = (block_num) / sb->blocks_per_group;
	uint32_t tind = (block_num) % sb->blocks_per_group; // index withing block group

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
	uint32_t tind = (inode_num - 1) % sb->inodes_per_group;
	uint32_t baddr = (tind * sb->inode_size) / FS_EXT2_SB_BLOCKSIZE(*sb);

	void* buf = kmalloc(FS_EXT2_SB_BLOCKSIZE(*sb));
	drive->read(drive, (bt->groups[blkgrp].inode_table_addr + baddr) * FS_EXT2_SB_BLOCKSECTORS(*sb),
			FS_EXT2_SB_BLOCKSECTORS(*sb), buf);
	memcpy(buf + sb->inode_size * tind, _in, sb->inode_size);
	drive->write(drive, (bt->groups[blkgrp].inode_table_addr + baddr) * FS_EXT2_SB_BLOCKSECTORS(*sb),
			FS_EXT2_SB_BLOCKSECTORS(*sb), buf);

	kfree(buf);
}


#define TRY_ALLOC_IBP(ibp, buf)\
{\
	if(!(ibp)){\
		uint32_t blkgrp_start = (inode_num - 1) / sb->inodes_per_group;\
		(ibp) = fs_ext2_find_unalloc_block(drive, sb, bt, blkgrp_start);\
		if(!(ibp)){\
			kfree(buf);\
			return 0;\
		}\
		fs_ext2_mark_alloc_block(drive, sb, bt, (ibp));\
		\
		memset(buf, 0, ATA_SECTOR_SIZE);\
		((uint32_t*)buf)[sibp_addr * sizeof(uint32_t) % ATA_SECTOR_SIZE] = ptr;\
		drive->write(drive, (ibp) * FS_EXT2_SB_BLOCKSECTORS(*sb)\
				+ (sibp_addr * sizeof(uint32_t) / ATA_SECTOR_SIZE),\
				ATA_SECTOR_SIZE, buf);\
		return 1;\
	}\
}

int fs_ext2_write_inode_block_pointer(ata_drive* drive, fs_ext2_sb* sb, fs_ext2_blkgrp_table* bt,
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

		TRY_ALLOC_IBP(inode->sibp, buf);

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

		TRY_ALLOC_IBP(inode->dibp, buf);

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

		TRY_ALLOC_IBP(inode->tibp, buf);

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
	return 1;
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

typedef struct{
	// general info
	fs_ext2_inode inode;
	uint32_t inode_num;

	// read/write pointer
	struct{
		void* blkbuf;		// buffer for current block data
		uint32_t blki;		// current block pointer index
		uint32_t byteaddr;	// current byte address withing current block
		uint8_t is_blk_valid;	// if 0, then current block is not allocated
		uint8_t is_buf_init;	// if 0, then buffer is not filled with data (and should be on read, clearing the value)
	} ptr;
} gfs_ext2_fd;

static int fs_ext2_create(file_system* fs, const char* path, int type);
static int fs_ext2_unlink(file_system* fs, const char* path);
static int fs_ext2_rename(file_system* fs, const char* old, const char* _new);
static int fs_ext2_open(file_system* fs, void* fd, const char* path, int flags);
static void fs_ext2_close(file_system* fs, void* fd);
static long fs_ext2_seek(file_system* fs, void* fd, long offset, int whence);
static size_t fs_ext2_read(file_system* fs, void* fd, void* buf, size_t count);
static size_t fs_ext2_write(file_system* fs, void* fd, const void* buf, size_t count);


// Initialization, conversion, etc. utils

int fs_ext2_gfs_detect(file_system* fs)
{
	fs_ext2_sb sb;
	sb.ext2_sig = 0;
	fs_ext2_read_sb(fs->drive, &sb);

	if(sb.ext2_sig == 0xEF53)
		return 1;
	return 0;
}

void fs_ext2_gfs_init(file_system* fs)
{
	fs->name = "ext2";

	fs->gdata = kmalloc(sizeof(gfs_ext2_gdata));
	gfs_ext2_gdata* gdat = (gfs_ext2_gdata*)fs->gdata;

	fs_ext2_read_sb(fs->drive, &gdat->sb);

	gdat->bt = (fs_ext2_blkgrp_table){kmalloc(FS_EXT2_SB_BLOCKGROUPS_TOTAL(gdat->sb) * sizeof(fs_ext2_blkgrp)),
			FS_EXT2_SB_BLOCKGROUPS_TOTAL(gdat->sb)};
	fs_ext2_read_blkgrp_table(fs->drive, &gdat->sb, &gdat->bt);

	fs->fd_size = sizeof(gfs_ext2_fd);

	fs->create = &fs_ext2_create;
	fs->unlink = &fs_ext2_unlink;
	fs->rename = &fs_ext2_rename;
	fs->open = &fs_ext2_open;
	fs->close = &fs_ext2_close;
	fs->seek = &fs_ext2_seek;
	fs->read = &fs_ext2_read;
	fs->write = &fs_ext2_write;
}

static inline int fs_ext2_type_fs2ext2_inode(int fs_type)
{
	switch(fs_type)
	{
		case FS_CREATE_TYPE_FILE:	return FS_EXT2_INODE_TYPE_FILE;
		case FS_CREATE_TYPE_DIR:	return FS_EXT2_INODE_TYPE_DIR;
		case FS_CREATE_TYPE_SYMLINK:	return FS_EXT2_INODE_TYPE_SYMLINK;
		case FS_CREATE_TYPE_FIFO:	return FS_EXT2_INODE_TYPE_FIFO;
		case FS_CREATE_TYPE_DEVCHAR:	return FS_EXT2_INODE_TYPE_CHARDEV;
		case FS_CREATE_TYPE_DEVBLK:	return FS_EXT2_INODE_TYPE_BLKDEV;
		case FS_CREATE_TYPE_UNIX_SOCK:	return FS_EXT2_INODE_TYPE_UNIXSOCK;
	}
	return 0;
}
static inline int fs_ext2_type_fs2ext2_typebyte(int fs_type)
{
	switch(fs_type)
	{
		case FS_CREATE_TYPE_FILE:	return 1;
		case FS_CREATE_TYPE_DIR:	return 2;
		case FS_CREATE_TYPE_SYMLINK:	return 7;
		case FS_CREATE_TYPE_FIFO:	return 5;
		case FS_CREATE_TYPE_DEVCHAR:	return 3;
		case FS_CREATE_TYPE_DEVBLK:	return 4;
		case FS_CREATE_TYPE_UNIX_SOCK:	return 6;
	}
	return 0;
}

static inline int fs_ext2_type_ext22fs_inode(uint16_t type_perms)
{
	if(type_perms & FS_EXT2_INODE_TYPE_FILE)	return FS_CREATE_TYPE_FILE;
	if(type_perms & FS_EXT2_INODE_TYPE_DIR)		return FS_CREATE_TYPE_DIR;
	if(type_perms & FS_EXT2_INODE_TYPE_SYMLINK)	return FS_CREATE_TYPE_SYMLINK;
	if(type_perms & FS_EXT2_INODE_TYPE_FIFO)	return FS_CREATE_TYPE_FIFO;
	if(type_perms & FS_EXT2_INODE_TYPE_CHARDEV)	return FS_CREATE_TYPE_DEVCHAR;
	if(type_perms & FS_EXT2_INODE_TYPE_BLKDEV)	return FS_CREATE_TYPE_DEVBLK;
	if(type_perms & FS_EXT2_INODE_TYPE_UNIXSOCK)	return FS_CREATE_TYPE_UNIX_SOCK;
	return 0;
}


// --------------------------------------
// Real interface, including subfunctions
// --------------------------------------

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
		fs_ext2_read_inode(fs->drive, &gdat->sb, &gdat->bt, cur_inode_num, _out);
	else // else read the file inode number that was found above is pointing to
		fs_ext2_read_inode(fs->drive, &gdat->sb, &gdat->bt, cur_inode_num, _out);
	*_num_out = cur_inode_num;
	kfree(it_buf); kfree(path_buf);
	return 0;
}

#define SET_DIRENT_NAMELN(de_nameln, gdat, de)\
{\
	if((gdat).sb.req_features & FS_EXT2_SB_RWFEATURE_DIRS_HAVE_TYPE_FIELD){\
		(de_nameln) = ((de_nameln) <= 255 ? (de_nameln) : 255);\
		(de).name_length_lo = (uint8_t)(de_nameln);\
	}\
	else{\
		(de).name_length_lo = (uint8_t)((de_nameln) & 0xFF);\
		(de).name_length_hi = (uint8_t)(((de_nameln) >> 8) & 0xFF);\
	}\
}
static int fs_ext2_occupy_vacant_dirent(file_system* fs, fs_ext2_inode* dir, uint32_t dir_num,
					uint32_t de_inode, const char* de_name, int type)
{
	gfs_ext2_gdata* gdat = (gfs_ext2_gdata*)fs->gdata;

	#define DIRENT_SZ_WO_NAME (sizeof(it.cur) - sizeof(it.cur.name))
	fs_ext2_dir_iterator it;
	void* it_buf = kmalloc(FS_EXT2_SB_BLOCKSIZE(gdat->sb));
	fs_ext2_iterate_dir_start(fs->drive, &gdat->sb, dir, &it, it_buf);
	while(fs_ext2_iterate_dir_next(fs->drive, &gdat->sb, &it))
	{
		if(it.cur.inode_num == 0 && it.cur.entry_sz >= DIRENT_SZ_WO_NAME + strlen(de_name))
		{ // found an invalid (inode number 0) and big enough direntry, which marks free space
			// form a directory entry
			it.cur.inode_num = de_inode;
			it.cur.type_byte = fs_ext2_type_fs2ext2_typebyte(type);

			uint16_t de_nameln = strlen(de_name);
			SET_DIRENT_NAMELN(de_nameln, *gdat, it.cur);

			// writing directory entry to block buffer
			memcpy(it.blkbuf + it.byteaddr - it.cur.entry_sz, &it.cur, DIRENT_SZ_WO_NAME);
			memcpy(it.blkbuf + it.byteaddr + DIRENT_SZ_WO_NAME - it.cur.entry_sz, de_name, de_nameln);

			// finally flushing changes to both entries to disk
			fs_ext2_write_inode_data(fs->drive, &gdat->sb, dir, it.blki, it.blkbuf);
			kfree(it_buf);
			return 1;
		}

		uint16_t nameln = it.cur.name_length_lo;
		if(!(gdat->sb.req_features & FS_EXT2_SB_RWFEATURE_DIRS_HAVE_TYPE_FIELD))
			nameln |= (uint16_t)it.cur.name_length_hi << (sizeof(it.cur.name_length_lo) * 8);

		uint16_t shrink_size = DIRENT_SZ_WO_NAME + nameln;
		shrink_size += 4 - shrink_size % 4;
		size_t gap = it.cur.entry_sz - shrink_size;

		if(gap >= DIRENT_SZ_WO_NAME + strlen(de_name))
		{ // found a fitting gap for a directory entry
			// shrink the entry to minimal size
			uint16_t it_prevesz = it.cur.entry_sz;
			it.cur.entry_sz = shrink_size;

			uint32_t de_addr;
			if(it.byteaddr < it_prevesz)
				de_addr = it.byteaddr;
			else
				de_addr = it.byteaddr - it_prevesz;
			memcpy(it.blkbuf + de_addr, &it.cur, DIRENT_SZ_WO_NAME);
			memcpy(it.blkbuf + de_addr + DIRENT_SZ_WO_NAME, it.cur.name, nameln);

			// form a directory entry
			fs_ext2_dirent de_in;
			de_in.inode_num = de_inode;
			de_in.type_byte = fs_ext2_type_fs2ext2_typebyte(type);

			uint16_t de_nameln = strlen(de_name);
			SET_DIRENT_NAMELN(de_nameln, *gdat, de_in);
			de_in.entry_sz = gap;

			// writing directory entry to block buffer
			de_addr += it.cur.entry_sz;
			memcpy(it.blkbuf + de_addr, &de_in, DIRENT_SZ_WO_NAME);
			memcpy(it.blkbuf + de_addr + DIRENT_SZ_WO_NAME, de_name, de_nameln);

			// finally flushing changes to both entries to disk
			fs_ext2_write_inode_data(fs->drive, &gdat->sb, dir, it.blki, it.blkbuf);
			kfree(it_buf);
			return 1;
		}
	}
	// no fitting gap was found, allocating and writing a new block
	uint32_t new_blk_grp = fs_ext2_get_inode_pointer(fs->drive, &gdat->sb, dir, it.blki, it.blkbuf) / gdat->sb.blocks_per_group;
	uint32_t new_blk = fs_ext2_find_unalloc_block(fs->drive, &gdat->sb, &gdat->bt,
					new_blk_grp);
	if(!new_blk)
		return 0;

	fs_ext2_mark_alloc_block(fs->drive, &gdat->sb, &gdat->bt, new_blk);
	// TODO: optimise to avoid 2 writes to disk
	fs_ext2_write_inode_block_pointer(fs->drive, &gdat->sb, &gdat->bt, dir,
						dir_num, it.blki, new_blk);

	// setting parent directory size to 1 block
	FS_EXT2_INODE_SET_SIZE(*dir, FS_EXT2_INODE_GET_SIZE(*dir) + FS_EXT2_SB_BLOCKSIZE(gdat->sb));
	fs_ext2_write_inode(fs->drive, &gdat->sb, &gdat->bt, dir_num, dir);

	// operating on first bytes of the buffer as fs_ext2_dirent
	fs_ext2_dirent* first_de = (fs_ext2_dirent*)it_buf;
	first_de->inode_num = de_inode;
	first_de->type_byte = fs_ext2_type_fs2ext2_typebyte(type);

	first_de->entry_sz = FS_EXT2_SB_BLOCKSIZE(gdat->sb);
	uint16_t de_nameln = strlen(de_name);
	SET_DIRENT_NAMELN(de_nameln, *gdat, *first_de);

	memcpy(it_buf + DIRENT_SZ_WO_NAME, de_name, strlen(de_name));

	// writing changes on the buffer
	fs_ext2_write_inode_data(fs->drive, &gdat->sb, dir, it.blki, it.blkbuf);

	kfree(it_buf);
	return 1;
	#undef DIRENT_SZ_WO_NAME
}

static void fs_ext2_sep_fname_and_dir(const char* path, char* path_buf, const char** name_out)
{
	strcpy(path_buf, path);
	// tearing the file name out (last token separating by '/')
	char* i; for(i = path_buf + strlen(path); i >= path_buf; --i){
		if(*i == '/'){
			*i = '\0';
			*name_out = i+1;
			break;
		}
	}
	// if operating on root directory, just remove the whole thing, we are not UNIX
	if(i < path_buf){
		path_buf[0] = '\0';
		*name_out = path;
	}
}

// ++++++++
// create()
// ++++++++
static int fs_ext2_create(file_system* fs, const char* path, int type)
{
	gfs_ext2_gdata* gdat = (gfs_ext2_gdata*)fs->gdata;

	// directory that will store the new file
	fs_ext2_inode fnode; uint32_t fnode_num;

	char* path_buf = kmalloc(strlen(path) + 1);
	const char* de_name;
	fs_ext2_sep_fname_and_dir(path, path_buf, &de_name);
	// don't allow empty names
	if(*de_name == '\0')
		return FS_ERR_INVALID_FILENAME;

	int err = fs_ext2_find_final_inode(fs, path_buf, &fnode, &fnode_num);
	kfree(path_buf);
	if(err)
		return err;

	uint32_t de_inode_num = fs_ext2_find_unalloc_inode(fs->drive, &gdat->sb, &gdat->bt,
					(fnode_num - 1) / gdat->sb.inodes_per_group);
	if(!de_inode_num)
		return FS_ERR_NO_SPACE;

	fs_ext2_inode de_inode;
	memset(&de_inode, 0, sizeof(de_inode));
	// TODO: assign more attributes like creation time
	de_inode.type_perms = FS_EXT2_INODE_PERM_EVERYTHING; // not used here, but for UNIX systems that pay attention to permissions
	de_inode.type_perms |= fs_ext2_type_fs2ext2_inode(type);
	switch(type)
	{
		case FS_CREATE_TYPE_DIR:
			de_inode.hard_links = 2;
			de_inode.sector_cnt = FS_EXT2_SB_BLOCKSECTORS(gdat->sb);
			break;
		default:
			de_inode.hard_links = 1;
			break;
	}

	// write inode and directory entry
	fs_ext2_mark_alloc_inode(fs->drive, &gdat->sb, &gdat->bt, de_inode_num);
	fs_ext2_write_inode(fs->drive, &gdat->sb, &gdat->bt, de_inode_num, &de_inode);
	if(!fs_ext2_occupy_vacant_dirent(fs, &fnode, fnode_num,
						de_inode_num, de_name, type))
		return FS_ERR_NO_SPACE;

	// modify parent directory's hard link count
	fnode.hard_links++;
	fs_ext2_write_inode(fs->drive, &gdat->sb, &gdat->bt, fnode_num, &fnode);

	// if creating directory, add "." and ".." directories into it
	if(type == FS_CREATE_TYPE_DIR)
	{
		if(!fs_ext2_occupy_vacant_dirent(fs, &de_inode, de_inode_num,
							de_inode_num, ".", type))
			return FS_ERR_NO_SPACE;
		if(!fs_ext2_occupy_vacant_dirent(fs, &de_inode, de_inode_num,
							de_inode_num, "..", type))
			return FS_ERR_NO_SPACE;
	}

	return 0;
}


static void fs_ext2_remove_dirent(file_system* fs, fs_ext2_inode* dir, uint32_t dir_num,
						uint32_t de_inode)
{
	gfs_ext2_gdata* gdat = (gfs_ext2_gdata*)fs->gdata;

	fs_ext2_dir_iterator it, it_prev;
	#define DIRENT_SZ_WO_NAME (sizeof(it.cur) - sizeof(it.cur.name))
	it_prev.is_valid = 0;
	void* it_buf = kmalloc(FS_EXT2_SB_BLOCKSIZE(gdat->sb));
	fs_ext2_iterate_dir_start(fs->drive, &gdat->sb, dir, &it, it_buf);

	while(fs_ext2_iterate_dir_next(fs->drive, &gdat->sb, &it))
	{
		if(it.cur.inode_num == de_inode)
		{
			if(it_prev.is_valid && it_prev.blki == it.blki)
			{ // if previous direntry exists and was in the same block, extend it to fill in the gap
				uint16_t prev_esz = it_prev.cur.entry_sz;
				it_prev.cur.entry_sz += it.cur.entry_sz;
				memcpy(it_prev.blkbuf + it_prev.byteaddr - prev_esz, &it_prev.cur, DIRENT_SZ_WO_NAME);
				fs_ext2_write_inode_data(fs->drive, &gdat->sb, dir, it_prev.blki, it_prev.blkbuf);

				// since currenty direntry was joined with previous, previous directory should stay the same
				continue;
			}
			else
			{ // mark the direntry as an empty space
				it.cur.inode_num = 0;
				memcpy(it.blkbuf + it.byteaddr - it.cur.entry_sz, &it.cur, DIRENT_SZ_WO_NAME);
				fs_ext2_write_inode_data(fs->drive, &gdat->sb, dir, it.blki, it.blkbuf);

				void* tbuf = kmalloc(FS_EXT2_SB_BLOCKSIZE(gdat->sb));
				if(it.cur.entry_sz == FS_EXT2_SB_BLOCKSIZE(gdat->sb)
				&& !fs_ext2_get_inode_pointer(fs->drive, &gdat->sb, dir, it.blki+1, tbuf))
				{ // an empty direntry spans accross the whole block, meaning it can be safely freed if it's the last of pointers
					fs_ext2_mark_unalloc_block(fs->drive, &gdat->sb, &gdat->bt,
						fs_ext2_get_inode_pointer(fs->drive, &gdat->sb, dir, it.blki, tbuf));
					fs_ext2_write_inode_block_pointer(fs->drive, &gdat->sb, &gdat->bt,
										dir, dir_num, it.blki, 0);
				}
				kfree(tbuf);
			}
			/* search continues to make sure duplicate entries are removed as well
			 * (in case of hard links to file in the same directories)
			 */
		}
		it_prev = it;
	}

	kfree(it_buf);
	#undef DIRENT_SZ_WO_NAME
}

static void fs_ext2_free_file(file_system* fs, fs_ext2_inode* file, uint32_t file_num)
{
	gfs_ext2_gdata* gdat = (gfs_ext2_gdata*)fs->gdata;
	void* it_buf = kmalloc(FS_EXT2_SB_BLOCKSIZE(gdat->sb));

	// mark all dedicated blocks as free
	uint32_t ptr;
	for(uint32_t blki = 0;
		(ptr = fs_ext2_get_inode_pointer(fs->drive, &gdat->sb, file, blki, it_buf));
		++blki)
	{
		fs_ext2_mark_unalloc_block(fs->drive, &gdat->sb, &gdat->bt, ptr);
	}

	// mark file inode as free
	fs_ext2_mark_unalloc_inode(fs->drive, &gdat->sb, &gdat->bt, file_num);

	kfree(it_buf);
}


// ++++++++
// unlink()
// ++++++++
static int fs_ext2_unlink(file_system* fs, const char* path)
{
	gfs_ext2_gdata* gdat = (gfs_ext2_gdata*)fs->gdata;

	// Finding file inode to unlink
	fs_ext2_inode fnode; uint32_t fnode_num;
	int err = fs_ext2_find_final_inode(fs, path, &fnode, &fnode_num);
	if(err)
		return err;

	// Finding directory to unlink the file from
	char* path_buf = kmalloc(strlen(path) + 1);
	const char* dir_name;
	fs_ext2_sep_fname_and_dir(path, path_buf, &dir_name);

	fs_ext2_inode dirnode; uint32_t dirnode_num;
	err = fs_ext2_find_final_inode(fs, path_buf, &dirnode, &dirnode_num);
	kfree(path_buf);
	if(err)
		return err;

	// remove directory entry of the file from directory
	fs_ext2_remove_dirent(fs, &dirnode, dirnode_num, fnode_num);

	// decrement hardlink count of the directory
	dirnode.hard_links--;
	/* Note: it is assumed that until directory is directly unlinked from it's parent directory,
	*  it's hard links count stays at or above 1.
	*/
	fs_ext2_write_inode(fs->drive, &gdat->sb, &gdat->bt, dirnode_num, &dirnode);

	// decrement hardlink count of the file
	fnode.hard_links--;
	if(!fnode.hard_links
	|| ((fnode.type_perms & FS_EXT2_INODE_TYPE_DIR) && fnode.hard_links == 1) )
	{ // if there is no more duplicate hard links (or it's an empty directory), free the file and all it's blocks
		fs_ext2_free_file(fs, &fnode, fnode_num);
	}
	else
		fs_ext2_write_inode(fs->drive, &gdat->sb, &gdat->bt, fnode_num, &fnode);

	return 0;
}

// ++++++++
// rename()
// ++++++++
static int fs_ext2_rename(file_system* fs, const char* old, const char* _new)
{
	// finding old and new directories for the file
	char* old_buf = kmalloc(strlen(old) + 1);
	char* new_buf = kmalloc(strlen(_new) + 1);
	const char* new_fname;
	fs_ext2_sep_fname_and_dir(old, old_buf, &new_fname);
	fs_ext2_sep_fname_and_dir(_new, new_buf, &new_fname);

	fs_ext2_inode old_dir_node, new_dir_node;
	uint32_t old_dir_num, new_dir_num;
	int err = fs_ext2_find_final_inode(fs, old_buf, &old_dir_node, &old_dir_num);
	kfree(old_buf);
	if(err){
		kfree(new_buf);
		return err;
	}
	err = fs_ext2_find_final_inode(fs, new_buf, &new_dir_node, &new_dir_num);
	if(err){
		kfree(new_buf);
		return err;
	}

	// finding the file itself
	fs_ext2_inode fnode; uint32_t fnode_num;
	err = fs_ext2_find_final_inode(fs, old, &fnode, &fnode_num);
	if(err){
		kfree(new_buf);
		return err;
	}

	// removing old directory entry and inserting the new one
	fs_ext2_remove_dirent(fs, &old_dir_node, old_dir_num, fnode_num);
	if(!fs_ext2_occupy_vacant_dirent(fs, &new_dir_node, new_dir_num,
				fnode_num, new_fname, fs_ext2_type_ext22fs_inode(fnode.type_perms))){
		kfree(new_buf);
		return FS_ERR_NO_SPACE;
	}

	kfree(new_buf);
	return 0;
}

// ++++++
// open()
// ++++++
static int fs_ext2_open(file_system* fs, void* fd, const char* path, int flags)
{
	gfs_ext2_gdata* gdat = (gfs_ext2_gdata*)fs->gdata;
	gfs_ext2_fd* _fd = (gfs_ext2_fd*)fd;

	_fd->ptr.is_buf_init = 0;

	int err = fs_ext2_find_final_inode(fs, path, &_fd->inode, &_fd->inode_num);
	if(err){
		if(err == FS_ERR_DOESNT_EXIST
		&& (flags & FS_OPEN_CREATE) && (flags & FS_OPEN_WRITE))
		{
			err = fs_ext2_create(fs, path, FS_CREATE_TYPE_FILE);
			if(err)
				return err;
			err = fs_ext2_find_final_inode(fs, path, &_fd->inode, &_fd->inode_num);
			if(err)
				return err;
		}
		else
			return err;
	}

	_fd->ptr.blkbuf = kmalloc(FS_EXT2_SB_BLOCKSIZE(gdat->sb));

	if( (flags & FS_OPEN_RECREATE) && (flags & FS_OPEN_WRITE) )
	{
		// mark all dedicated blocks as free, and set corresponding pointers to zero
		uint32_t ptr;
		for(uint32_t blki = 0;
			(ptr = fs_ext2_get_inode_pointer(fs->drive, &gdat->sb, &_fd->inode, blki, _fd->ptr.blkbuf));
			++blki)
		{
			fs_ext2_write_inode_block_pointer(fs->drive, &gdat->sb, &gdat->bt,
								&_fd->inode, _fd->inode_num, blki, 0);
			fs_ext2_mark_unalloc_block(fs->drive, &gdat->sb, &gdat->bt, ptr);
		}
		FS_EXT2_INODE_SET_SIZE(_fd->inode, 0);
		fs_ext2_write_inode(fs->drive, &gdat->sb, &gdat->bt, _fd->inode_num, &_fd->inode);
	}

	_fd->ptr.is_blk_valid = 1;
	if(flags & FS_OPEN_ENDPTR){
		uint32_t blki; for(blki = 0;
			(fs_ext2_get_inode_pointer(fs->drive, &gdat->sb, &_fd->inode, blki, _fd->ptr.blkbuf));
			++blki)
			;
		if(blki)
			--blki;
		else
			_fd->ptr.is_blk_valid = 0;

		_fd->ptr.blki = blki;
		_fd->ptr.byteaddr = FS_EXT2_INODE_GET_SIZE(_fd->inode) % FS_EXT2_SB_BLOCKSIZE(gdat->sb);
		if(_fd->ptr.byteaddr == 0)
			_fd->ptr.is_blk_valid = 0; // pointing to the last, non-allocated block pointer
	}
	else{
		_fd->ptr.blki = 0;
		_fd->ptr.byteaddr = 0;
		if(!fs_ext2_get_inode_pointer(fs->drive, &gdat->sb, &_fd->inode, 0, _fd->ptr.blkbuf))
			_fd->ptr.is_blk_valid = 0;
	}

	return 0;
}

// ++++++
// close()
// ++++++
static void fs_ext2_close(file_system* fs, void* fd)
{
	gfs_ext2_fd* _fd = (gfs_ext2_fd*)fd;
	kfree(_fd->ptr.blkbuf);
}

// +++++
// seek()
// +++++
static long fs_ext2_seek(file_system* fs, void* fd, long offset, int whence)
{
	gfs_ext2_gdata* gdat = (gfs_ext2_gdata*)fs->gdata;
	gfs_ext2_fd* _fd = (gfs_ext2_fd*)fd;

	uint32_t blki, byteaddr;
	// set initial position according to whence
	if(whence & FS_SEEK_CUR){
		blki = _fd->ptr.blki;
		byteaddr = _fd->ptr.byteaddr;
	}
	else if(whence & FS_SEEK_END){
		for(blki = 0;
			(fs_ext2_get_inode_pointer(fs->drive, &gdat->sb, &_fd->inode, blki, _fd->ptr.blkbuf));
			++blki)
			;
		if(blki)
			--blki;
		byteaddr = FS_EXT2_INODE_GET_SIZE(_fd->inode) % FS_EXT2_SB_BLOCKSIZE(gdat->sb);
	}
	else{ // FS_SEEK_BEGIN
		blki = 0;
		byteaddr = 0;
	}

	// adjust the position according to offset
	uint32_t bytes = blki * FS_EXT2_SB_BLOCKSIZE(gdat->sb) + byteaddr;
	if(offset > 0){
		if(bytes > UINT32_MAX - offset)
		{ // handling integer overflow
			offset = UINT32_MAX - bytes;
		}
		if(bytes > FS_EXT2_INODE_GET_SIZE(_fd->inode) - offset)
		{ // handling seeking past file
			offset = FS_EXT2_INODE_GET_SIZE(_fd->inode) - bytes;
		}
	}
	else if(offset < 0){
		if((uint32_t)(-offset) > bytes)
		{ // handling integer underflow (trying to seek before file)
			offset = -bytes;
		}

	}

	bytes += offset;
	_fd->ptr.blki = bytes / FS_EXT2_SB_BLOCKSIZE(gdat->sb);
	_fd->ptr.byteaddr = bytes % FS_EXT2_SB_BLOCKSIZE(gdat->sb);

	return bytes;
}

// +++++
// read()
// +++++
static size_t fs_ext2_read(file_system* fs, void* fd, void* buf, size_t count)
{
	gfs_ext2_gdata* gdat = (gfs_ext2_gdata*)fs->gdata;
	gfs_ext2_fd* _fd = (gfs_ext2_fd*)fd;

	// do not allow reading past file, trimming the result
	uint32_t bytes = _fd->ptr.blki * FS_EXT2_SB_BLOCKSIZE(gdat->sb) + _fd->ptr.byteaddr;
	count = count > FS_EXT2_INODE_GET_SIZE(_fd->inode) - bytes ?
				FS_EXT2_INODE_GET_SIZE(_fd->inode) - bytes :
				count;

	if(!_fd->ptr.is_buf_init){
		// try to read first chunk of data
		if(!fs_ext2_read_inode_data(fs->drive, &gdat->sb, &_fd->inode, _fd->ptr.blki, _fd->ptr.blkbuf)){
			_fd->ptr.is_blk_valid = 0;
			return 0;
		}
	}

	size_t left = count;
	while(left)
	{
		if(!_fd->ptr.is_blk_valid){
			// invalid (unallocated) block - can't read any further
			return count - left;
		}
		if(_fd->ptr.byteaddr == FS_EXT2_SB_BLOCKSIZE(gdat->sb)){
			// read the whole block, moving to next one
			_fd->ptr.blki++;
			_fd->ptr.byteaddr = 0;
			if(!fs_ext2_read_inode_data(fs->drive, &gdat->sb, &_fd->inode, _fd->ptr.blki, _fd->ptr.blkbuf)){
				// can't read any further, triggering previous if condition
				_fd->ptr.is_blk_valid = 0;
				continue;
			}
		}

		size_t tocopy = left <= FS_EXT2_SB_BLOCKSIZE(gdat->sb) - _fd->ptr.byteaddr ? left
											   : FS_EXT2_SB_BLOCKSIZE(gdat->sb) - _fd->ptr.byteaddr;
		memcpy(buf, _fd->ptr.blkbuf + _fd->ptr.byteaddr, tocopy);

		buf += tocopy;
		_fd->ptr.byteaddr += tocopy;
		left -= tocopy;
	}

	return count;
}

// +++++
// write()
// +++++

static size_t fs_ext2_write(file_system* fs, void* fd, const void* buf, size_t count)
{
	gfs_ext2_gdata* gdat = (gfs_ext2_gdata*)fs->gdata;
	gfs_ext2_fd* _fd = (gfs_ext2_fd*)fd;

	size_t left = count;
	while(left)
	{
		if(!_fd->ptr.is_blk_valid){
			// invalid (unallocated) block pointer - allocate a block for it
			uint32_t new_ptr = fs_ext2_find_unalloc_block(fs->drive, &gdat->sb, &gdat->bt,
									(_fd->inode_num - 1) / gdat->sb.inodes_per_group);
			if(new_ptr == ((uint32_t)-1)){ // no space left
				FS_EXT2_INODE_SET_SIZE(_fd->inode, FS_EXT2_INODE_GET_SIZE(_fd->inode) + (count - left));
				fs_ext2_write_inode(fs->drive, &gdat->sb, &gdat->bt, _fd->inode_num, &_fd->inode);
				return count - left;
			}

			// mark block as allocated and write new pointer to inode
			fs_ext2_mark_alloc_block(fs->drive, &gdat->sb, &gdat->bt, new_ptr);
			fs_ext2_write_inode_block_pointer(fs->drive, &gdat->sb, &gdat->bt, &_fd->inode, _fd->inode_num,
								_fd->ptr.blki, new_ptr);
			_fd->ptr.is_blk_valid = 1;
		}
		if(_fd->ptr.byteaddr == FS_EXT2_SB_BLOCKSIZE(gdat->sb)){
			// wrote the whole block, flushing the changes to disk
			fs_ext2_write_inode_data(fs->drive, &gdat->sb, &_fd->inode, _fd->ptr.blki, _fd->ptr.blkbuf);

			// moving to next block
			_fd->ptr.blki++;
			_fd->ptr.byteaddr = 0;
			if(!fs_ext2_read_inode_data(fs->drive, &gdat->sb, &_fd->inode, _fd->ptr.blki, _fd->ptr.blkbuf)){
				// can't write any further, triggering previous if condition
				_fd->ptr.is_blk_valid = 0;
				continue;
			}
		}

		size_t tocopy = left <= FS_EXT2_SB_BLOCKSIZE(gdat->sb) - _fd->ptr.byteaddr ? left
											   : FS_EXT2_SB_BLOCKSIZE(gdat->sb) - _fd->ptr.byteaddr;
		memcpy(_fd->ptr.blkbuf + _fd->ptr.byteaddr, buf, tocopy);

		buf += tocopy;
		_fd->ptr.byteaddr += tocopy;
		left -= tocopy;
	}
	// one more write for the last block
	fs_ext2_write_inode_data(fs->drive, &gdat->sb, &_fd->inode, _fd->ptr.blki, _fd->ptr.blkbuf);

	FS_EXT2_INODE_SET_SIZE(_fd->inode, FS_EXT2_INODE_GET_SIZE(_fd->inode) + count);
	fs_ext2_write_inode(fs->drive, &gdat->sb, &gdat->bt, _fd->inode_num, &_fd->inode);
	return count;
}
