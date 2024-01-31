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
#include "cpu/x86/rsdp.h"
#include "cpu/x86/hpet.h"

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

#include "cpu/x86/apic.h"
#include "cpu/spinlock.h"
#define TEST_THREAD_COUNT	256
thread** threads;
void(*mtask_scheduler_dequeue_thread)(thread*);
volatile size_t tt_start[TEST_THREAD_COUNT];
volatile size_t tt_end[TEST_THREAD_COUNT];
void ap_test()
{
	register int eax asm("eax");
	int thread_i = eax;

	size_t hpet_timer_blocks_cnt;
	hpet_desc_table** hpet_timer_blocks = hpet_get_timer_blocks(&hpet_timer_blocks_cnt);
	void* timer_addr = (void*)hpet_timer_blocks[0]->base_addr.addr;
	tt_end[thread_i] = 0;

	int calc_smth = 0;
	for(size_t i = 0; i < 1000000000LL; ++i)
		calc_smth = ((calc_smth + 1312) >> 3) % 308959120 * 984859812;

	uint32_t lapic_id = lapic_read(LAPIC_REG_ID) >> 24;
	uart_printf("!!thread %d ended %lu %lu %u %p %d\r\n", thread_i, tt_start[thread_i], HPET_READ_REG(timer_addr, HPET_GENREG_COUNTER), lapic_id, threads[thread_i], calc_smth);
	tt_end[thread_i] = HPET_READ_REG(timer_addr, HPET_GENREG_COUNTER);

	if(thread_i == 0){
		for(;;){
			int cont = 0;
			for(size_t i = 0; i < TEST_THREAD_COUNT; ++i)
				if(!tt_end[i]) { cont = 1; break; }
			if(!cont)
				break;
		}

		size_t avg_tt = 0, avg_schlat = 0, avg_ts = 0;
		size_t min_tt = (size_t)-1, min_schlat = (size_t)-1, min_ts = (size_t)-1;
		size_t max_tt = 0, max_schlat = 0, max_ts = 0, prep_max_i = 0;
		for(size_t i = 0; i < TEST_THREAD_COUNT; ++i){
			size_t tt = tt_end[i] - tt_start[i], schlat = threads[i]->last_sched_latency;
			//uart_printf("[%lu] TT: %lu\tLatency: %lu\r\n", i, tt, schlat);
			avg_tt += tt; avg_schlat += schlat;
			if(tt < min_tt) min_tt = tt;
			if(schlat < min_schlat) min_schlat = schlat;
			if(tt > max_tt) max_tt = tt;
			if(schlat > max_schlat) max_schlat = schlat;

			if(threads[i]->task_switch_max > threads[prep_max_i]->task_switch_max) prep_max_i = i;
			//uart_printf("%lu %lu %lu\r\n", threads[i]->task_switch_avg, threads[i]->task_switch_min, threads[i]->task_switch_max);
		}
		for(size_t i = 0; i < TEST_THREAD_COUNT; ++i){
			if(i == prep_max_i) continue; // one process spends much more time on first 2 task switches, no idea why
			threads[i]->task_switch_avg /= threads[i]->task_switch_cnt;
			avg_ts += threads[i]->task_switch_avg;
			if(threads[i]->task_switch_min < min_ts) min_ts = threads[i]->task_switch_min;
			if(threads[i]->task_switch_max > max_ts) max_ts = threads[i]->task_switch_max;
		}
		avg_tt /= TEST_THREAD_COUNT; avg_schlat /= TEST_THREAD_COUNT; avg_ts /= TEST_THREAD_COUNT;
		uart_printf("%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\r\n", avg_tt, min_tt, max_tt, avg_schlat, min_schlat, max_schlat, avg_ts, min_ts, max_ts);
		/*uart_printf("[AVG] TT: %lu\tLatency: %lu\r\n", avg_tt, avg_schlat);
		uart_printf("[MIN] TT: %lu\tLatency: %lu\r\n", min_tt, min_schlat);
		uart_printf("[MAX] TT: %lu\tLatency: %lu\r\n", max_tt, max_schlat);*/

		for(;;){}
	}
	else{
		for(;;){}
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
					{(void*)0x3c000, (void*)0x3c000, 0xc3fff, VMEM_FLAG_SIZE_IN_BYTES},
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
	// TODO segregate this initialization into a separate cpu_init function that runs post identity mapping
	map_rsdp(vmemory_map_phys, vmemory_mem_unit_size);
	hpet_init(vmemory_map_phys, vmemory_mem_unit_size);

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
	void(*mtask_scheduler_queue_thread)(thread*) = elf_get_function_module(&module_mtask, "scheduler_queue_thread");
	mtask_scheduler_dequeue_thread = elf_get_function_module(&module_mtask, "scheduler_dequeue_thread");
	void(*mtask_scheduler_sleep_thread)(thread*, uint64_t) = elf_get_function_module(&module_mtask, "scheduler_sleep_thread");

	uart_printf("MTASK base: %p\r\n", module_mtask.elf_data);
	boot_log_printf_status(BOOT_LOG_STATUS_RUNNING, "Initializing multitasking module");
	boot_log_increase_nest_level();
	err = mtask_init();
	boot_log_decrease_nest_level();

	if(err)
		boot_log_printf_status(BOOT_LOG_STATUS_FAIL, "Initializing multitasking module: error code %d", err);
	else
		boot_log_printf_status(BOOT_LOG_STATUS_SUCCESS, "Initializing multitasking module");

	size_t hpet_timer_blocks_cnt;
	hpet_desc_table** hpet_timer_blocks = hpet_get_timer_blocks(&hpet_timer_blocks_cnt);
	void* timer_addr = (void*)hpet_timer_blocks[0]->base_addr.addr;
	size_t timer_res_ns = HPET_COUNTER_CLK_PERIOD(HPET_READ_REG(timer_addr, HPET_GENREG_CAP_ID));
	timer_res_ns /= 1000000;

	threads = kmalloc(sizeof(void*) * TEST_THREAD_COUNT);
	for(size_t i = 0; i < TEST_THREAD_COUNT; ++i)
	{ // schedule test code
		
		process pr; mtask_create_process(&pr);
		thread* th_pt = kmalloc_align(sizeof(thread), 16);
		memset(th_pt, 0, sizeof(thread));
		threads[i] = th_pt;
		MTASK_SAVE_CONTEXT(th_pt);
		th_pt->state.rip = (uintptr_t)(ap_test);
		th_pt->state.rax = i;
		void* ap_test_stack = kmalloc(1024);
		th_pt->state.rsp = th_pt->state.rbp = (uintptr_t)ap_test_stack + 1024;
		th_pt->weight = 1024;//1024 + 128 * i;
		//mtask_process_add_thread(&pr, th_pt);
		mtask_scheduler_queue_thread(th_pt);
		//kfree(th_pt);
	}
	for(size_t i = 0; i < TEST_THREAD_COUNT; ++i){
		tt_start[i] = HPET_READ_REG(timer_addr, HPET_GENREG_COUNTER) + (500000000 + 10000000*i) / timer_res_ns;
		mtask_scheduler_sleep_thread(threads[i], 500000000 + 10000000*i);
	}

	void(*toggle_sts)(int) = elf_get_function_module(&module_mtask, "toggle_sts");
	toggle_sts(1);
}
