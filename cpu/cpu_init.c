#include "cpu_init.h"

#include "x86/pic.h"
#include "x86/gdt.h"

void cpu_init()
{
	#ifdef CPU_I386
	pic_remap_irqs(0x20, 0x28);
	gdt_init();
	#endif
}
