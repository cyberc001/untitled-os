#include "elf.h"

#include "string.h"

#include "../kernlib/kernmem.h"

#include "module.h"

// ------------------------
// Section header functions
// ------------------------
static inline elf_section_header* elf_get_section_header_table(elf_header* header)
{
	return (elf_section_header*)((void*)header + header->sht_off);
}
static inline elf_section_header* elf_get_section(elf_header* header, size_t index)
{
	return elf_get_section_header_table(header) + index;
}

// ----------------------
// String table functions
// ----------------------
static inline char* elf_get_strtable(elf_header* header, uint32_t table)
{
	elf_section_header* stb = elf_get_section(header, table);
	return (char*)header + stb->offset;
}
static inline char* elf_get_section_strtable(elf_header* header)
{
	if(!header->nst_sht_index) // section name string table does not exist
		return NULL;
	return (char*)header + elf_get_section(header, header->nst_sht_index)->offset;
}

// ----------------
// Symbol functions
// ----------------
static elf_symbol* elf_get_symbol(elf_header* header, uint32_t table, uint32_t ind)
{
	elf_section_header* stb = elf_get_section(header, table); // symbol table
	elf_symbol* symb = (elf_symbol*)((void*)header + stb->offset) + ind;
	return symb;
}
static elf_symbol* elf_lookup_symbol(const char* name, module_table* mt,
					elf_section_header** symtable_out, module** module_out)
{
	// iterate through all loaded modules in GMT (global module table)
	for(size_t m = 0; m < mt->module_count; ++m)
	{
		elf_header* header = mt->modules[m].elf_data;
		// iterate through all section headers, choosing ones that are symbol tables
		elf_section_header* sht = elf_get_section_header_table(header);
		elf_section_header* sect = (void*)sht;
		for(uint32_t i = 0; i < header->sht_entry_count;
				++i, sect = (void*)sect + header->sht_entry_size)
		{
			if(sect->type == ELF_SECT_TYPE_SYMTAB || sect->type == ELF_SECT_TYPE_DYNSYM)
			{ // iterate through all symbols in the table
				// section link contains section header index of the string table being used
				char* stb = elf_get_strtable(header, sect->link);
				uint32_t sym_cnt = sect->size / sect->entry_size;

				elf_symbol* sym = (void*)header + sect->offset;
				for(uint32_t j = 0; j < sym_cnt;
					++j, sym = (void*)sym + sect->entry_size)
				{
					char* symname = stb + sym->name;
					if(!strcmp(symname, name)){
						if(symtable_out) *symtable_out = sect;
						if(module_out) *module_out = &mt->modules[m];
						return sym;
					}
				}
			}
		}
	}

	return NULL;
}

static int elf_get_symbol_value(elf_header* header, elf_section_header* symtable, elf_symbol* symbol)
{
	if(!symbol->hdt_index){ // external symbol
		char* stb = elf_get_strtable(header, symtable->link);
		char* name = stb + symbol->name;

		void* val_sym = elf_lookup_symbol(name, &gmt, NULL, NULL); // lookup symbol in symbol string table
		if(!val_sym){
			if(ELF_SYMBOL_INFO_BIND(*symbol) & ELF_SYMBOL_BIND_WEAK)
				return 0;
			return ELF_ERR_EXTERN_SYMBOL_NOT_DEFINED;
		}
		else{
			return (int)val_sym;
		}
	}
	else if(symbol->hdt_index == ELF_SECT_INDEX_ABSOLUTE){ // absolute symbol; value does not change
		return symbol->value;
	}
	else{ // internal symbol
		// FIXME
		/*elf_section_header* val_sh = */elf_get_section(header, symbol->hdt_index);
		//bios_vga_printf("section index: %lu value: %lu offset: %lu\n", symbol->hdt_index, symbol->value, val_sh->offset);
		return (int)((void*)header + symbol->value/* + val_sh->offset*/);
	}
}

// --------------------
// Relocation functions
// --------------------

static int elf_relocate_symbol(elf_header* header, elf_reloc* rel, elf_section_header* reltb)
{
	elf_section_header* target = elf_get_section(header, reltb->info);
	void* target_addr = (void*)header + target->offset;
	uint32_t* target_rel = (uint32_t*)(target_addr + rel->offset);

	int symval = 0;
	if(ELF_RELOC_SYMTAB_ENTRY(*rel)) // reloc is NOT pointing to symbol table entry at index 0
	{
		symval = elf_get_symbol_value(header, elf_get_section(header, reltb->link),
					elf_get_symbol(header, reltb->link, ELF_RELOC_SYMTAB_ENTRY(*rel)));
		if(symval == ELF_ERR_EXTERN_SYMBOL_NOT_DEFINED)
			return ELF_ERR_EXTERN_SYMBOL_NOT_DEFINED;
	}
	//bios_vga_printf("%lu: %lu %d %lu\n", ELF_RELOC_TYPE(*rel), *target_rel, symval, (uint32_t)header);
	switch(ELF_RELOC_TYPE(*rel))
	{
		case ELF_RELOC_TYPE_NONE: // none
			break;
		case ELF_RELOC_TYPE_32: // symbol (S)
			*target_rel = *target_rel + symval;
			break;
		case ELF_RELOC_TYPE_PC32: // symbol - storage unit address/section offset (S - P)
			*target_rel = *target_rel + symval - (uint32_t)target_rel;
			break;
		case ELF_RELOC_TYPE_GLOB_DAT: // symbol (S)
			*target_rel = *target_rel + symval;
			break;
		case ELF_RELOC_TYPE_JMP_SLOT: // symbol (S)
			*target_rel = *target_rel + symval;
			break;
		case ELF_RELOC_TYPE_RELATIVE: // base (B)
			*target_rel = *target_rel + (uint32_t)header;
			break;
		default:
			return ELF_ERR_UNSUPPORTED_RELOCATION_TYPE;
	}
	return 0;
}
static int elf_relocate_symbol_addend(elf_header* header, elf_reloc_addend* rela, elf_section_header* reltb)
{
	elf_section_header* target = elf_get_section(header, reltb->info);
	void* target_addr = (void*)header + target->offset;
	uint32_t* target_rel = (uint32_t*)(target_addr + rela->offset);

	int symval = 0;
	if(ELF_RELOC_SYMTAB_ENTRY(*rela)) // reloc is NOT pointing to symbol table entry at index 0
	{
		symval = elf_get_symbol_value(header, elf_get_section(header, reltb->link),
					elf_get_symbol(header, reltb->link, ELF_RELOC_SYMTAB_ENTRY(*rela)));
		if(symval == ELF_ERR_EXTERN_SYMBOL_NOT_DEFINED)
			return ELF_ERR_EXTERN_SYMBOL_NOT_DEFINED;
	}
	switch(ELF_RELOC_TYPE(*rela))
	{
		case ELF_RELOC_TYPE_NONE: // none
			break;
		case ELF_RELOC_TYPE_32: // symbol (S) + addend (A)
			*target_rel = (uint32_t)(*target_rel + symval + rela->addend);
			break;
		case ELF_RELOC_TYPE_PC32: // symbol (S) + addend (A) - storage unit address/section offset (P)
			*target_rel = (uint32_t)(*target_rel + symval + rela->addend - (uint32_t)target_rel);
			break;
		case ELF_RELOC_TYPE_GLOB_DAT: // symbol (S) + addend (A)
			*target_rel = (uint32_t)(*target_rel + symval + rela->addend);
			break;
		case ELF_RELOC_TYPE_JMP_SLOT: // symbol (S) + addend (A)
			*target_rel = (uint32_t)(*target_rel + symval + rela->addend);
			break;
		case ELF_RELOC_TYPE_RELATIVE: // base (B) + addend (A)
			*target_rel = (uint32_t)(*target_rel + header + rela->addend);
			break;
		default:
			return ELF_ERR_UNSUPPORTED_RELOCATION_TYPE;
	}
	return 0;
}


///////////////////
// Public interface
///////////////////

int elf_read_header(file_system* fs, void* fd, elf_header* header)
{
	fs->read(fs, fd, header, sizeof(elf_header));
	if(header->ident.fields.magic[0] != ELF_MAGIC0
	|| header->ident.fields.magic[1] != ELF_MAGIC1
	|| header->ident.fields.magic[2] != ELF_MAGIC2
	|| header->ident.fields.magic[3] != ELF_MAGIC3)
		return ELF_ERR_INCORRECT_MAGIC;

	#ifdef CPU_I386
	if(header->ident.fields.file_class != ELF_CLASS_32)
		return ELF_ERR_UNSUPPORTED_BITNESS;
	if(header->ident.fields.encoding != ELF_DATA_2LSB) // TODO: add MSB support?
		return ELF_ERR_UNSUPPORTED_ENDIANESS;
	if(header->ident.fields.version != ELF_VERSION_CURRENT)	// TODO: look into newer versions
		return ELF_ERR_UNSUPPORTED_VERSION;
	#else
		#error ELF is not supported for this platform
	#endif
	return 0;
}


int elf_init_addresses(void** elf_file, size_t* elf_file_size)
{
	elf_header* header = *((elf_header**)elf_file);

	elf_section_header* sht = elf_get_section_header_table(header);
	elf_section_header* sect = (void*)sht;
	uint32_t max_addr = 0;
	for(uint32_t i = 0; i < header->sht_entry_count;
			++i, sect = (void*)sect + header->sht_entry_size)
	{
		if(sect->address + sect->size > max_addr)
			max_addr = sect->address + sect->size;
	}

	*elf_file_size = max_addr;
	*elf_file = krealloc(*elf_file, *elf_file_size);

	header = *((elf_header**)elf_file);
	sht = elf_get_section_header_table(header);
	sect = (void*)sht;
	for(uint32_t i = 0; i < header->sht_entry_count;
			++i, sect = (void*)sect + header->sht_entry_size)
	{
		if(sect->address && sect->address != sect->offset){
			memcpy((void*)header + sect->address, (void*)header + sect->offset, sect->size);
		}
	}

	return 0;
}

int elf_init_nobits(void** elf_file, size_t* elf_file_size)
{
	elf_header* header = *((elf_header**)elf_file);

	elf_section_header* sht = elf_get_section_header_table(header);
	elf_section_header* sect = (void*)sht;
	uint32_t add_memory = 0;
	for(uint32_t i = 0; i < header->sht_entry_count;
			++i, sect = (void*)sect + header->sht_entry_size)
	{
		if(sect->type == ELF_SECT_TYPE_NOBITS && sect->size
		&& sect->flags & ELF_SECT_FLAG_ALLOC)
		{
			add_memory += sect->size;
		}
	}

	*elf_file = krealloc(*elf_file, *elf_file_size + add_memory);
	header = *((elf_header**)elf_file);
	sht = elf_get_section_header_table(header);
	sect = (void*)header + sht->offset;

	void* cur_off = *((void**)elf_file) + *elf_file_size;
	*elf_file_size += add_memory;
	for(uint32_t i = 0; i < header->sht_entry_count;
			++i, sect = (void*)sect + header->sht_entry_size)
	{
		if(sect->type == ELF_SECT_TYPE_NOBITS && sect->size
		&& sect->flags & ELF_SECT_FLAG_ALLOC)
		{
			sect->offset = (elf_off)cur_off;
			cur_off += sect->size;
		}
	}

	return 0;
}

int elf_init_relocate(void* elf_file)
{
	elf_header* header = (elf_header*)elf_file;

	elf_section_header* sht = elf_get_section_header_table(header);
	elf_section_header* sect = (void*)sht;
	for(uint32_t i = 0; i < header->sht_entry_count;
			++i, sect = (void*)sect + header->sht_entry_size)
	{
		if(sect->type == ELF_SECT_TYPE_REL)
		{ // relocation table
			elf_reloc* rel = (void*)header + sect->offset;
			for(uint32_t j = 0; j < sect->size / sect->entry_size;
				++j, rel = (void*)rel + sect->entry_size)
			{
				int err;
				if((err = elf_relocate_symbol(header, rel, sect)))
					return err;
			}
		}
		else if(sect->type == ELF_SECT_TYPE_RELA)
		{ // relocation table
			elf_reloc_addend* rela = (void*)header + sect->offset;
			for(uint32_t j = 0; j < sect->size / sect->entry_size;
				++j, rela = (void*)rela + sect->entry_size)
			{
				int err;
				if((err = elf_relocate_symbol_addend(header, rela, sect)))
					return err;
			}
		}
	}
	return 0;
}

void* elf_get_function_gmt(const char* name)
{
	module* md;
	elf_section_header* sect;
	elf_symbol* sym = elf_lookup_symbol(name, &gmt, &sect, &md);
	if(!sym)
		return NULL;
	return (void*)elf_get_symbol_value(md->elf_data, sect, sym);
}
