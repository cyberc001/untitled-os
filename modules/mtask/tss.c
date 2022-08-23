#include "tss.h"
#include "mtask.h"

#include "cpu/x86/gdt.h"
#include "kernlib/kernmem.h"
#include "string.h"

int mtask_tss_init()
{
	for(uint8_t i = 0; i < core_num; ++i){
		uint64_t rsp[3] = {0, 0, (uintptr_t)kmalloc(MTASK_TSS_STACK_SIZE)};
		uint64_t ist[7] = {0};
		gdt_add_tss_desc(rsp, ist);
	}
}
