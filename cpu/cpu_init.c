#include "cpu_init.h"

#include "x86/pic.h"
#include "x86/gdt.h"
#include "../dev/uart.h"

void cpu_init()
{
	#ifdef CPU_I386
	uart_printf("Remapping IRQs for PIC, to 0x20 and 0x28\r\n");
	pic_remap_irqs(0x20, 0x28);
	uart_printf("Initializing a flat GDT\r\n");
	gdt_init();
	#endif
}
