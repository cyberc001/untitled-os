// Header for kernel memory operations

#ifndef KERNMEM_H
#define KERNMEM_H

#include <stddef.h>

void kmem_init();

void* kmalloc(size_t size);
void* krealloc(void* ptr, size_t size);
void kfree(void* ptr);

#endif
