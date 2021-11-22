#include "bios/bios_io.h"
#include "kernlib/kernmem.h"
#include "kernlib/kerndefs.h"

#include "cpu/pci.h"
#include "dev/pio.h"
#include "dev/ata.h"

#include "fs/ext2.h"
#include "fs/fs.h"

void kernel_main(void)
{
	bios_vga_init();

	pci_setup();
	ata_drive drives[4];
	ata_probe(drives);

	file_system _fs;
	fs_scan(&_fs, &drives[0]);
	bios_vga_printf("drive 0 file system: %s\n", _fs.name);

	/*fs_ext2_sb sb;
	fs_ext2_read_sb(&drives[0], &sb);
	bios_vga_printf("ext2 signature: 0x%X\n", (unsigned)sb.ext2_sig);

	fs_ext2_blkgrp_table bgrp_table = {kmalloc(FS_EXT2_SB_BLOCKGROUPS_TOTAL(sb) * sizeof(fs_ext2_blkgrp)),
						FS_EXT2_SB_BLOCKGROUPS_TOTAL(sb)};
	fs_ext2_read_blkgrp_table(&drives[0], &sb, &bgrp_table);

	fs_ext2_inode ie1;
	fs_ext2_read_inode(&drives[0], &sb, &bgrp_table, 2, &ie1);

	fs_ext2_dir_iterator ie1_it;
	void* ie1_buf = kmalloc(FS_EXT2_SB_BLOCKSIZE(sb));
	fs_ext2_iterate_dir_start(&drives[0], &sb, &ie1, &ie1_it, ie1_buf);
	while(fs_ext2_iterate_dir_next(&drives[0], &sb, &ie1_it))
	{
		bios_vga_printf("%d %d %lu %s\n", ie1_it.cur.name_length_lo, ie1_it.cur.entry_sz, ie1_it.cur.inode_num, ie1_it.cur.name);
	}*/
}

