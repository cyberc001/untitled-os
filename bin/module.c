#include "module.h"

#include "../kernlib/kernmem.h"
#include "../bios/bios_io.h"

int module_load_kernmem(module* md, file_system* fs, void* fd)
{
	elf_header ehd;
	int err = elf_read_header(fs, fd, &ehd);
	if(err) return err;

	md->elf_data_size = fs->get_size(fs, fd);
	md->elf_data = kmalloc(md->elf_data_size);
	fs->seek(fs, fd, 0, FS_SEEK_BEGIN);
	fs->read(fs, fd, md->elf_data, md->elf_data_size);

	err = elf_init_nobits(&md->elf_data, &md->elf_data_size);
	if(err) return err;
	err = elf_init_relocate(md->elf_data);
	if(err) return err;

	const char* test_func = elf_get_function(md->elf_data, "test");
	bios_vga_printf("test function memory offset: %x\n", (void*)test_func - md->elf_data);
	bios_vga_printf("return value: %s\n", ((const char*(*)())test_func)());

	return 0;
}
