#include "vmemory.h"

#include "dev/uart.h"
#include "kernlib/kernmem.h"

#include "allocator.h"

int init(uint64_t mem_limit)
{
	allocator_init(mem_limit);
	void* addr = allocator_alloc_addr(4096, (void*)0x16000);
	uart_printf("mapped: %p\r\n", addr);
	allocator_free(addr, 4096);
	uart_printf("freed\r\n");

	return 0;
}

