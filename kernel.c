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
	_fs.drive = &drives[0];
	fs_ext2_gfs_init(&_fs);

	//bios_vga_printf("create return code: %d\n", _fs.create(&_fs, "test", FS_CREATE_TYPE_DIR));
	//bios_vga_printf("unlink return code: %d\n", _fs.unlink(&_fs, "test"));
	//bios_vga_printf("rename return code: %d\n", _fs.rename(&_fs, "readme", "do_not_read_me"));
	void* fd = kmalloc(_fs.fd_size);
	bios_vga_printf("open return code: %d\n", _fs.open(&_fs, fd, "readme", FS_OPEN_WRITE));
	bios_vga_printf("seek return code: %ld\n", _fs.seek(&_fs, fd, -200, FS_SEEK_END));

	char* test_buf = kmalloc(501);
	size_t r = _fs.read(&_fs, fd, test_buf, 200);
	bios_vga_printf("read bytes: %lu\n", r);
	test_buf[r] = '\0';
	bios_vga_printf("%s\n", test_buf);

	bios_vga_printf("wrote bytes: %lu\n", _fs.write(&_fs, fd, "what a shame.\n", strlen("what a shame.\n")));

	fs_ext2_sb sb;
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
	}

	/*
	// test file read
	fs_ext2_read_inode(&drives[0], &sb, &bgrp_table, 12, &ie1);
	uint32_t i = 0; for(; (i+1) * FS_EXT2_SB_BLOCKSIZE(sb) < FS_EXT2_INODE_SIZE(ie1); ++i){
		fs_ext2_read_inode_data(&drives[0], &sb, &ie1, i, ie1_buf);
		for(size_t j = 0; j < FS_EXT2_SB_BLOCKSIZE(sb); ++j) bios_vga_putchar(((char*)ie1_buf)[j]);
	}
	fs_ext2_read_inode_data(&drives[0], &sb, &ie1, i, ie1_buf);
	for(size_t j = 0; j < FS_EXT2_INODE_SIZE(ie1) % FS_EXT2_SB_BLOCKSIZE(sb); ++j) bios_vga_putchar(((char*)ie1_buf)[j]);*/
}

