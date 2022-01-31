#ifndef UART_H
#define UART_H

#include <stddef.h>

// raw input functions
void uart_putchar(char c);
void uart_write(const char* dat, size_t s);

// string functions
void uart_puts(const char* str);
void uart_printf(const char* format, ...)
	__attribute__((format(printf, 1, 2)));

#endif
