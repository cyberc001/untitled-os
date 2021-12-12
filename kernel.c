#include "bios/bios_io.h"
#include "kernlib/kernmem.h"
#include "kernlib/kerndefs.h"

#include "cpu/pci.h"
#include "cpu/cpu_int.h"
#include "cpu/cpu_mode.h"

#include "cpu/x86/gdt.h"
#include "cpu/x86/pic.h"

#include "fs/fs.h"

#include "bin/module.h"

void kernel_main(void)
{
	bios_vga_init();

	pic_remap_irqs(0x20, 0x28);

	cpu_interrupt_set(0);
	init_gdt_flat();
	cpu_mode_set(CPU_MODE_PROTECTED);
	cpu_interrupt_init();

	pci_setup();
	ata_drive drives[4];
	ata_probe(drives);

	file_system _fs;
	fs_scan(&_fs, &drives[0]);
	bios_vga_printf("drive 0 file system: %s\n", _fs.name);

	void* fd = kmalloc(_fs.fd_size);
	_fs.open(&_fs, fd, "test_module.so", FS_OPEN_READ);
	module module_memory;
	module_load_kernmem(&module_memory, &_fs, fd);

	/*void* dit = kmalloc(_fs.dit_size);
	_fs.dir_iter_start(&_fs, dit, "");
	file_system_dirent dent = {.name = kmalloc(256), .name_max = 256};
	while(_fs.dir_iter_next(&_fs, dit, &dent))
	{
		bios_vga_printf("%u %s\n", dent.type, dent.name);
	}*/
}

