#ifndef MULTIBOOT_MBT_H
#define MULTIBOOT_MBT_H

#include <stdint.h>

// GRUB's multiboot header
typedef struct{
	uint32_t magic;
	uint32_t flags;
	uint32_t checksum;

	uint32_t header_addr;
	uint32_t load_addr;
	uint32_t load_end_addr;
	uint32_t bss_end_addr;
	uint32_t entry_addr;
	uint32_t mode_type;
	uint32_t width;
	uint32_t height;
	uint32_t depth;
} multiboot_mbt;

// GRUB's multiboot information, held in EAX at startup
typedef struct{
	uint32_t flags;

	uint64_t mem;
	uint32_t boot_device;
	uint32_t cmdline;

	uint32_t mods_count;
	uint32_t mods_addr;

	char syms[12];

	uint32_t mmap_length;
	uint32_t mmap_addr;

	uint32_t drives_length;
	uint32_t drives_addr;

	uint32_t config_table;
	uint32_t boot_loader_name;
	uint32_t apm_table;

	uint32_t vbe_control_info;
	uint32_t vbe_mode_info;
	uint32_t vbe_mode;
	uint32_t vbe_interface_seg;
	uint32_t vbe_interface_offset;
	uint32_t vbe_interface_length;

	uint32_t framebuf_addr;
	uint32_t framebuff_pitch;
	uint32_t framebuff_width;
	uint32_t framebuff_height;
	uint32_t framebuff_bpp;
	uint32_t framebuff_type;
	char color_info[5];
} multiboot_info;

extern multiboot_info* main_multiboot_info; // left to be set by the kernel

// GRUB's memory map structure to detect available physical memory
typedef struct{
	uint32_t size;
	uint64_t base;
	uint64_t length;
	uint32_t type;
} multiboot_mmap_entry;

// 'mbi' is of type 'multiboot_info'
// 'entry' is of type 'multiboot_mbt_entry * '
#define MULTIBOOT_MMAP_ENTRY_FIRST(mbi) ((multiboot_mmap_entry*)(mbi).mmap_addr)
#define MULTIBOOT_MMAP_ENTRY_IS_VALID(mbi, entry) ( (void*)(entry) < (void*)(mbi).mmap_addr + (mbi).mmap_length)
#define MULTIBOOT_MMAP_ENTRY_NEXT(entry) { (entry) = (void*)(entry) + (entry)->size + sizeof((entry)->size); }

#endif
