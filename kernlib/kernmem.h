// Header for kernel memory operations

#ifndef KERNMEM_H
#define KERNMEM_H

#include <stddef.h>
#include <stdint.h>

#define KMEM_HEAP_BASE ((void*)0x200000)

void kmem_init();
void kmem_set_map_functions(int (*alloc_func)(void*, uint64_t, int),
				uint64_t (*mem_unit_size_func)());

void* kmem_get_heap_end();
void print_kmem_llist();

void* kmalloc(size_t size);
void* kmalloc_align(size_t size, size_t align);
void* krealloc(void* ptr, size_t size);
void* krealloc_align(void* ptr, size_t size, size_t align);
void kfree(void* ptr);

#endif
