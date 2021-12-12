#ifndef ELF_H
#define ELF_H

#include <stdint.h>

#include "../fs/fs.h"

typedef uint16_t elf_half;
typedef uint32_t elf_off;
typedef uint32_t elf_addr;
typedef uint32_t elf_word;
typedef int32_t elf_sword;

typedef struct{
	union{
		uint8_t bytes[16];
		struct{
			uint8_t magic[4];
			uint8_t file_class;
			uint8_t encoding;
			uint8_t version;
			uint8_t pad_start;
		} fields;
	} ident;
	elf_half type;
	elf_half machine;
	elf_word version;
	elf_addr entry;
	elf_off pht_off;		// program header table offset
	elf_off sht_off;		// section header table offset
	elf_word flags;
	elf_half header_size;
	elf_half pht_entry_size;	// size of 1 entry in file program's header table
	elf_half pht_entry_count;
	elf_half sht_entry_size;	// size of 1 section header
	elf_half sht_entry_count;
	elf_half nst_sht_index; 	// name string table index (in section header table)
} elf_header;

#define ELF_MAGIC0 0x7F
#define ELF_MAGIC1 'E'
#define ELF_MAGIC2 'L'
#define ELF_MAGIC3 'F'

#define ELF_CLASS_NONE	0	// Invalid class
#define ELF_CLASS_32	1
#define ELF_CLASS_64	2

#define ELF_DATA_NONE	0
#define ELF_DATA_2LSB	1	// big-endian
#define ELF_DATA_2MSB	2	// little-endian

#define ELF_VERSION_INVALID	0
#define ELF_VERSION_CURRENT	1

typedef struct{
	elf_word name;
	elf_word type;
	elf_word flags;
	elf_addr address;
	elf_off offset;
	elf_word size;		// section's size in bytes
	elf_word link;
	elf_word info;
	elf_word addralign;
	elf_word entry_size; 	// inner entry size (symbol tables, etc.)
} elf_section_header;

#define ELF_SECT_TYPE_INACTIVE		0
#define ELF_SECT_TYPE_PROGBITS		1	// format defined by program
#define ELF_SECT_TYPE_SYMTAB		2	// symbol table for link editing
#define ELF_SECT_TYPE_STRTAB		3	// string table
#define ELF_SECT_TYPE_RELA		4	// relocation entries with explicit addends
#define ELF_SECT_TYPE_HASH		5	// symbol hash table
#define ELF_SECT_TYPE_DYNAMIC		6	// information for dynamic linking
#define ELF_SECT_TYPE_NOTE		7
#define ELF_SECT_TYPE_NOBITS		8	// does not occupy file space, otherwise same as PROGBITS
#define ELF_SECT_TYPE_REL		9	// relocation entries without explicit addends
#define ELF_SECT_TYPE_SHLIB		10	// reserved
#define ELF_SECT_TYPE_DYNSYM		11	// minial set of linking symbols

#define ELF_SECT_FLAG_WRITE		0x1	// section should be writable in runtime
#define ELF_SECT_FLAG_ALLOC		0x2	// section resides in memory in runtime
#define ELF_SECT_FLAG_EXECINSTR		0x4	// section contains executable machine instructions

// special section indexes (affect symbols defined relative to these sections)
#define ELF_SECT_INDEX_ABSOLUTE		0xFFF1	// symbols are not affected by relocation
#define ELF_SECT_INDEX_COMMON		0xFFF2	// common symbols (ex. unallocated C external variables)

typedef struct{
	elf_word name;		// in symbol string table
	elf_addr value;		// "value" of associated symbol: absolute value, address, etc.
	elf_word size;
	uint8_t info;		// symbol's type and binding attributes
	uint8_t other;		// holds 0; reserved
	elf_half hdt_index;	// relevant section header table index
} elf_symbol;

#define ELF_SYMBOL_INFO_BIND(entry) ((entry).info >> 4)
#define ELF_SYMBOL_INFO_TYPE(entry) ((entry).info & 0xF)

#define ELF_SYMBOL_BIND_LOCAL		0
#define ELF_SYMBOL_BIND_GLOBAL		1
#define ELF_SYMBOL_BIND_WEAK		2	// same as global bind, but lower precedence

#define ELF_SYMBOL_TYPE_NONE		0
#define ELF_SYMBOL_TYPE_OBJECT		1	// data object: a variable, an array, etc.
#define ELF_SYMBOL_TYPE_FUNC		2	// a function or other executable object
#define ELF_SYMBOL_TYPE_SECTION		3
#define ELF_SYMBOL_TYPE_FILE		4	// source file associated with object file

typedef struct{
	elf_addr offset; 	// relative position of symbol being relocated within its section
	elf_word info; 		// upper bytes: entry in symtable to which relocation applies
				// lower byte: type of relocation to apply
} elf_reloc;
typedef struct{
	elf_addr offset;
	elf_word info;
	elf_sword addend;	// a constant addend for computing value stored in relocatable field
} elf_reloc_addend;

#define ELF_RELOC_SYMTAB_ENTRY(rel) ((rel).info >> 8)
#define ELF_RELOC_TYPE(rel) ((rel).info & 0xFF)

#define ELF_RELOC_TYPE_NONE		0
#define ELF_RELOC_TYPE_32		1
#define ELF_RELOC_TYPE_PC32		2
#define ELF_RELOC_TYPE_GOT32		3
#define ELF_RELOC_TYPE_PLT32		4
#define ELF_RELOC_TYPE_COPY		5
#define ELF_RELOC_TYPE_GLOB_DAT		6
#define ELF_RELOC_TYPE_JMP_SLOT		7
#define ELF_RELOC_TYPE_RELATIVE		8
#define ELF_RELOC_TYPE_GOTOFF		9
#define ELF_RELOC_TYPE_GOTPC		10

typedef struct{
	elf_word type;
	elf_off offset;
	elf_addr vaddr;	// position-dependant code address
	elf_addr paddr; // ???
	elf_word file_size;
	elf_word mem_size;
	elf_word flags;
	elf_word align;
} elf_program_header;


// ----------------
// ELF error codes:
// ----------------

#define ELF_ERR_INCORRECT_MAGIC			-1	// Incorrect magic fields of ident.
#define ELF_ERR_UNSUPPORTED_BITNESS		-2
#define ELF_ERR_UNSUPPORTED_ENDIANESS		-3
#define ELF_ERR_UNSUPPORTED_VERSION		-4

#define ELF_ERR_EXTERN_SYMBOL_NOT_DEFINED	-5

#define ELF_ERR_UNSUPPORTED_RELOCATION_TYPE	-6


// ---------------------
// ELF public interface:
// ---------------------

/* fd should be a file descriptor of an open file,
*  byte (seek) pointer set to the beginning of the file.
*  Returns a negative value if an error has occured,
*  0 otherwise.
*  Note: this is the only function that does not operate on ELF file
*  contents directly in memory (for purpose of not loading incompatible
*  ELF files in memory)
*/
// TODO: pay attention to failing to read the required amount of data
int elf_read_header(file_system* fs, void* fd, elf_header* header);

int elf_init_nobits(void** elf_file, size_t* elf_file_size);
int elf_init_relocate(void* elf_file);

void* elf_get_function_gmt(const char* name);

#endif
