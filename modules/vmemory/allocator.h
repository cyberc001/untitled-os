#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stdint.h>

/* Initializes data structures needed for managing allocation.
*  Arguments:
*  	mem_limit - maximum amount of physical memory from init() function.
*/
void allocator_init(uint64_t mem_limit);

/* TODO: rename */
void* allocator_give(uint64_t size);

#endif
