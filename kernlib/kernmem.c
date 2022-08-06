#include "kernmem.h"
#include "kerndefs.h"

#include <stdint.h>

#include "dev/uart.h"
#include "cstdlib/string.h"

#include "modules/vmemory/vmemory.h"

// Basic kernel heap memory implementation.

int (*vmemory_map_alloc)(void*, uint64_t, int) = NULL;
uint64_t (*vmemory_get_mem_unit_size)() = NULL;
void kmem_set_map_functions(int (*alloc_func)(void*, uint64_t, int), uint64_t (*mem_unit_size_func)())
{
	vmemory_map_alloc = alloc_func;
	vmemory_get_mem_unit_size = mem_unit_size_func;
}

// Basic linked list implementation
typedef struct kmem_node kmem_node;
struct kmem_node
{
	size_t sz;
	struct kmem_node* next;
	struct kmem_node* prev;
};
kmem_node* kmem_head = (kmem_node*)KERN_HEAP_BASE;
void* occupied_to = KERN_HEAP_BASE; // upper bound of memory currently mapped

void kmem_init()
{
	// initializing the dummy node, which would store the 1st element of the list
	kmem_head->next = kmem_head->prev = NULL;
}
void* kmem_get_heap_end()
{
	return occupied_to;
}

void print_kmem_llist()
{
	kmem_node* it = kmem_head;
	while(it){
		uart_printf("size: %p\tnext: %p\tprev: %p\t\r\n", (void*)it->sz, it->next, it->prev);
		it = it->next;
	}
}


void* kmalloc(size_t size)
{
	return kmalloc_align(size, 1);
}
void* kmalloc_align(size_t size, size_t align)
{
	kmem_node* it = kmem_head;
	while(it->next){
		// calculate the gap between current and next memory node
		void* gap_beg = (void*)it + sizeof(kmem_node) + it->sz;
		void* gap_end = it->next;
		if(((uint64_t)gap_beg + sizeof(kmem_node)) % align) // account for alignment
			gap_beg += align - ((uint64_t)gap_beg + sizeof(kmem_node)) % align;
		if(gap_end > gap_beg){
			size_t gap = gap_end - gap_beg;
			if(gap >= size + sizeof(kmem_node)){
				kmem_node* it_next = it->next;
				it->next = gap_beg;
				it_next->prev = it->next;

				it->next->sz = size;
				it->next->next = it_next;
				it->next->prev = it;
				return it->next + 1;
			}
		}

		it = it->next;
	}

	// if no large enough gap was found, mark space after the last (thus farthest in the memory) node allocated
	void* nblk = (void*)it + sizeof(kmem_node) + it->sz;
	if(((uint64_t)nblk + sizeof(kmem_node)) % align)
		nblk += align - ((uint64_t)nblk + sizeof(kmem_node)) % align;
	if(nblk + sizeof(kmem_node) + size > occupied_to){
		if(vmemory_map_alloc){
			uint64_t mem_unit_size = vmemory_get_mem_unit_size();
			void* occupied_to_aligned = occupied_to - (uint64_t)occupied_to % mem_unit_size;
			if((uint64_t)((nblk + sizeof(kmem_node) + size) - occupied_to_aligned) > mem_unit_size)
				vmemory_map_alloc(occupied_to_aligned + mem_unit_size,
									(uint64_t)(nblk + sizeof(kmem_node) + size) - (uint64_t)occupied_to_aligned, VMEM_FLAG_SIZE_IN_BYTES);
		}
		occupied_to = nblk + sizeof(kmem_node) + size;
	}
	it->next = nblk;
	it->next->sz = size;
	it->next->next = NULL;
	it->next->prev = it;
	return it->next + 1;
}

void* krealloc(void* ptr, size_t size)
{
	if(!ptr)
		return kmalloc(size);

	kmem_node* kn = ptr; kn--;

	if(kn->next){ // if it's not the last element on the list, try to resize in the gap first
		void* gap_beg = ptr;
		void* gap_end = kn->next;
		size_t gap = gap_end - gap_beg;

		if(gap >= size + sizeof(kmem_node)){
			kn->sz = size;
			return kn + 1;
		}
	}
	// otherwise try to allocate space somewhere else
	void* nptr = kmalloc(size);
	if(!nptr)
		return NULL;
	memcpy(nptr, ptr, kn->sz);
	kfree(ptr);
	return nptr;
}

void kfree(void* ptr)
{
	// just unlink the node with previous and next ones, if they exist
	kmem_node* it = (kmem_node*)ptr - 1;
	if(it->prev)
		it->prev->next = it->next;
	if(it->next)
		it->next->prev = it->prev;
}
