#include <stdint.h>
#include <stddef.h>
#include <stivale2.h>

#include "dev/uart.h"

#include "kernlib/kernmem.h"
#include "kernlib/kerndefs.h"

#include "cpu/pci.h"
#include "cpu/cpu_int.h"
#include "cpu/cpu_mode.h"

#include "cpu/cpu_init.h"

#include "fs/fs.h"
#include "bin/module.h"
#include "bin/elf.h"

void kernel_main();

// Initial stack
static uint8_t stack[8192];

// Terminal tag (basically a NULL-terminator for list of stivale tags)
static struct stivale2_header_tag_terminal terminal_hdr_tag =
{
	.tag = {
		.identifier = STIVALE2_HEADER_TAG_TERMINAL_ID,
		.next = 0
    	},
	.flags = 0
};

// Framebuffer header tag (defining custom graphical framebuffer, instead
// of text mode)
static struct stivale2_header_tag_framebuffer framebuffer_hdr_tag =
{
	.tag = {
		.identifier = STIVALE2_HEADER_TAG_FRAMEBUFFER_ID,
		.next = (uint64_t)&terminal_hdr_tag
	},
	.framebuffer_width  = 0,
	.framebuffer_height = 0,
	.framebuffer_bpp    = 0
};

// Putting a header structure to stivale2 ELF section
__attribute__((section(".stivale2hdr"), used))
static struct stivale2_header stivale_hdr =
{
	// No alternative entry point
	.entry_point = 0,
	// Pointer to stack
	.stack = (uintptr_t)stack + sizeof(stack),
	// Bit:		Description:
	// 1		return pointers to the higher half
	// 2		enable protected memory ranges
	// 3		enable fully virtual kernel mappings
	// 4		disable "some deprecated feature"?
	.flags = (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4),
	// head of linked list of headers:
   	.tags = (uintptr_t)&framebuffer_hdr_tag
};

// scan for tags wanted from bootloader
void* stivale2_get_tag(struct stivale2_struct* stivale2_struct, uint64_t id)
{
	struct stivale2_tag* current_tag = (void*)stivale2_struct->tags;
	while(current_tag)
	{
		if(current_tag->identifier == id)
			return current_tag;
		current_tag = (void*)current_tag->next;
	}
	return NULL;
}

// First entry point
void _start(struct stivale2_struct* stivale2_struct)
{
	struct stivale2_struct_tag_terminal* term_str_tag;
	term_str_tag = stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_TERMINAL_ID);
	if(!term_str_tag)
		uart_printf("Couldn't find terminal tag!");
	//void *term_write_ptr = (void *)term_str_tag->term_write;
	//void (*term_write)(const char *string, size_t length) = term_write_ptr;
	//term_write("Hello World", 11);

	kernel_main(stivale2_struct);

	for (;;)
        	asm ("hlt");
}

void kernel_main(struct stivale2_struct* stivale2_struct)
{
	uart_printf("Initializing PCI\r\n");
	pci_setup();

	cpu_interrupt_set(0);
	uart_printf("** Generic CPU initialization **\r\n");
	cpu_init();
	uart_printf("** End of genertic CPU initializtaion **\r\n");
	uart_printf("Entering protected mode\r\n");
	cpu_mode_set(CPU_MODE_PROTECTED);
	uart_printf("Initializing interrupts\r\n");
	cpu_interrupt_init();
	uart_printf("Enabling interrupts\r\n");
	cpu_interrupt_set(1);

	uart_printf("Probing ATA decives\r\n");
	ata_drive drives[4];
	uart_printf("%lu ATA drives detected\r\n", ata_probe(drives));

	file_system _fs;
	uart_printf("Detecting file system of drive 0\r\n");
	fs_scan(&_fs, &drives[0]);
	uart_printf("drive 0 file system: %s\r\n", _fs.name);

	module_init_api();

	module module_vmemory = {.name = "modload_memory"};
	void *modfd = kmalloc(_fs.fd_size), *descfd = kmalloc(_fs.fd_size);
	_fs.open(&_fs, modfd, "vmemory.so", FS_OPEN_READ);
	_fs.open(&_fs, descfd, "vmemory.dsc", FS_OPEN_READ);
	uart_printf("loaded memory virtualization module with code %d\r\n", module_load_kernmem(&module_vmemory, &_fs, modfd, descfd));
	module_add_to_gmt(&module_vmemory);
	uart_puts("\r\n");

	struct stivale2_struct_tag_memmap *mmap_struct_tag = stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_MEMMAP_ID);
	uint64_t memory_limit = 0;
	for(uint64_t i = 0; i < mmap_struct_tag->entries; ++i)
		if(mmap_struct_tag->memmap[i].base + mmap_struct_tag->memmap[i].length > memory_limit)
			memory_limit = mmap_struct_tag->memmap[i].base + mmap_struct_tag->memmap[i].length;
	uart_printf("Available memory: %lu\r\n", memory_limit);

	uart_printf("Initializing memory virtualization module\r\n");
	int(*vmemory_init)() = elf_get_function_module(&module_vmemory, "init");
	vmemory_init(memory_limit);

	uint64_t(*vmemory_get_mem_unit_size)() = elf_get_function_module(&module_vmemory, "get_mem_unit_size");
	uint64_t vmemory_mem_unit_size = vmemory_get_mem_unit_size();

	uart_printf("Identity mapping the module loader\r\n");
	int(*vmemory_map_phys)() = elf_get_function_module(&module_vmemory, "map_phys");
	void* kmem_heap_end = kmem_get_heap_end();
	vmemory_map_phys(KERN_HEAP_BASE, KERN_HEAP_BASE, (kmem_heap_end - KERN_HEAP_BASE + (vmemory_mem_unit_size - 1)) / (vmemory_mem_unit_size));

	// map kernel image:
	struct stivale2_struct_tag_kernel_base_address *kbase_tag = stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_KERNEL_BASE_ADDRESS_ID);
	vmemory_map_phys((void*)kbase_tag->virtual_base_address, (void*)kbase_tag->physical_base_address, (20 * 1024 * 1024) / vmemory_mem_unit_size, 0);

	uart_printf("Enabling memory virtualization module\r\n");
	int(*vmemory_enable)() = elf_get_function_module(&module_vmemory, "enable");
	vmemory_enable();

	int(*vmemory_map_alloc)() = elf_get_function_module(&module_vmemory, "map_alloc");
	kmem_set_map_functions(vmemory_map_alloc, vmemory_get_mem_unit_size);

	uart_printf("Success!\r\n");
}
