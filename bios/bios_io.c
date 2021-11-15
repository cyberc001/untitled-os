#include "bios_io.h"

#include <stdarg.h>

struct{
	size_t w, h; // (80, 25) by default
	struct {
		size_t x, y;
	} cursor;
	uint8_t cur_color;

	uint16_t* buff;
} main_scr;

// management functions
void bios_vga_init()
{
	main_scr.w = 80; main_scr.h = 25;
	main_scr.cursor.x = main_scr.cursor.y = 0;
	main_scr.cur_color = BIOS_VGA_DEFAULT_COLOR;
	main_scr.buff = (uint16_t*)0xB8000;

	bios_vga_clear();
}

void bios_vga_clear()
{
	main_scr.cur_color = BIOS_VGA_DEFAULT_COLOR;
	for(size_t i = 0; i < main_scr.w * main_scr.h; ++i)
		main_scr.buff[i] = BIOS_VGA_ENTRY(' ', main_scr.cur_color);
}


// raw input functions
static int bios_vga_is_char_printable(char c)
{ return c != '\n' && c != '\r' && c != '\0'; }

void bios_vga_setchar(uint16_t ce, size_t x, size_t y)
{
	main_scr.buff[y * main_scr.w + x] = ce;
}

void bios_vga_putchar(char c)
{
	if(bios_vga_is_char_printable(c))
		bios_vga_setchar(BIOS_VGA_ENTRY(c, main_scr.cur_color),
					main_scr.cursor.x++, main_scr.cursor.y);

	if(c == '\n' || main_scr.cursor.x == main_scr.w){
		main_scr.cursor.x = 0; main_scr.cursor.y++; // shifting cursor to the next line
		if(main_scr.cursor.y == main_scr.h){ // scrolling down
			for(size_t y = 1; y < main_scr.h; ++y)
				memcpy(main_scr.buff + ((y-1) * main_scr.w), main_scr.buff + (y * main_scr.w), main_scr.w * sizeof(uint16_t));
			for(size_t x = 0; x < main_scr.w; ++x)
				main_scr.buff[(main_scr.h-1) * main_scr.w + x] = BIOS_VGA_ENTRY(' ', main_scr.cur_color);
			main_scr.cursor.y--;
		}
	}
}

void bios_vga_write(const char* data, size_t size)
{
	for(size_t i = 0; i < size; i++)
		bios_vga_putchar(data[i]);
}


// string functions
void bios_vga_puts(const char* str)
{
	bios_vga_write(str, strlen(str));
}

// printf definitions zone
#define DEF_BIOS_VGA_PUT_NUM(tname, T)\
static void bios_vga_put ## tname (T num)\
{\
	if(num == 0){\
		bios_vga_putchar('0');\
		return;\
	}\
	if(num < 0){\
		bios_vga_putchar('-');\
		num *= -1;\
	}\
	int nums[24]; size_t nums_i = 0;\
	while(num > 0){\
		nums[nums_i++] = num % 10;\
		num /= 10;\
	}\
	nums_i--;\
	while(1){\
		bios_vga_putchar('0' + nums[nums_i]);\
		if(nums_i == 0) break;\
		nums_i--;\
	}\
}
// skips sign check so compiler would not complain
#define DEF_BIOS_VGA_PUT_UNUM(tname, T)\
static void bios_vga_put ## tname (T num)\
{\
	if(num == 0){\
		bios_vga_putchar('0');\
		return;\
	}\
	int nums[24]; size_t nums_i = 0;\
	while(num > 0){\
		nums[nums_i++] = num % 10;\
		num /= 10;\
	}\
	nums_i--;\
	while(1){\
		bios_vga_putchar('0' + nums[nums_i]);\
		if(nums_i == 0) break;\
		nums_i--;\
	}\
}
DEF_BIOS_VGA_PUT_NUM(i, int);
DEF_BIOS_VGA_PUT_UNUM(u, unsigned int);
DEF_BIOS_VGA_PUT_NUM(l, long int);
DEF_BIOS_VGA_PUT_UNUM(lu, long unsigned int);
DEF_BIOS_VGA_PUT_NUM(ll, long long int);
DEF_BIOS_VGA_PUT_UNUM(llu, long long unsigned int);

// TODO: account for overflow, writing like previous 2 macro
#define DEF_BIOS_VGA_PUT_NUM_HEXL(tname, T)\
static void bios_vga_put ## tname (T num)\
{\
	if(num == 0){\
		bios_vga_putchar('0');\
		return;\
	}\
	int nums[24]; size_t nums_i = 0;\
	while(num > 0){\
		nums[nums_i++] = num % 16;\
		num /= 16;\
	}\
	nums_i--;\
	while(1){\
		if(nums[nums_i] <= 9)\
			bios_vga_putchar('0' + nums[nums_i]);\
		else\
			bios_vga_putchar('a' + (nums[nums_i] - 10));\
		if(nums_i == 0) break;\
		nums_i--;\
	}\
}
#define DEF_BIOS_VGA_PUT_NUM_HEXU(tname, T)\
static void bios_vga_put ## tname (T num)\
{\
	if(num == 0){\
		bios_vga_putchar('0');\
		return;\
	}\
	int nums[24]; size_t nums_i = 0;\
	while(num > 0){\
		nums[nums_i++] = num % 16;\
		num /= 16;\
	}\
	nums_i--;\
	while(1){\
		if(nums[nums_i] <= 9)\
			bios_vga_putchar('0' + nums[nums_i]);\
		else\
			bios_vga_putchar('A' + (nums[nums_i] - 10));\
		if(nums_i == 0) break;\
		nums_i--;\
	}\
}
DEF_BIOS_VGA_PUT_NUM_HEXL(x, unsigned int);
DEF_BIOS_VGA_PUT_NUM_HEXU(X, unsigned int);


void bios_vga_printf(const char* format, ...)
{
	va_list args;
	va_start(args, format);

	for(; *format != '\0'; ++format)
	{
		if(*format == '%'){
			++format;
			switch(*format)
			{
				case 's':
					bios_vga_puts(va_arg(args, const char*));
					break;
				case 'c':
					bios_vga_putchar(va_arg(args, int));
					break;

				case 'd': case 'i':
					bios_vga_puti(va_arg(args, int));
					break;
				case 'u':
					bios_vga_putu(va_arg(args, unsigned int));
					break;

				case 'x':
					bios_vga_putx(va_arg(args, unsigned int));
					break;
				case 'X':
					bios_vga_putX(va_arg(args, unsigned int));
					break;

				case 'l':
					++format;
					switch(*format)
					{
						case 'd': case 'i':
							bios_vga_putl(va_arg(args, long int));
							break;
						case 'u':
							bios_vga_putlu(va_arg(args, long unsigned int));
							break;

						case 'l':
							++format;
							switch(*format)
							{
								case 'd': case 'i':
									bios_vga_putll(va_arg(args, long long int));
									break;
								case 'u':
									bios_vga_putllu(va_arg(args, unsigned long long int));
									break;
							}
							break;
					}
					break;
			}
		}
		else{
			bios_vga_putchar(*format);
		}
	}

	va_end(args);
}
