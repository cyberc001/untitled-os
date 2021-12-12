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

	pci_setup();
	pic_remap_irqs(0x20, 0x28);

	cpu_interrupt_set(0);
	init_gdt_flat();
	cpu_mode_set(CPU_MODE_PROTECTED);
	cpu_interrupt_init();


	ata_drive drives[4];
	ata_probe(drives);

	file_system _fs;
	fs_scan(&_fs, &drives[0]);
	bios_vga_printf("drive 0 file system: %s\n", _fs.name);

	void* fd = kmalloc(_fs.fd_size);
	_fs.open(&_fs, fd, "test_module.so", FS_OPEN_READ);
	module module_memory;
	module_load_kernmem(&module_memory, &_fs, fd);
	module_add_to_gmt(&module_memory);

	const char* test_func = elf_get_function_gmt("test");
	bios_vga_printf("return value: %s\n", ((const char*(*)())test_func)());
}

