#ifndef EXT2_H
#define EXT2_H

#include <stdint.h>

#include "../dev/ata.h"
#include "fs.h"

// FS manipulation functions

// structure containing ext2 extended superblock fields
typedef struct _fs_ext2_sb_ext
{
	uint32_t first_unres_inode;	// first unreserved inode
	uint16_t inode_size;

	uint16_t parent_block_group;	// group which this superblock is part of (if redundant)

	uint32_t opt_features;		// optional features
	uint32_t req_features;		// required (for r/w) features
	uint32_t write_features;	// required (for w) features

	char fs_id[16];
	char volume_name[16];
	char last_mnt_path[64];

	uint32_t compr_alg;		// compression algorithm used
	uint8_t prealloc_file_blocks;
	uint8_t prealloc_dir_blocks;

	uint16_t reserved;

	char journal_id[16];
	uint32_t journal_inode;
	uint32_t journal_device;
	uint32_t orphan_head;
} fs_ext2_sb_ext;
// ext2 superblock
typedef struct
{
	// Basic superblock fields
	uint32_t inodes_total;
	uint32_t blocks_total;
	uint32_t blocks_su; 		// blocks reserved for superuser
	uint32_t blocks_unalloc; 	// total # of unallocated blocks
	uint32_t inodes_unalloc; 	// total # of unallocated inodes

	uint32_t sb_blockno;		// # of the block containing the superblock

	uint32_t block_logsz;		// log_2(block_size) - 10
	uint32_t frag_logsz;		// log_2(fragment_size) - 10
	uint32_t blocks_per_group;
	uint32_t frags_per_group;
	uint32_t inodes_per_group;

	uint32_t last_mnt_time;
	uint32_t last_write_time;
	uint16_t mnt_cnt;		// number of times volume has been mounted since
					// it's last consistency check
	uint16_t mnt_before_chk;	// number of mounts before a forced consistency check

	uint16_t ext2_sig;		// ext2 signature, 0xef53
	uint16_t state;			// FS state
	uint16_t error_action;		// what to do on FS error

	uint16_t ver_minor;
	uint32_t last_chk_time;		// time of last consistency check
	uint32_t chk_interval;		// interval between forced consistency checks
	uint32_t os_id;			// ID of FS creator's OS
	uint32_t ver_major;

	uint16_t rsblocks_uid;		// User ID that can use reserved blocks
	uint16_t rsblocks_gid;		// Group ID that can use reserved blocks

	struct _fs_ext2_sb_ext;
} fs_ext2_sb;

// Superblock flags
// -- optional feature flags
#define FS_EXT2_SB_FEATURE_PREALLOCATE			0x0001
#define FS_EXT2_SB_FEATURE_AFS_SERVER_INODES		0x0002
#define FS_EXT2_SB_FEATURE_EXT3_JOURNAL			0x0004
#define FS_EXT2_SB_FEATURE_INODE_EXT_ATTR		0x0008
#define FS_EXT2_SB_FEATURE_RESIZABLE_FS			0x0010	// FS can be resized for larger partitions
#define FS_EXT2_SB_FEATURE_DIRS_USE_HASH		0x0020  // directories use hash index
// -- r/w required feature flags
#define FS_EXT2_SB_RWFEATURE_COMPRESSION		0x0001
#define FS_EXT2_SB_RWFEATURE_DIRS_HAVE_TYPE_FIELD	0x0002
#define FS_EXT2_SB_RWFEATURE_NEED_REPLAY_JOURNAL	0x0004
#define FS_EXT2_SB_RWFEATURE_USE_JOURNAL_DEVICE		0x0008
// -- w required feature flags
#define FS_EXT2_SB_WFEATURE_SPARSE_SUPERBLOCKS		0x0001
#define FS_EXT2_SB_WFEATURE_64BIT_FILE_SIZE		0x0002
#define FS_EXT2_SB_WFEATURE_DIRS_USE_BIN_TREES		0x0004

// Superblock calculations
#define FS_EXT2_SB_BLOCKSIZE(sb) ((uint32_t)1024 << (sb).block_logsz)
#define FS_EXT2_SB_BLOCKSECTORS(sb) (FS_EXT2_SB_BLOCKSIZE(sb) / ATA_SECTOR_SIZE)
#define FS_EXT2_SB_BLOCKGROUPS_TOTAL(sb) ((sb).blocks_total / (sb).blocks_per_group + !!((sb).blocks_total % (sb).blocks_per_group))


// ext2 block group descriptor
#pragma pack(push)
#pragma pack(2)
typedef struct
{
	// addresses are given in block no. offset from block group address
	uint32_t block_bitmap_addr;	// address of block usage bitmap
	uint32_t inode_bitmap_addr;	// address of inode usage bitmap
	uint32_t inode_table_addr;

	uint16_t blocks_unalloc;
	uint16_t inodes_unalloc;
	uint16_t dirs_cnt;

	char __padding[14]; // to pad descriptor to 32 bytes
} fs_ext2_blkgrp;
#pragma pack(pop)
// ext2 block group desctriptor table
typedef struct
{
	fs_ext2_blkgrp* groups;
	uint32_t length; // in descriptors
} fs_ext2_blkgrp_table;

// ext2 inode
#pragma pack(push)
#pragma pack(1)
typedef struct
{
	uint16_t type_perms; // type and permissions
	uint16_t uid; // user ID
	uint32_t size_lo; // lower bits of size in bytes

	uint32_t access_time;
	uint32_t creation_time;
	uint32_t modification_time;
	uint32_t deletion_time;

	uint16_t gid;

	uint16_t hard_links;
	uint32_t sector_cnt; // count of disc sectors used by this node's data
	uint32_t flags;
	uint32_t os1; // OS-specific value 1

	uint32_t dbp[12]; // direct block pointers
	uint32_t sibp, dibp, tibp; // singly/doubly/triply indirect block pointers

	uint32_t generation; // used for NFS

	uint32_t acl; // extended attribute block (version >= 1)
	uint32_t size_hi; // upper bits of size in bytes if file, ACL if directory (version >= 1)

	uint32_t frag_addr; // fragment address, in blocks

	union{
		struct{
			uint8_t frag_num, frag_size;
			uint16_t resv;
			uint16_t uid_hi, gid_hi;
			uint32_t resv2;
		} _linux;
		struct{
			uint8_t frag_num, frag_size;
			uint16_t type_perms_hi;
			uint16_t uid_hi, gid_hi;
			uint32_t author_uid; // if 0xFFFFFFFF, normal UID is used
		} _hurd;
		struct{
			uint8_t frag_num, frag_size;
		} _masix;
	} os2;
} fs_ext2_inode;
#pragma pack(pop)

#define FS_EXT2_ROOT_INODE 2

// Inode operations

#define FS_EXT2_INODE_GET_SIZE(inode) ((uint64_t)(inode).size_lo | ((uint64_t)(inode).size_hi << 32))
#define FS_EXT2_INODE_SET_SIZE(inode, size)\
{\
	(inode).size_lo = (size) & 0xFFFFFFFF;\
	(inode).size_hi = ((uint64_t)(size) >> 32) & 0xFFFFFFFF;\
}


// inode flags
// -- type and permissions
// ---- type
#define FS_EXT2_INODE_TYPE_FIFO		0x1000
#define FS_EXT2_INODE_TYPE_CHARDEV	0x2000  // character device
#define FS_EXT2_INODE_TYPE_DIR		0x4000
#define FS_EXT2_INODE_TYPE_BLKDEV	0x6000  // block device
#define FS_EXT2_INODE_TYPE_FILE		0x8000
#define FS_EXT2_INODE_TYPE_SYMLINK	0xA000	// symbolic link
#define FS_EXT2_INODE_TYPE_UNIXSOCK	0xC000	// UNIX socket

// ---- permissions
// (O - other, G - group, U - user, X - execute, W - write, R - read)
#define FS_EXT2_INODE_PERM_OX		0x001
#define FS_EXT2_INODE_PERM_OW		0x002
#define FS_EXT2_INODE_PERM_OR		0x004
#define FS_EXT2_INODE_PERM_GX		0x008
#define FS_EXT2_INODE_PERM_GW		0x010
#define FS_EXT2_INODE_PERM_GR		0x020
#define FS_EXT2_INODE_PERM_UX		0x040
#define FS_EXT2_INODE_PERM_UW		0x080
#define FS_EXT2_INODE_PERM_UR		0x100
#define FS_EXT2_INODE_PERM_STICKY_BIT	0x200
#define FS_EXT2_INODE_PERM_SET_GID	0x400
#define FS_EXT2_INODE_PERM_SET_UID	0x800

#define FS_EXT2_INODE_PERM_EVERYTHING	(FS_EXT2_INODE_PERM_OX | FS_EXT2_INODE_PERM_OW | FS_EXT2_INODE_PERM_OR |\
					 FS_EXT2_INODE_PERM_GX | FS_EXT2_INODE_PERM_GW | FS_EXT2_INODE_PERM_GR |\
 					 FS_EXT2_INODE_PERM_UX | FS_EXT2_INODE_PERM_UW | FS_EXT2_INODE_PERM_UR)

// -- flags (flags field)
#define FS_EXT2_INODE_SYNC			0x00008
#define FS_EXT2_INODE_IMMUTABLE			0x00010 // content cannot be changed
#define FS_EXT2_INODE_APPEND_ONLY		0x00020
#define FS_EXT2_INODE_NODUMP			0x00040 // file is not included in "dump" command
#define FS_EXT2_INODE_DONTUPDATE_ACCESSTIME	0x00080
#define FS_EXT2_INODE_HASHIND_DIR		0x10000 // hash indexed directory
#define FS_EXT2_INODE_AFS_DIR			0x20000
#define FS_EXT2_INODE_JOURNAL_FILE_DATA		0x40000

// inode calculations
#define FS_EXT2_INODE_SIZE(inode) (( ((uint64_t)(inode).size_hi) << (sizeof((inode).size_hi) * 8)) | ((uint64_t)(inode).size_lo))

typedef struct{
	uint32_t inode_num;
	uint16_t entry_sz; // total size of this directory entry
	uint8_t name_length_lo;
	union{
		uint8_t type_byte; 	// only if feature bit for "directory entries have file type byte"
		uint8_t name_length_hi;	// is set, otherwise upper 8 bits of name length
	};
	char name[256];
} fs_ext2_dirent;
typedef struct{
	fs_ext2_inode* dir;	// directory being iterated through

	// addresses for reading directory entries:
	uint32_t byteaddr;		// current byte address (relative to current block)
	uint32_t blki;			// current block index (in directory's inode block pointers)
	void* blkbuf;			// current block buffer

	fs_ext2_dirent cur;	// current entry
	uint8_t is_valid;	// if iterator is valid
				// (set to 1 on initialization, set to 0 when there is no more directory data)
} fs_ext2_dir_iterator;


// ----------------------
// Public read interface
// ----------------------

void fs_ext2_read_sb(ata_drive* drive, fs_ext2_sb* sb);

/* table is considered already allocated and it's size set to FS_EXT2_SB_BLOCKGROUPS_TOTAL, ex.:
 * fs_ext2_blkgrp_table bgrp_table = {malloc(FS_EXT2_SB_BLOCKGROUPS_TOTAL(sb) * sizeof(fs_ext2_blkgrp)),
 * 					FS_EXT2_SB_BLOCKGROUPS_TOTAL(sb)};
*/
void fs_ext2_read_blkgrp_table(ata_drive* drive, fs_ext2_sb* sb, fs_ext2_blkgrp_table* table);

void fs_ext2_read_inode(ata_drive* drive, fs_ext2_sb* sb, fs_ext2_blkgrp_table* bt,
				uint32_t inode_num, fs_ext2_inode* _out);


uint32_t fs_ext2_get_inode_pointer(ata_drive* drive, fs_ext2_sb* sb, fs_ext2_inode* inode,
					uint32_t bind, void* buf);
/* Return value:
*  0 if final address of the block in inode that is being read is zero,
*  1 otherwise.
*/
int fs_ext2_read_inode_data(ata_drive* drive, fs_ext2_sb* sb, fs_ext2_inode* inode,
					uint32_t bind, void* bout);


/* pbuf should be allocated beforehand, of size FS_EXT2_SB_BLOCKSIZE. */
void fs_ext2_iterate_dir_start(ata_drive* drive, fs_ext2_sb* sb, fs_ext2_inode* inode,
					fs_ext2_dir_iterator* it, void* pbuf);
/* Return value:
*  0 if no more blocks of directory entries have left,
*  1 otherwise.
*/
int fs_ext2_iterate_dir_next(ata_drive* drive, fs_ext2_sb* sb, fs_ext2_dir_iterator* it);

/* Return value:
*  address of an unallocated block withing the group,
*  (uint32_t)-1 if no free blocks were found.
*/
uint32_t fs_ext2_find_grp_unalloc_block(ata_drive* drive, fs_ext2_sb* sb,
						fs_ext2_blkgrp_table* bt, uint32_t blkgrp);
/* Return value:
*  address of an unallocated block (trying to find one within or ahead the specified group first),
*  (uint32_t)-1 if no free block were found.
*  Note: if no group is preferred, specify blkgrp_start as 0.
*/
uint32_t fs_ext2_find_unalloc_block(ata_drive* drive, fs_ext2_sb* sb,
						fs_ext2_blkgrp_table* bt, uint32_t blkgrp_start);

/* Return value:
*  inode number within the group,
*  0 if no free inodes were found.
*/
uint32_t fs_ext2_find_grp_unalloc_inode(ata_drive* drive, fs_ext2_sb* sb,
						fs_ext2_blkgrp_table* bt, uint32_t blkgrp);
/* Return value:
*  global inode number (trying to find one within or ahead of the specified group first),
*  0 if no free inodes were found.
*/
uint32_t fs_ext2_find_unalloc_inode(ata_drive* drive, fs_ext2_sb* sb,
						fs_ext2_blkgrp_table* bt, uint32_t blkgrp_start);



// ----------------------
// Public write interface
// ----------------------

void fs_ext2_write_sb(ata_drive* drive, fs_ext2_sb* sb);

/* technically the whole sector where the index resides is updated,
*  but it's not adviced to rely upon, especially if more complicated
*  (esp. "lazy writing") policies will be involved in the future.
*/
void fs_ext2_write_blkgrp_table_index(ata_drive* drive, fs_ext2_sb* sb,
					fs_ext2_blkgrp_table* bt, uint32_t i);

void fs_ext2_mark_alloc_inode(ata_drive* drive, fs_ext2_sb* sb,
					fs_ext2_blkgrp_table* bt, uint32_t inode_num);
void fs_ext2_mark_alloc_block(ata_drive* drive, fs_ext2_sb* sb,
					fs_ext2_blkgrp_table* bt, uint32_t block_num);
void fs_ext2_mark_unalloc_inode(ata_drive* drive, fs_ext2_sb* sb,
					fs_ext2_blkgrp_table* bt, uint32_t inode_num);
void fs_ext2_mark_unalloc_block(ata_drive* drive, fs_ext2_sb* sb,
					fs_ext2_blkgrp_table* bt, uint32_t block_num);


void fs_ext2_write_inode(ata_drive* drive, fs_ext2_sb* sb, fs_ext2_blkgrp_table* bt,
				uint32_t inode_num, fs_ext2_inode* _in);

/* writes out inode data if accessing any DBP.
*  Return value:
*  0 if there was a failed attempt of allocating memory for a SIBP/TIBP/DIBP,
*  1 otherwise (no attempt or successful allocation).
*/
int fs_ext2_write_inode_block_pointer(ata_drive* drive, fs_ext2_sb* sb, fs_ext2_blkgrp_table* bt,
					fs_ext2_inode* inode, uint32_t inode_num, uint32_t bind, uint32_t ptr);
/* Return value:
*  0 if final address of the block in inode that is being written is zero,
*  1 otherwise.
*/
int fs_ext2_write_inode_data(ata_drive* drive, fs_ext2_sb* sb, fs_ext2_inode* inode,
					uint32_t bind, void* bin);


// ---------
// GFS setup
// ---------

int fs_ext2_gfs_detect(file_system* fs);
void fs_ext2_gfs_init(file_system* fs);

#endif


