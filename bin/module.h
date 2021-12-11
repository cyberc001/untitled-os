#ifndef MODULE_H
#define MODULE_H

#include "elf.h"

typedef struct{
	void* elf_data; // raw elf file
	size_t elf_data_size;
} module;

/* Loads a module in kernel memory (kernmem/kernmem.h).
*  Should be used only on 1st loaded mod that provides an interface
*  for allocating memory in both kernel and user-space.
*  Returns an ELF file reading/relocating error (defined in elf.h),
*  or 0 on success.
*/
int module_load_kernmem(module* md, file_system* fs, void* fd);

#endif
