#ifndef MODULE_H
#define MODULE_H

#include "elf.h"

#include "../kernlib/kernmem.h"

typedef struct{
	void* elf_data; // raw elf file
	size_t elf_data_size;
} module;

typedef struct{
	module* modules;
	size_t module_count;
} module_table;
extern module_table gmt;


/* Loads a module in kernel memory (kernmem/kernmem.h).
*  Internal ELF loading functions use GMT (global module table) for symbol relocation.
*  Returns an ELF file reading/relocating error (defined in elf.h),
*  or 0 on success.
*/
int module_load_kernmem(module* md, file_system* fs, void* fd);

/* Adds a module to the global module table.
*  Features lazy allocation (since GMT is initially NULL and of size of 0).
*/
void module_add_to_gmt(module* md);

#endif
