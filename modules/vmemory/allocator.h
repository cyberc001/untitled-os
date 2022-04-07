#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stdint.h>

/* Initializes data structures needed for managing allocation.
*  Arguments:
*  	mem_limit - maximum amount of physical memory from init() function.
*/
void allocator_init(uint64_t mem_limit);

/* Looks for free space size bytes long or more, then marks it as occupied and returns an address.
*  Arguments:
*  	size - size of requested continous space.
*  Return value:
*	a valid pointer or (void*)-1 if there wasn't any free space of such size.
*/
void* allocator_alloc(uint64_t size);

/* Simlar to allocate(), but tries to mark a certain address as occupied.
*  Arguments:
*	size - size of requested continous space.
*	addr - address to be marked as occupied.
*  Return value:
*	addr or (void*)-1 if given address was already occupied (may be partially) / doesn't physically exist
*/
void* allocator_alloc_addr(uint64_t size, void* addr);

/* Marks requested address as free, performing necessary node insertions and merges.
*  Arguments:
*	addr - address to be marked as free.
*	size - size of memory region to be marked as free.
*/
void allocator_free(void* addr, uint64_t size);

#endif
