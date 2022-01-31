#include "uart.h"
#include "../cpu/cpu_io.h"

#include <stdint.h>
#include <stdarg.h>

#define UART_PORT	0x3F8

void uart_putchar(char c)
{
	cpu_out8(UART_PORT, c);
}
void uart_write(const char* dat, size_t s)
{
	for(size_t i = 0; i < s; ++i)
		cpu_out8(UART_PORT, dat[i]);
}

void uart_puts(const char* str)
{
	for(; *str; ++str)
		cpu_out8(UART_PORT, *str);
}

// printf helper functions for each type
#define DEF_UART_PUT_NUM(tname, T)\
static void uart_put ## tname (T num)\
{\
        if(num == 0){\
                uart_putchar('0');\
                return;\
        }\
        if(num < 0){\
                uart_putchar('-');\
                num *= -1;\
        }\
        int nums[24]; size_t nums_i = 0;\
        while(num > 0){\
                nums[nums_i++] = num % 10;\
                num /= 10;\
        }\
        nums_i--;\
        while(1){\
                uart_putchar('0' + nums[nums_i]);\
                if(nums_i == 0) break;\
                nums_i--;\
        }\
}
#define DEF_UART_PUT_UNUM(tname, T)\
static void uart_put ## tname (T num)\
{\
        if(num == 0){\
                uart_putchar('0');\
                return;\
        }\
        int nums[24]; size_t nums_i = 0;\
        while(num > 0){\
                nums[nums_i++] = num % 10;\
                num /= 10;\
        }\
        nums_i--;\
        while(1){\
                uart_putchar('0' + nums[nums_i]);\
                if(nums_i == 0) break;\
                nums_i--;\
        }\
}
DEF_UART_PUT_NUM(i, int);
DEF_UART_PUT_UNUM(u, unsigned int);
DEF_UART_PUT_NUM(l, long int);
DEF_UART_PUT_UNUM(lu, long unsigned int);
DEF_UART_PUT_NUM(ll, long long int);
DEF_UART_PUT_UNUM(llu, long long unsigned int);

#define DEF_UART_PUT_NUM_HEXL(tname, T)\
static void uart_put ## tname (T num)\
{\
        if(num == 0){\
                uart_putchar('0');\
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
                        uart_putchar('0' + nums[nums_i]);\
                else\
                        uart_putchar('a' + (nums[nums_i] - 10));\
                if(nums_i == 0) break;\
                nums_i--;\
        }\
}
#define DEF_UART_PUT_NUM_HEXU(tname, T)\
static void uart_put ## tname (T num)\
{\
        if(num == 0){\
                uart_putchar('0');\
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
                        uart_putchar('0' + nums[nums_i]);\
                else\
                        uart_putchar('A' + (nums[nums_i] - 10));\
                if(nums_i == 0) break;\
                nums_i--;\
        }\
}
DEF_UART_PUT_NUM_HEXL(x, unsigned int);
DEF_UART_PUT_NUM_HEXU(X, unsigned int);
DEF_UART_PUT_NUM_HEXL(p, unsigned int);
DEF_UART_PUT_NUM_HEXU(P, unsigned int);

void uart_printf(const char* format, ...)
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
					uart_puts(va_arg(args, const char*));
					break;
				case 'c':
					uart_putchar(va_arg(args, int));
					break;

				case 'd': case 'i':
					uart_puti(va_arg(args, int));
					break;
				case 'u':
					uart_putu(va_arg(args, unsigned int));
					break;

				case 'x':
					uart_putx(va_arg(args, unsigned int));
					break;
				case 'X':
					uart_putX(va_arg(args, unsigned int));
					break;

				case 'p':
					uart_putp((unsigned int)va_arg(args, void*));
					break;
				case 'P':
					uart_putP((unsigned int)va_arg(args, void*));
					break;

				case 'l':
					++format;
					switch(*format)
					{
						case 'd': case 'i':
							uart_putl(va_arg(args, long int));
							break;
						case 'u':
							uart_putlu(va_arg(args, long unsigned int));
							break;

						case 'l':
							++format;
							switch(*format)
							{
								case 'd': case 'i':
									uart_putll(va_arg(args, long long int));
									break;
								case 'u':
									uart_putllu(va_arg(args, unsigned long long int));
									break;
							}
							break;
					}
					break;
			}
		}
		else
			uart_putchar(*format);
	}

	va_end(args);
}
