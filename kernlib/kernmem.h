// Header for kernel memory operations

#ifndef KERNMEM_H
#define KERNMEM_H

#include <stddef.h>

void kmem_init();
void* kmem_get_heap_end();
void print_kmem_llist();

void* kmalloc(size_t size);
void* kmalloc_align(size_t size, size_t align);
void* krealloc(void* ptr, size_t size);
void kfree(void* ptr);

#endif
