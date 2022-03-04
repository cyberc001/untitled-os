#ifndef MODULE_H
#define MODULE_H

#include "../kernlib/kernmem.h"
#include "../fs/fs.h"

typedef struct{
	char** dependencies;
} module_desc;

typedef struct{
	const char* name;
	module_desc desc;

	void* elf_data; // raw elf file
	size_t elf_data_size;
} module;

typedef struct{
	module* modules;
	size_t module_count;
} module_table;
extern module_table gmt;


// Module loader API symbol table
typedef struct{
	uint64_t* symbols;
	const char** names;
	size_t length;
} module_loader_api;
extern module_loader_api gmapi;

#define MODULE_ERR_DEPENDENCIES_NOT_FOUND	-100

/* Fills module loader API symbol table with data.
*/
void module_init_api();

/* Loads a module in kernel memory (kernmem/kernmem.h).
*  Internal ELF loading functions use GMT (global module table) for symbol relocation.
*  Returns an ELF file reading/relocating error (defined in elf.h),
*  module interface error (defined here), or 0 on success.
*  bin_fd - binary file descriptor, desc_fd - description file description
*/
int module_load_kernmem(module* md, file_system* fs, void* bin_fd, void* desc_fd);

/* Adds a module to the global module table.
*  Features lazy allocation (since GMT is initially NULL and of size of 0).
*/
void module_add_to_gmt(module* md);

#endif
