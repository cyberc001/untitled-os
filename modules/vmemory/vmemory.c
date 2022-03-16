#include "vmemory.h"

#include "dev/uart.h"
#include "kernlib/kernmem.h"

#include "allocator.h"

int init(uint64_t mem_limit)
{
	allocator_init(mem_limit);
	return 0;
}

