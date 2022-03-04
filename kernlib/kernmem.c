#include "kernmem.h"
#include "kerndefs.h"

#include "../cstdlib/string.h"

// Basic heap memory implementation.
// Does not provide any memory virtualization interfaces (yet).
// TODO: impelment memory virtualization and integrate kernel memory with it

// Basic linked list implementation
typedef struct kmem_node kmem_node;
struct kmem_node
{
	size_t sz;
	struct kmem_node* next;
	struct kmem_node* prev;
};
kmem_node* kmem_head = (kmem_node*)KERN_HEAP_BASE;

void kmem_init()
{
	// initializing the dummy node, which would store the 1st element of the list
	kmem_head->next = kmem_head->prev = NULL;
}

// TODO: add restraints on how far kernel memory can go
void* kmalloc(size_t size)
{
	kmem_node* it = kmem_head;
	while(it->next){
		// calculate the gap between current and next memory node
		void* gap_beg = (void*)it + sizeof(kmem_node) + it->sz;
		void* gap_end = it->next;
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

		it = it->next;
	}

	// if no large enough gap was found, mark space after the last (thus farthest in the memory) node allocated
	void* nblk = (void*)it + sizeof(kmem_node) + it->sz;
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
