#include "boot_log.h"

#include <stdint.h>
#include <stdarg.h>
#include "string.h"

#include "dev/uart.h"

static void (*term_write)(const char*, size_t);

void boot_log_set_write_func(void (*write_func)(const char*, size_t))
{
	term_write = write_func;
}


// output functions

void boot_log_putchar(char c)
{
	boot_log_write(&c, 1);
}
void boot_log_write(const char* str, size_t s)
{
	term_write(str, s);
	uart_write(str, s);
}

void boot_log_puts(const char* str)
{
	boot_log_write(str, strlen(str));
}

// printf helper functions for each type
#define DEF_BOOT_LOG_PUT_NUM(tname, T)\
static void boot_log_put ## tname (T num)\
{\
        if(num == 0){\
                boot_log_putchar('0');\
                return;\
        }\
        if(num < 0){\
                boot_log_putchar('-');\
                num *= -1;\
        }\
        int nums[24]; size_t nums_i = 0;\
        while(num > 0){\
                nums[nums_i++] = num % 10;\
                num /= 10;\
        }\
        nums_i--;\
        while(1){\
                boot_log_putchar('0' + nums[nums_i]);\
                if(nums_i == 0) break;\
                nums_i--;\
        }\
}
#define DEF_BOOT_LOG_PUT_UNUM(tname, T)\
static void boot_log_put ## tname (T num)\
{\
        if(num == 0){\
                boot_log_putchar('0');\
                return;\
        }\
        int nums[24]; size_t nums_i = 0;\
        while(num > 0){\
                nums[nums_i++] = num % 10;\
                num /= 10;\
        }\
        nums_i--;\
        while(1){\
                boot_log_putchar('0' + nums[nums_i]);\
                if(nums_i == 0) break;\
                nums_i--;\
        }\
}
DEF_BOOT_LOG_PUT_NUM(i, int);
DEF_BOOT_LOG_PUT_UNUM(u, unsigned int);
DEF_BOOT_LOG_PUT_NUM(l, long int);
DEF_BOOT_LOG_PUT_UNUM(lu, long unsigned int);
DEF_BOOT_LOG_PUT_NUM(ll, long long int);
DEF_BOOT_LOG_PUT_UNUM(llu, long long unsigned int);

#define DEF_BOOT_LOG_PUT_NUM_HEXL(tname, T)\
static void boot_log_put ## tname (T num)\
{\
        if(num == 0){\
                boot_log_putchar('0');\
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
                        boot_log_putchar('0' + nums[nums_i]);\
                else\
                        boot_log_putchar('a' + (nums[nums_i] - 10));\
                if(nums_i == 0) break;\
                nums_i--;\
        }\
}
#define DEF_BOOT_LOG_PUT_NUM_HEXU(tname, T)\
static void boot_log_put ## tname (T num)\
{\
        if(num == 0){\
                boot_log_putchar('0');\
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
                        boot_log_putchar('0' + nums[nums_i]);\
                else\
                        boot_log_putchar('A' + (nums[nums_i] - 10));\
                if(nums_i == 0) break;\
                nums_i--;\
        }\
}
DEF_BOOT_LOG_PUT_NUM_HEXL(x, unsigned int);
DEF_BOOT_LOG_PUT_NUM_HEXU(X, unsigned int);
DEF_BOOT_LOG_PUT_NUM_HEXL(p, unsigned long);
DEF_BOOT_LOG_PUT_NUM_HEXU(P, unsigned long);

void boot_log_printf(const char* format, ...)
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
					boot_log_puts(va_arg(args, const char*));
					break;
				case 'c':
					boot_log_putchar(va_arg(args, int));
					break;

				case 'd': case 'i':
					boot_log_puti(va_arg(args, int));
					break;
				case 'u':
					boot_log_putu(va_arg(args, unsigned int));
					break;

				case 'x':
					boot_log_putx(va_arg(args, unsigned int));
					break;
				case 'X':
					boot_log_putX(va_arg(args, unsigned int));
					break;

				case 'p':
					boot_log_putp((unsigned long)va_arg(args, void*));
					break;
				case 'P':
					boot_log_putP((unsigned long)va_arg(args, void*));
					break;

				case 'l':
					++format;
					switch(*format)
					{
						case 'd': case 'i':
							boot_log_putl(va_arg(args, long int));
							break;
						case 'u':
							boot_log_putlu(va_arg(args, long unsigned int));
							break;

						case 'l':
							++format;
							switch(*format)
							{
								case 'd': case 'i':
									boot_log_putll(va_arg(args, long long int));
									break;
								case 'u':
									boot_log_putllu(va_arg(args, unsigned long long int));
									break;
							}
							break;
					}
					break;
			}
		}
		else
			boot_log_putchar(*format);
	}

	va_end(args);
}

// pretty progress output

static size_t nest_level = 0;

void boot_log_print_nest_padding()
{
	for(size_t i = 0; i < nest_level; ++i)
		boot_log_putchar('\t');
}
void boot_log_increase_nest_level()
{
	nest_level++;
	boot_log_putchar('\n');
}
void boot_log_decrease_nest_level()
{
	nest_level--;
}
