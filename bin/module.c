#include "module.h"

#include "../kernlib/kernmem.h"
#include "../bios/bios_io.h"

module_table gmt = {NULL, 0};

int module_load_kernmem(module* md, file_system* fs, void* fd)
{
	elf_header ehd;
	int err = elf_read_header(fs, fd, &ehd);
	if(err) return err;

	md->elf_data_size = fs->get_size(fs, fd);
	md->elf_data = kmalloc(md->elf_data_size);
	fs->seek(fs, fd, 0, FS_SEEK_BEGIN);
	fs->read(fs, fd, md->elf_data, md->elf_data_size);

	err = elf_init_addresses(&md->elf_data, &md->elf_data_size);
	if(err) return err;
	err = elf_init_nobits(&md->elf_data, &md->elf_data_size);
	if(err) return err;
	err = elf_init_relocate(md->elf_data);
	if(err) return err;

	return 0;
}

void module_add_to_gmt(module* md)
{
	gmt.module_count++;

	if(!gmt.modules)
		gmt.modules = kmalloc(sizeof(module));
	else
		gmt.modules = krealloc(gmt.modules, gmt.module_count * sizeof(module));

	gmt.modules[gmt.module_count-1] = *md;
}
