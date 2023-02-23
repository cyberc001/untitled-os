#include <stdint.h>
#include <stddef.h>
#include <stivale2.h>

#include "dev/uart.h"
#include "log/boot_log.h"

#include "kernlib/kernmem.h"
#include "string.h"

#include "cpu/pci.h"
#include "cpu/cpu_int.h"
#include "cpu/cpu_mode.h"

#include "cpu/cpu_init.h"

#include "fs/fs.h"
#include "bin/module.h"
#include "bin/elf.h"

#include "modules/vmemory/vmemory.h"

#include "modules/mtask/process.h"
#include "modules/mtask/thread.h"

#include "cpu/x86/pit.h"

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

// Framebuffer header tag (defining custom graphical framebuffer, instead of text mode)
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



struct mmap_entry{
	void* vaddr; void* paddr;
	uint64_t usize;
	int flags;
};

// First entry point
void _start(struct stivale2_struct* stivale2_struct)
{
	kernel_main(stivale2_struct);
	for(;;)
        	asm ("hlt");
}

void boot_log_write_stub(const char* str, size_t s){}

volatile int ap_test_counter = 0;
void ap_test()
{
	for(;;){
		uart_printf("Scheduling test - [%d]\r\n", ap_test_counter);
		--ap_test_counter;
		pit_sleep_ms(10);
	}
}
void ap_test2()
{
	for(;;){
		uart_printf("Scheduling test + [%d]\r\n", ap_test_counter);
		++ap_test_counter;
		pit_sleep_ms(10);
	}
}

void kernel_main(struct stivale2_struct* stivale2_struct)
{
	kmem_init();

	struct stivale2_struct_tag_terminal* term_str_tag;
	term_str_tag = stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_TERMINAL_ID);
	if(!term_str_tag){
		uart_printf("Couldn't find terminal tag, can't initialize boot logger\r\n");
		boot_log_set_write_func(boot_log_write_stub);
	}
	else
		boot_log_set_write_func((void*)term_str_tag->term_write);

	boot_log_printf_status(BOOT_LOG_STATUS_RUNNING, "Initializing PCI");
	pci_setup();
	boot_log_printf_status(BOOT_LOG_STATUS_SUCCESS, "Initializing PCI");

	boot_log_printf_status(BOOT_LOG_STATUS_RUNNING, "Generic CPU initialization");
	cpu_interrupt_set(0);
	int err = cpu_init();
	if(err)
		boot_log_printf_status(BOOT_LOG_STATUS_FAIL, "Generic CPU initialization: error code %d", err);
	else
		boot_log_printf_status(BOOT_LOG_STATUS_SUCCESS, "Generic CPU initialization");

	boot_log_printf_status(BOOT_LOG_STATUS_RUNNING, "Enterting protected mode");
	cpu_mode_set(CPU_MODE_PROTECTED);
	boot_log_printf_status(BOOT_LOG_STATUS_SUCCESS, "Enterting protected mode");
	boot_log_printf_status(BOOT_LOG_STATUS_RUNNING, "Initializing interrupts");
	cpu_interrupt_init();
	cpu_interrupt_set(1);
	boot_log_printf_status(BOOT_LOG_STATUS_SUCCESS, "Initializing interrupts");

	boot_log_printf_status(BOOT_LOG_STATUS_RUNNING, "Probing ATA decives");
	ata_drive drives[4];
	unsigned ata_devs = ata_probe(drives);
	boot_log_printf_status(BOOT_LOG_STATUS_SUCCESS, "ATA devices detected: %u", ata_devs);

	boot_log_printf_status(BOOT_LOG_STATUS_RUNNING, "Detecting file system on drive 0");
	file_system _fs;
	fs_scan(&_fs, &drives[0]);
	boot_log_printf_status(BOOT_LOG_STATUS_SUCCESS, "Detecting file system on drive 0: %s", _fs.name);

	module_init_api();

	boot_log_printf_status(BOOT_LOG_STATUS_RUNNING, "Loading memory virtualization module");
	module module_vmemory = {.name = "modload_memory"};
	void *modfd = kmalloc(_fs.fd_size), *descfd = kmalloc(_fs.fd_size);
	_fs.open(&_fs, modfd, "vmemory.so", FS_OPEN_READ);
	_fs.open(&_fs, descfd, "vmemory.dsc", FS_OPEN_READ);
	err = module_load_kernmem(&module_vmemory, &_fs, modfd, descfd);
	if(err)
		boot_log_printf_status(BOOT_LOG_STATUS_FAIL, "Loading memory virtualization module: error code %d", err);
	else
		boot_log_printf_status(BOOT_LOG_STATUS_SUCCESS, "Loading memory virtualization module");
	module_add_to_gmt(&module_vmemory);
	_fs.close(&_fs, modfd);
	_fs.close(&_fs, descfd);

	struct stivale2_struct_tag_memmap *mmap_struct_tag = stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_MEMMAP_ID);
	uint64_t memory_limit = 0;
	for(uint64_t i = 0; i < mmap_struct_tag->entries; ++i){
		if(mmap_struct_tag->memmap[i].type == 1 && mmap_struct_tag->memmap[i].base + mmap_struct_tag->memmap[i].length > memory_limit)
			memory_limit = mmap_struct_tag->memmap[i].base + mmap_struct_tag->memmap[i].length;
	}
	boot_log_printf_status(BOOT_LOG_STATUS_NLINE, "Memory limit: 0x%p", (void*)memory_limit);

	boot_log_printf_status(BOOT_LOG_STATUS_RUNNING, "Initializing memory virtualization module");
	int(*vmemory_init)() = elf_get_function_module(&module_vmemory, "vmemory_init");
	err = vmemory_init(memory_limit);
	if(err)
		boot_log_printf_status(BOOT_LOG_STATUS_FAIL, "Initializing memory virtualization module: error code %d", err);
	else
		boot_log_printf_status(BOOT_LOG_STATUS_SUCCESS, "Initializing memory virtualization module");
	uart_printf("VMEMORY base: %p\r\n", module_vmemory.elf_data);

	uint64_t(*vmemory_get_mem_unit_size)() = elf_get_function_module(&module_vmemory, "get_mem_unit_size");
	uint64_t vmemory_mem_unit_size = vmemory_get_mem_unit_size();

	boot_log_printf_status(BOOT_LOG_STATUS_RUNNING, "Creating memory context for kernel");
	uint64_t(*get_mem_hndl_size)() = elf_get_function_module(&module_vmemory, "get_mem_hndl_size");
	uint64_t mem_hndl_size = get_mem_hndl_size();
	void* kernel_mem_hndl = kmalloc(mem_hndl_size);
	int(*create_mem_hndl)() = elf_get_function_module(&module_vmemory, "create_mem_hndl");
	int(*select_mem_hndl)() = elf_get_function_module(&module_vmemory, "select_mem_hndl");
	err = create_mem_hndl(kernel_mem_hndl);
	if(err)
		boot_log_printf_status(BOOT_LOG_STATUS_FAIL, "Creating memory context for kernel: error code %d", err);
	else{
		err = select_mem_hndl(kernel_mem_hndl, 0);
		if(err)
			boot_log_printf_status(BOOT_LOG_STATUS_FAIL, "Creating memory context for kernel: error code %d", err);
		else
			boot_log_printf_status(BOOT_LOG_STATUS_SUCCESS, "Creating memory context for kernel");
	}

	boot_log_printf_status(BOOT_LOG_STATUS_RUNNING, "Identity mapping module loader");
	int(*vmemory_map_phys)() = elf_get_function_module(&module_vmemory, "map_phys");

	void* kmem_heap_end = kmem_get_heap_end();
	struct stivale2_struct_tag_kernel_base_address *kbase_tag = stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_KERNEL_BASE_ADDRESS_ID);

	#define KERN_IMG_SIZE (8 * 1024 * 1024)
	void* err_addr = NULL;
	struct mmap_entry ments[] = {
					// identity map memory heap
					{KMEM_HEAP_BASE, KMEM_HEAP_BASE, kmem_heap_end - KMEM_HEAP_BASE, VMEM_FLAG_SIZE_IN_BYTES},
					// identity map APIC base
					{(void*)0xfee00000, (void*)0xfee00000, 0x400/*APIC_REG_SIZE*/, VMEM_FLAG_SIZE_IN_BYTES},
					// identity map kernel image
					{(void*)kbase_tag->virtual_base_address, (void*)kbase_tag->physical_base_address, KERN_IMG_SIZE, VMEM_FLAG_SIZE_IN_BYTES}
				    };
	for(size_t i = 0; i < sizeof(ments) / sizeof(ments[0]); ++i){
		err = vmemory_map_phys(ments[i].vaddr, ments[i].paddr, ments[i].usize, ments[i].flags);
		if(err){ err_addr = ments[i].paddr; break; }
	}

	void* prev_addr = 0;
	uint64_t prev_length = 0;
	if(!err) for(uint64_t i = 0; i < mmap_struct_tag->entries; ++i){
		if(mmap_struct_tag->memmap[i].type == 0x1000 || mmap_struct_tag->memmap[i].type == 0x1001 || mmap_struct_tag->memmap[i].type == 0x1002){
			void* addr = (void*)mmap_struct_tag->memmap[i].base;
			uint64_t length = mmap_struct_tag->memmap[i].length + (uint64_t)addr % vmemory_mem_unit_size;
			addr = (void*)((uint64_t)addr - (uint64_t)addr % vmemory_mem_unit_size);
			// check for overlap with previous section
			if(prev_length && (prev_addr + (prev_length + ((prev_length + (vmemory_mem_unit_size - 1)) / vmemory_mem_unit_size) * vmemory_mem_unit_size) >= addr))
				continue;
			// check for overlap with kernel image
			if(addr + ((length + (vmemory_mem_unit_size - 1)) / vmemory_mem_unit_size) * vmemory_mem_unit_size >= (void*)kbase_tag->physical_base_address + KERN_IMG_SIZE
			&& addr <= (void*)kbase_tag->physical_base_address + KERN_IMG_SIZE)
				continue;

			err = vmemory_map_phys(addr, addr, length, VMEM_FLAG_SIZE_IN_BYTES);

			prev_addr = addr; prev_length = length;
			if(err) {err_addr = addr; break;}
		}
	}

	if(err){
		boot_log_printf_status(BOOT_LOG_STATUS_FAIL, "Identity mapping module loader: error code %d", err);
		boot_log_printf_status(BOOT_LOG_STATUS_NLINE, "On mapping address 0x%p\r\n", err_addr);
	}
	else
		boot_log_printf_status(BOOT_LOG_STATUS_SUCCESS, "Identity mapping module loader");

	boot_log_printf_status(BOOT_LOG_STATUS_RUNNING, "Selecting kernel memory context");
	err = select_mem_hndl(kernel_mem_hndl, 1);
	if(err)
		boot_log_printf_status(BOOT_LOG_STATUS_FAIL, "Selecting kernel memory context");
	else
		boot_log_printf_status(BOOT_LOG_STATUS_SUCCESS, "Selecting kernel memory context");

	int(*vmemory_map_alloc)() = elf_get_function_module(&module_vmemory, "map_alloc");
	kmem_set_map_functions(vmemory_map_alloc, vmemory_get_mem_unit_size);

	boot_log_printf_status(BOOT_LOG_STATUS_RUNNING, "Loading multitasking module");
	module module_mtask = {.name = "modload_mtask"};
	_fs.open(&_fs, modfd, "mtask.so", FS_OPEN_READ);
	_fs.open(&_fs, descfd, "mtask.dsc", FS_OPEN_READ);
	err = module_load_kernmem(&module_mtask, &_fs, modfd, descfd);
	if(err)
		boot_log_printf_status(BOOT_LOG_STATUS_FAIL, "Loading multitasking module: error code %d", err);
	else
		boot_log_printf_status(BOOT_LOG_STATUS_SUCCESS, "Loading multitasking module");
	module_add_to_gmt(&module_mtask);
	_fs.close(&_fs, modfd);
	_fs.close(&_fs, descfd);

	int(*mtask_init)() = elf_get_function_module(&module_mtask, "mtask_init");
	void(*mtask_create_process)() = elf_get_function_module(&module_mtask, "create_process");
	void(*mtask_load_context)() = elf_get_function_module(&module_mtask, "load_context");
	//void(*mtask_save_context)() = elf_get_function_module(&module_mtask, "save_context");
	thread*(*mtask_process_add_thread)(process*, thread*) = elf_get_function_module(&module_mtask, "process_add_thread");
	process*(*mtask_scheduler_add_process)(process*) = elf_get_function_module(&module_mtask, "scheduler_add_process");
	void(*mtask_scheduler_queue_thread)(thread*) = elf_get_function_module(&module_mtask, "scheduler_queue_thread");

	uart_printf("MTASK base: %p\r\n", module_mtask.elf_data);
	boot_log_printf_status(BOOT_LOG_STATUS_RUNNING, "Initializing multitasking module");
	boot_log_increase_nest_level();
	err = mtask_init();
	boot_log_decrease_nest_level();

	if(err)
		boot_log_printf_status(BOOT_LOG_STATUS_FAIL, "Initializing multitasking module: error code %d", err);
	else
		boot_log_printf_status(BOOT_LOG_STATUS_SUCCESS, "Initializing multitasking module");

	{ // schedule test code
		process pr; mtask_create_process(&pr);
		thread* th_pt = kmalloc_align(sizeof(thread), 16);
		MTASK_SAVE_CONTEXT(th_pt);
		th_pt->state.rip = (uintptr_t)ap_test;
		void* ap_test_stack = kmalloc(512);
		th_pt->state.rsp = (uintptr_t)ap_test_stack + 512;
		mtask_process_add_thread(&pr, th_pt);
		mtask_scheduler_add_process(&pr);
		mtask_scheduler_queue_thread(th_pt);
		kfree(th_pt);
	}
	{ // schedule test code
		process pr; mtask_create_process(&pr);
		thread* th_pt = kmalloc_align(sizeof(thread), 16);
		MTASK_SAVE_CONTEXT(th_pt);
		th_pt->state.rip = (uintptr_t)ap_test2;
		void* ap_test_stack = kmalloc(512);
		th_pt->state.rsp = (uintptr_t)ap_test_stack + 512;
		mtask_process_add_thread(&pr, th_pt);
		mtask_scheduler_add_process(&pr);
		mtask_scheduler_queue_thread(th_pt);
		kfree(th_pt);
	}
}
