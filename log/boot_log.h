#ifndef BOOT_LOG
#define BOOT_LOG

#include <stddef.h>

void boot_log_set_write_func(void (*write_func)(const char*, size_t));

// raw input output
void boot_log_putchar(char c);
void boot_log_write(const char* str, size_t s);

// string output
void boot_log_puts(const char* str);
void boot_log_printf(const char* format, ...)
	__attribute__((format(printf, 1, 2)));

// pretty progress output
#define BOOT_LOG_STATUS_RUNNING		0
#define BOOT_LOG_STATUS_SUCCESS		1
#define BOOT_LOG_STATUS_FAIL		2

#define boot_log_printf_status(status, format, ...)\
	boot_log_printf("%s " format "%s",\
			  (status) == BOOT_LOG_STATUS_RUNNING ? "[....]"\
			: (status) == BOOT_LOG_STATUS_SUCCESS ? "[ OK ]"\
			: (status) == BOOT_LOG_STATUS_FAIL    ? "[FAIL]"\
			: "[    ]",\
			##__VA_ARGS__,\
			(((status) == BOOT_LOG_STATUS_SUCCESS) | ((status) == BOOT_LOG_STATUS_FAIL)) ? "\r\n" : "\r")

#endif
