#include <stddef.h>

const char* test_string = "test string";
static int x;
extern void uart_printf(const char* format, ...)
		__attribute__((format(printf, 1, 2)));

const char* test()
{
	uart_printf("Hello module world!\r\n");
	return test_string;
}
int* test2()
{
	x = 2812;
	return &x;
}
