#include "isr.h"

#include "../../dev/uart.h"

noreturn void isr_exception_stub_func(uint64_t isr_num)
{
	uart_printf("CPU exception #%ld\r\n", isr_num);
	asm volatile ("cli; hlt");
}
void isr_exception_stub_func_noerr(uint64_t isr_num)
{
	uart_printf("CPU exception (not halting) #%ld\r\n", isr_num);
}
