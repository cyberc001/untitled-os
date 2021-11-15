// Header for BIOS I/O operations
// Currently features:
// - BIOS VGA character output (including barebones formatted output)

#ifndef BIOS_IO_H
#define BIOS_IO_H

#include <stddef.h>
#include <stdint.h>

#include "string.h"

enum bios_vga_clr {
	BIOS_VGA_COLOR_BLACK = 0,
	BIOS_VGA_COLOR_BLUE = 1,
	BIOS_VGA_COLOR_GREEN = 2,
	BIOS_VGA_COLOR_CYAN = 3,
	BIOS_VGA_COLOR_RED = 4,
	BIOS_VGA_COLOR_MAGENTA = 5,
	BIOS_VGA_COLOR_BROWN = 6,
	BIOS_VGA_COLOR_LIGHT_GREY = 7,
	BIOS_VGA_COLOR_DARK_GREY = 8,
	BIOS_VGA_COLOR_LIGHT_BLUE = 9,
	BIOS_VGA_COLOR_LIGHT_GREEN = 10,
	BIOS_VGA_COLOR_LIGHT_CYAN = 11,
	BIOS_VGA_COLOR_LIGHT_RED = 12,
	BIOS_VGA_COLOR_LIGHT_MAGENTA = 13,
	BIOS_VGA_COLOR_LIGHT_BROWN = 14,
	BIOS_VGA_COLOR_WHITE = 15
};

#define BIOS_VGA_ENTRY_COLOR(fg, bg) ( (fg) | (bg) << 4 )
#define BIOS_VGA_ENTRY(c, clr) ( ((uint16_t)(c)) | ((uint16_t)(clr)) << 8 )

#define BIOS_VGA_DEFAULT_COLOR (BIOS_VGA_ENTRY_COLOR(BIOS_VGA_COLOR_LIGHT_GREY, BIOS_VGA_COLOR_BLACK))

// management functions
void bios_vga_init();
void bios_vga_clear();

// raw input functions
void bios_vga_setchar(uint16_t ce, size_t x, size_t y);
void bios_vga_putchar(char c);

void bios_vga_write(const char* dat, size_t s);

// string functions similar to cstdlib ones
void bios_vga_puts(const char* str);
void bios_vga_printf(const char* format, ...)
	__attribute__((format(printf, 1, 2)));

#endif
