#include "module.h"

#include "elf.h"

#include "../kernlib/kernmem.h"
#include "dev/uart.h"

#include "string.h"

module_table gmt = {NULL, 0};


static module_desc mdesc_parse(file_system* fs, void* desc_fd);

int module_load_kernmem(module* md, file_system* fs, void* bin_fd, void* desc_fd)
{
	elf_header ehd;
	int err = elf_read_header(fs, bin_fd, &ehd);
	if(err) return err;

	md->elf_data_size = fs->get_size(fs, bin_fd);
	md->elf_data = kmalloc(md->elf_data_size);
	fs->seek(fs, bin_fd, 0, FS_SEEK_BEGIN);
	fs->read(fs, bin_fd, md->elf_data, md->elf_data_size);

	err = elf_init_addresses(&md->elf_data, &md->elf_data_size);
	if(err) return err;
	err = elf_init_nobits(&md->elf_data, &md->elf_data_size);
	if(err) return err;

	md->desc = mdesc_parse(fs, desc_fd);
	err = elf_init_relocate(md->elf_data, (const char**)md->desc.dependencies);
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


// Module description parsing

static const char* mdesc_wspaces = " ,\t\r\n";

static char* mdesc_get_lexem(file_system* fs, void* desc_fd)
{
	char c;
	size_t rd = 0;
	while( (rd = fs->read(fs, desc_fd, &c, 1)) && strchr(mdesc_wspaces, c))
		; // skip preceeding whitespaces
	if(!rd) return NULL;
	char buf[256];
	buf[0] = c == '\"' ? mdesc_wspaces[0] : c;
	char* buf_it = buf + (c != '\"');
	char* buf_end = buf + sizeof(buf) - 1;

	int is_quote = (c == '\"');
	while(buf_it != buf_end && (rd = fs->read(fs, desc_fd, buf_it, 1))
		&& ( is_quote || !strchr(mdesc_wspaces, *buf_it) ) )
	{
		if(*buf_it == '\"')
		{ is_quote = !is_quote; *buf_it = mdesc_wspaces[0]; }
		else ++buf_it;
	}
	if(!rd) return NULL;

	buf_it[strchr(mdesc_wspaces, *buf_it) ? 0 : 1] = '\0';
	char* ret = kmalloc(strlen(buf) + 1);
	strcpy(ret, buf);
	return ret;
}

static char** mdesc_parse_list(file_system* fs, void* desc_fd)
{
	size_t list_ln = 0;
	char** list = NULL;

	while(1){
		char* lx = mdesc_get_lexem(fs, desc_fd);
		if(!lx) break;

		if(!strcmp(lx, "}"))
		{ kfree(lx); break; }

		char** prev_list = list;
		list = krealloc(list, (++list_ln) * sizeof(char*));
		if(!list){
			for(size_t i = 0; i < list_ln - 1; ++i)
				kfree(prev_list[i]);
			kfree(prev_list);
			return NULL;
		}
		list[list_ln - 1] = lx;
	}
	char** prev_list = list;
	list = krealloc(list, (++list_ln) * sizeof(char*));
	if(!list){
		for(size_t i = 0; i < list_ln - 1; ++i)
			kfree(prev_list[i]);
		kfree(prev_list);
		return NULL;
	}
	list[list_ln - 1] = NULL;

	return list;
}


static module_desc mdesc_parse(file_system* fs, void* desc_fd)
{
	module_desc ret = {.dependencies = NULL};

	char* prop_name = NULL;
	while(1){
		char* lx = mdesc_get_lexem(fs, desc_fd);
		if(!lx) break;

		if(!prop_name)
			prop_name = lx;
		else{
			if(!strcmp(prop_name, "dependencies") && !strcmp(lx, "{")){
				ret.dependencies = mdesc_parse_list(fs, desc_fd);
			}
			/*else if(!strcmp(lx, "=")){

			}*/

			kfree(lx);
			kfree(prop_name);
			prop_name = NULL;
		}
	}
	if(prop_name)
		kfree(prop_name);

	return ret;
}


// Module API symbol table

module_loader_api gmapi = {NULL, NULL, 0};

#include "cpu/cpu_int.h"
#include "cpu/x86/gdt.h"
#include "cpu/x86/pic.h"
#include "cpu/x86/cpuid.h"
#include "cpu/x86/apic.h"
#include "cpu/x86/pit.h"
#include "cpu/x86/rsdp.h"
#include "cpu/x86/hpet.h"
#include "dev/pio.h"
#include "log/boot_log.h"
void module_init_api()
{
	#define GMAPI_ENTRY(sym) { size_t i = __COUNTER__; gmapi.symbols[i] = (uint64_t)(sym); gmapi.names[i] = #sym; }
	gmapi.length = 61;
	gmapi.symbols = kmalloc(sizeof(uint64_t) * gmapi.length);
	gmapi.names = kmalloc(sizeof(const char*) * gmapi.length);

	// /dev
		// uart.h
		GMAPI_ENTRY(uart_putchar)
		GMAPI_ENTRY(uart_write)
		GMAPI_ENTRY(uart_puts)
		GMAPI_ENTRY(uart_printf)
		// ata.h
		GMAPI_ENTRY(ata_probe)
		GMAPI_ENTRY(ata_wait)
		GMAPI_ENTRY(ata_poll)
		GMAPI_ENTRY(ata_soft_reset)
		GMAPI_ENTRY(ata_select_drive)
		// pio.h
		GMAPI_ENTRY(pio_setup_drive)
		GMAPI_ENTRY(pio_read)
		GMAPI_ENTRY(pio_write)
	// /cpu
		// cpu_int.h
		GMAPI_ENTRY(cpu_interrupt_set_gate)
		// pci.h
		GMAPI_ENTRY(pci_set_ioaddr)
		GMAPI_ENTRY(pci_scan_for_device)
		GMAPI_ENTRY(pci_read_bar)
		// /x86
			// gdt.h
			GMAPI_ENTRY(gdt_add_desc)
			GMAPI_ENTRY(gdt_add_tss_desc)
			GMAPI_ENTRY(gdt_reload)
			GMAPI_ENTRY(gdt_init)
			// pic.h
			GMAPI_ENTRY(pic_send_eoi)
			GMAPI_ENTRY(pic_remap_irqs)
			GMAPI_ENTRY(pic_set_mask)
			GMAPI_ENTRY(pic_clear_mask)
			GMAPI_ENTRY(pic_get_irr)
			GMAPI_ENTRY(pic_get_isr)
			// cpuid.h
			GMAPI_ENTRY(cpuid_check)
			GMAPI_ENTRY(cpuid)
			// apic.h
			GMAPI_ENTRY(apic_check)
			GMAPI_ENTRY(lapic_read)
			GMAPI_ENTRY(lapic_write)
			GMAPI_ENTRY(apic_enable_spurious_ints)
			GMAPI_ENTRY(apic_set_timer)
			// pit.h
			GMAPI_ENTRY(pit_sleep_ms)
			// rsdp.h
			GMAPI_ENTRY(find_rsdp)
			// hpet.h
			GMAPI_ENTRY(hpet_get_timer_blocks)
	// /cstdlib
		// string.h
		GMAPI_ENTRY(memcpy)
		GMAPI_ENTRY(memmove)
		GMAPI_ENTRY(memset)
		GMAPI_ENTRY(memcmp)
		GMAPI_ENTRY(strlen)
		GMAPI_ENTRY(strcpy)
		GMAPI_ENTRY(strncpy)
		GMAPI_ENTRY(strcat)
		GMAPI_ENTRY(strncat)
		GMAPI_ENTRY(strcmp)
		GMAPI_ENTRY(strncmp)
		GMAPI_ENTRY(strchr)
	// /kernlib
		// kernmem.h
		GMAPI_ENTRY(kmalloc)
		GMAPI_ENTRY(kmalloc_align)
		GMAPI_ENTRY(krealloc)
		GMAPI_ENTRY(krealloc_align)
		GMAPI_ENTRY(kfree)
		GMAPI_ENTRY(print_kmem_llist)
	// /log
		// boot_log.h
		GMAPI_ENTRY(boot_log_putchar)
		GMAPI_ENTRY(boot_log_write)
		GMAPI_ENTRY(boot_log_puts)
		GMAPI_ENTRY(boot_log_printf)
		GMAPI_ENTRY(boot_log_print_nest_padding)
		GMAPI_ENTRY(boot_log_increase_nest_level)
		GMAPI_ENTRY(boot_log_decrease_nest_level)

	#undef GMAPI_ENTRY
}
