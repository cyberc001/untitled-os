#include "vmemory.h"

#include "dev/uart.h"
#include "kernlib/kernmem.h"

#include "allocator.h"

#define PFLAG_PRESENT			(1 << 0)
#define PFLAG_CANWRITE			(1 << 1)	// if 0, writes are not allowed
#define PFLAG_SUPERVISOR		(1 << 2)	// if 0, user-mode access is not allowed
#define PFLAG_WRITETHROUGH		(1 << 3)
#define PFLAG_CACHEDISABLE		(1 << 4)	// if 1, disables caching this page
#define PFLAG_ACCESSED			(1 << 5)
#define PFLAG_DIRTY				(1 << 6)
#define PFLAG_PSIZE				(1 << 7)	// if 1 at:
											// Page Directory Pointer Table Entry (PDPTE): maps a 1 GB page
											// Page Directory Entry (PDE): maps a 2 MB page
											// if 0, pages have size of 4 KB
#define PFLAG_GLOBAL			(1 << 8)	// if 1, translation is global (not invalidated in TLB)
#define PFLAG_PAT				(1 << 12)
#define PFLAG_XD				(1 << 63)	// if 1, does not allow instruction fetches from this page (if CPU supports it)


#define GET_BITS_LAST(expr, amt)		((uint64_t)(expr) & ((1 << (amt)) - 1))
#define GET_BITS(expr, start, end)		GET_BITS_LAST((uint64_t)(expr) >> (start), (end) - (start))

#define SET_BITS(expr, bits, start)		{ (expr) = (__typeof__(expr))((uint64_t)(expr) | (bits) << (start)); }

// PML4:
uint64_t* pml4;
#define PML4_ENTRIES				512
#define PML4_ALIGN					4096
#define GET_PML4E(addr)				(uint64_t*)((uint64_t)cur_hndl->pml4 | (GET_BITS(addr, 39, 48) << 3))
#define SET_PDPT(pml4e, paddr)		SET_BITS(pml4e, paddr, 0) // 51:12

// PDPT:
#define PDPT_ENTRIES				512
#define PDPT_ALIGN					4096
#define GET_PDPTE(addr, pml4e)		((uint64_t*)( ((pml4e) & 0xFFFFFFFFFF000) | (GET_BITS(addr, 30, 39) << 3)))
#define SET_PD(pdpte, paddr)		SET_BITS(pdpte, paddr, 0) // 51:12

// PD:
#define PD_ENTRIES					512
#define PD_ALIGN					4096
#define GET_PDE(addr, pdpte)		(uint64_t*)( ((pdpte) & 0xFFFFFFFFFF000) | (GET_BITS(addr, 21, 30) << 3))

#define SET_PDE_PHYSADDR(pde, paddr)		SET_BITS(pde, paddr, 0) // 51:21
#define GET_PDE_PHYSADDR(addr, pde)			(void*)( ((pde) & 0xFFFFFFFE00000) | GET_BITS(addr, 0, 21))

#define PAGE_SIZE (2 * 1024 * 1024) // 2 MB
uint64_t get_mem_unit_size() { return PAGE_SIZE; }


/* Helper functions for managing context and memory unit size: */

typedef struct {
	uint64_t* pml4;
} mem_hndl;
static mem_hndl* cur_hndl = NULL;

uint64_t get_mem_hndl_size() { return sizeof(mem_hndl); }

static uint64_t* get_entry(void* vaddr);
int create_mem_hndl(void* _hndl)
{
	mem_hndl* hndl = _hndl;
	// allocate space for PDPT and mark all pd pointers as not present
	hndl->pml4 = kmalloc_align(sizeof(uint64_t) * PML4_ENTRIES, PML4_ALIGN);
	if(!hndl->pml4)
		return VMEM_ERR_NOSPACE;

	if(hndl->pml4 != 0x203000){
		uart_printf("%p -- %p\r\n", (void*)cur_hndl->pml4[0], (void*)hndl->pml4[0]);
		uart_printf("%p -- %p\r\n", (void*)cur_hndl->pml4, (void*)hndl->pml4);
	}
	for(uint64_t i = 0; i < PML4_ENTRIES; ++i){
		hndl->pml4[i] = 0x0;
	}
	return 0;
}

void* get_current_mem_hndl() { return cur_hndl; }

int select_mem_hndl(void* _hndl, int activate)
{
	mem_hndl* hndl = _hndl;
	cur_hndl = hndl;
	if(activate)
		asm volatile("mov %0, %%cr3" :: "r" (hndl->pml4));
	return 0;
}

int destroy_mem_hndl(void* _hndl)
{
	mem_hndl* hndl = _hndl;
	for(uint64_t i = 0; i < PML4_ENTRIES; ++i){
		if(!(hndl->pml4[i] & PFLAG_PRESENT))
			continue;
		for(uint64_t j = 0; j < PDPT_ENTRIES; ++j){
			if(!(GET_PDPTE(0, hndl->pml4[i])[j] & PFLAG_PRESENT))
				continue;
			kfree(GET_PDE(0, GET_PDPTE(0, hndl->pml4[i])[j]));
		}
		kfree(GET_PDPTE(0, hndl->pml4[i]));
	}
	kfree(hndl->pml4);
	return 0;
}


/* Memory mapping functions: */

/* Makes an entry for specified virtual address.
*  If there was already an entry, the existing entry is returned, so check for the present bit.
*  Return value:
*	Returns a pointer to this entry, or NULL if:
*	- Physical address allocated by the kernel memory manager (kernlib.h) is too high.
*	- Kernel memory manager couldn't allocate more memory.
*/
static uint64_t* make_entry(void* vaddr)
{
	uint64_t* pml4e = GET_PML4E(vaddr);
	if(!(*pml4e & PFLAG_PRESENT)){ // allocate space for a page directory pointer table
		uint64_t* new_pdpt = kmalloc_align(sizeof(uint64_t) * PDPT_ENTRIES, PDPT_ALIGN);
		if(!new_pdpt)
			return NULL;
		for(uint64_t i = 0; i < PDPT_ENTRIES; ++i)
			new_pdpt[i] = 0x0;
		SET_PDPT(*pml4e, (uint64_t)new_pdpt);
		*pml4e |= PFLAG_PRESENT | PFLAG_CANWRITE;
	}

	uint64_t* pdpte = GET_PDPTE(vaddr, *pml4e);
	if(!(*pdpte & PFLAG_PRESENT)){ // allocate space for a page directory
		uint64_t* new_pd = kmalloc_align(sizeof(uint64_t) * PD_ENTRIES, PD_ALIGN);
		if(!new_pd)
			return NULL;
		for(uint64_t i = 0; i < PD_ENTRIES; ++i)
			new_pd[i] = 0x0;
		SET_PD(*pdpte, (uint64_t)new_pd);
		*pdpte |= PFLAG_PRESENT | PFLAG_CANWRITE;
	}

	uint64_t* pde = GET_PDE(vaddr, *pdpte);
	*pde |= PFLAG_PSIZE;
	return pde;
}
/* Gets entry for specified virtual address.
*  Return value:
*	Returns a pointer to corresponding entry, or NULL if any of indirection tables are not present.
*/
static uint64_t* get_entry(void* vaddr)
{
	uint64_t* pml4e = GET_PML4E(vaddr);
	if(!(*pml4e & PFLAG_PRESENT))
		return NULL;
	uint64_t* pdpte = GET_PDPTE(vaddr, *pml4e);
	if(!(*pdpte & PFLAG_PRESENT))
		return NULL;
	uint64_t* pde = GET_PDE(vaddr, *pdpte);
	return pde;
}


// Public interface

int vmemory_init(uint64_t mem_limit)
{
	allocator_init(mem_limit);
	return 0;
}


int map_alloc(void* vaddr, uint64_t usize, int flags)
{
	if(flags & VMEM_FLAG_SIZE_IN_BYTES)
		usize = (usize + (get_mem_unit_size() - 1)) / get_mem_unit_size();

	void* paddr = allocator_alloc(usize * PAGE_SIZE); // TODO: not always maintain continuity (configurable through flags)
	if(paddr == (void*)-1)
		return VMEM_ERR_NOSPACE;

	uint64_t* pde = make_entry(vaddr);
	if(!pde)
		return VMEM_ERR_NOSPACE;
	if(*pde & PFLAG_PRESENT)
		return VMEM_ERR_VIRT_OCCUPIED;

	SET_PDE_PHYSADDR(*pde, (uint64_t)paddr);
	*pde |= PFLAG_PRESENT | PFLAG_CANWRITE;

	return 0;
}

int map_phys(void* vaddr, void* paddr, uint64_t usize, int flags)
{
	if(flags & VMEM_FLAG_SIZE_IN_BYTES)
		usize = (usize + (get_mem_unit_size() - 1)) / get_mem_unit_size();

	allocator_alloc_addr(usize * PAGE_SIZE, paddr); // TODO: add a flag for ignoring failure to allocate page frame
	for(; usize; --usize, vaddr += PAGE_SIZE, paddr += PAGE_SIZE){
		uint64_t* pde = make_entry(vaddr);
		if(!pde)
			return VMEM_ERR_NOSPACE;
		if(*pde & PFLAG_PRESENT)
			return VMEM_ERR_VIRT_OCCUPIED;

		SET_PDE_PHYSADDR(*pde, (uint64_t)paddr);
		*pde |= PFLAG_PRESENT | PFLAG_CANWRITE;
	}
	return 0;
}

int unmap(void* vaddr, uint64_t usize, int flags)
{
	if(flags & VMEM_FLAG_SIZE_IN_BYTES)
		usize = (usize + (get_mem_unit_size() - 1)) / get_mem_unit_size();

	uint64_t* pde = get_entry(vaddr);
	if(!pde)
		return VMEM_NOT_MAPPED;

	allocator_free((void*)(GET_PDE_PHYSADDR(vaddr, *pde)), usize * PAGE_SIZE);
	*pde &= ~PFLAG_PRESENT;
	return 0;
}
