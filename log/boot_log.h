#ifndef BOOT_LOG
#define BOOT_LOG

#include <stddef.h>

#define TERM_NORMAL 		"\x1b[0m"

#define TERM_FGCLR_RED 		"\x1b[31m"
#define TERM_FGCLR_GREEN 	"\x1b[32m"
#define TERM_FGCLR_YELLOW 	"\x1b[33m"
#define TERM_FGCLR_BLUE		"\x1b[34m"
#define TERM_FGCLR_MAGENTA	"\x1b[35m"
#define TERM_FGCLR_CYAN 	"\x1b[36m"

#define TERM_BGCLR_RED 		"\x1b[41m"
#define TERM_BGCLR_GREEN 	"\x1b[42m"
#define TERM_BGCLR_YELLOW 	"\x1b[43m"
#define TERM_BGCLR_BLUE		"\x1b[44m"
#define TERM_BGCLR_MAGENTA	"\x1b[45m"
#define TERM_BGCLR_CYAN 	"\x1b[46m"

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
#define BOOT_LOG_STATUS_NLINE		3

#define boot_log_printf_status(status, format, ...)\
	boot_log_print_nest_padding(),\
	boot_log_printf("%s " format "%s",\
			  (status) == BOOT_LOG_STATUS_RUNNING ? "[" TERM_FGCLR_YELLOW "...." TERM_NORMAL "]"\
			: (status) == BOOT_LOG_STATUS_SUCCESS ? "[ "TERM_FGCLR_GREEN "OK" TERM_NORMAL " ]"\
			: (status) == BOOT_LOG_STATUS_FAIL    ? "[" TERM_FGCLR_RED "FAIL" TERM_NORMAL "]"\
			: (status) == BOOT_LOG_STATUS_NLINE	  ? "      "\
			: "[    ]",\
			##__VA_ARGS__,\
			(((status) == BOOT_LOG_STATUS_SUCCESS) | ((status) == BOOT_LOG_STATUS_FAIL) | ((status) == BOOT_LOG_STATUS_NLINE)) ? "\r\n" : "\r")

void boot_log_print_nest_padding();
void boot_log_increase_nest_level();
void boot_log_decrease_nest_level();

#endif
