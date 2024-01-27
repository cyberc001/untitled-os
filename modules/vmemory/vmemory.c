#include "vmemory.h"

#include "dev/uart.h"
#include "kernlib/kernmem.h"

#include "allocator.h"

#include "bits.h"

/* IA-32e paging */

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

// PML4:
uint64_t* pml4;
#define PML4_ENTRIES				512
#define PML4_ALIGN					4096
#define GET_PML4E(addr)				((uint64_t*)((uint64_t)cur_hndl->pml4 | (GET_BITS(addr, 39, 48) << 3)))
#define SET_PDPT(pml4e, paddr)		SET_BITS(pml4e, paddr, 0) // 51:12

// PDPT:
#define PDPT_ENTRIES				512
#define PDPT_ALIGN					4096
#define GET_PDPTE(addr, pml4e)		((uint64_t*)( ((pml4e) & 0xFFFFFFFFFF000) | (GET_BITS(addr, 30, 39) << 3)))
#define SET_PD(pdpte, paddr)		SET_BITS(pdpte, paddr, 0) // 51:12

// PDE:
#define PD_ENTRIES					512
#define PD_ALIGN					4096
#define GET_PDE(addr, pdpte)		((uint64_t*)( ((pdpte) & 0xFFFFFFFFFF000) | (GET_BITS(addr, 21, 30) << 3)))
#define SET_PT(pde, paddr)			SET_BITS(pde, paddr, 0) // 51:12

#define SET_PDE_PHYSADDR(pde, paddr)		SET_BITS(pde, paddr, 0) // 51:21
#define GET_PDE_PHYSADDR(addr, pde)			(void*)( ((pde) & 0xFFFFFFFE00000) | GET_BITS(addr, 0, 21))

// PTE:
#define PT_ENTRIES					512
#define PT_ALIGN					4096
#define GET_PTE(addr, pde)			((uint64_t*)( ((pde) & 0xFFFFFFFFFF000) | (GET_BITS(addr, 12, 21) << 3)))

#define SET_PTE_PHYSADDR(pte, paddr)		SET_BITS(pte, paddr, 0) // 51:12
#define GET_PTE_PHYSADDR(addr, pte)			(void*)( ((pte) & 0xFFFFFFFFFF000) | GET_BITS(addr, 0, 12)) 

#define PAGE_SIZE 4096
#define PAGE_SIZE2 (2 * 1024 * 1024)
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
	// allocate space for PML4 and mark all PML4 entries as not present
	hndl->pml4 = kmalloc_align(sizeof(uint64_t) * PML4_ENTRIES, PML4_ALIGN);

	if(!hndl->pml4)
		return VMEM_ERR_NOSPACE;
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
*  Argument page_size governs at what level the function stops and puts size bit (or doesn't put size bit if this is a 4kb page).
*  Return value:
*	Returns a pointer to this entry, or NULL if:
*	- Physical address allocated by the kernel memory manager (kernlib.h) is too high.
*	- Kernel memory manager couldn't allocate more memory.
*/
static uint64_t* make_entry(void* vaddr, size_t page_size)
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

	if(page_size == PAGE_SIZE2){
		uint64_t* pde = GET_PDE(vaddr, *pdpte);
		*pde |= PFLAG_PSIZE;
		return pde;
	}
	
	// otherwise page_size == PAGE_SIZE
	uint64_t* pde = GET_PDE(vaddr, *pdpte);
	if(!(*pde & PFLAG_PRESENT)){ // allocate space for a page table
		uint64_t* new_pt = kmalloc_align(sizeof(uint64_t) * PT_ENTRIES, PT_ALIGN);
		if(!new_pt)
			return NULL;
		for(uint64_t i = 0; i < PT_ENTRIES; ++i)
			new_pt[i] = 0x0;
		SET_PT(*pde, (uint64_t)new_pt);
		*pde |= PFLAG_PRESENT | PFLAG_CANWRITE;
	}

	uint64_t* pte = GET_PTE(vaddr, *pde);
	return pte;
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
	if(!(*pde & PFLAG_PRESENT))
		return NULL;
	if(*pde & PFLAG_PSIZE)
		return pde;
	uint64_t* pte = GET_PTE(vaddr, *pde);
	return pte;
}


// Public interface

int vmemory_init(uint64_t mem_limit)
{
	allocator_init(mem_limit);
	return 0;
}

#define MAP_PAGE(vaddr, paddr)\
{\
	uint64_t* ent = make_entry(vaddr, PAGE_SIZE);\
	if(!ent)\
		return VMEM_ERR_NOSPACE;\
	if(*ent & PFLAG_PRESENT)\
		return VMEM_ERR_VIRT_OCCUPIED;\
	SET_PTE_PHYSADDR(*ent, (uint64_t)paddr);\
	*ent |= PFLAG_PRESENT | PFLAG_CANWRITE;\
}
#define MAP_PAGE2(vaddr, paddr)\
{\
	uint64_t* ent = make_entry(vaddr, PAGE_SIZE2);\
	if(!ent)\
		return VMEM_ERR_NOSPACE;\
	if(*ent & PFLAG_PRESENT)\
		return VMEM_ERR_VIRT_OCCUPIED;\
	SET_PDE_PHYSADDR(*ent, (uint64_t)paddr);\
	*ent |= PFLAG_PRESENT | PFLAG_CANWRITE;\
}

#define MAP_PAGE_ALLOC(vaddr)\
{\
	void* paddr = allocator_alloc_align(PAGE_SIZE, PAGE_SIZE);\
	if(paddr == (void*)-1)\
		return VMEM_ERR_NOSPACE;\
	MAP_PAGE(vaddr, paddr);\
}
#define MAP_PAGE2_ALLOC(vaddr)\
{\
	void* paddr = allocator_alloc_align(PAGE_SIZE2, PAGE_SIZE2);\
	if(paddr == (void*)-1)\
		break;\
	MAP_PAGE2(vaddr, paddr);\
}

#define UNMAP_PAGE(vaddr)\
{\
	uint64_t* ent = get_entry(vaddr);\
	if(!ent)\
		return VMEM_NOT_MAPPED;\
	allocator_free((void*)(GET_PDE_PHYSADDR(vaddr, *ent)), PAGE_SIZE);\
	*ent &= ~PFLAG_PRESENT;\
}
#define UNMAP_PAGE2(vaddr)\
{\
	uint64_t* ent = get_entry(vaddr);\
	if(!ent)\
		return VMEM_NOT_MAPPED;\
	allocator_free((void*)(GET_PDE_PHYSADDR(vaddr, *ent)), PAGE_SIZE2);\
	*ent &= ~PFLAG_PRESENT;\
}

int map_alloc(void* vaddr, uint64_t usize, int flags)
{
	if(flags & VMEM_FLAG_SIZE_IN_BYTES)
		usize = (usize + (PAGE_SIZE - 1)) / PAGE_SIZE;

	if(flags & VMEM_FLAG_MAINTAIN_CONTINUITY){
		void* paddr = allocator_alloc_align(usize * PAGE_SIZE, PAGE_SIZE);
		if(paddr == (void*)-1)
			return VMEM_ERR_NOSPACE;
		for(; usize && (uintptr_t)paddr % PAGE_SIZE2 > 0 && (uintptr_t)vaddr % PAGE_SIZE2 > 0; --usize, vaddr += PAGE_SIZE, paddr += PAGE_SIZE)
			MAP_PAGE(vaddr, paddr);
		for(; usize >= PAGE_SIZE2 / PAGE_SIZE; usize -= PAGE_SIZE2 / PAGE_SIZE, vaddr += PAGE_SIZE2, paddr += PAGE_SIZE2)
			MAP_PAGE2(vaddr, paddr);
		for(; usize; --usize, vaddr += PAGE_SIZE, paddr += PAGE_SIZE)
			MAP_PAGE(vaddr, paddr);
	}
	else{
		for(; usize && (uintptr_t)vaddr % PAGE_SIZE2 > 0; --usize, vaddr += PAGE_SIZE) 
			MAP_PAGE_ALLOC(vaddr); 
		for(; usize >= PAGE_SIZE2 / PAGE_SIZE; usize -= PAGE_SIZE2 / PAGE_SIZE, vaddr += PAGE_SIZE2)
			MAP_PAGE2_ALLOC(vaddr);
		for(; usize; --usize, vaddr += PAGE_SIZE)
			MAP_PAGE_ALLOC(vaddr);
	}
	return 0;
}

int map_phys(void* vaddr, void* paddr, uint64_t usize, int flags)
{
	if(flags & VMEM_FLAG_SIZE_IN_BYTES)
		usize = (usize + (PAGE_SIZE - 1)) / PAGE_SIZE;

	allocator_alloc_addr(usize * PAGE_SIZE, paddr);

	for(; usize && (uintptr_t)paddr % PAGE_SIZE2 > 0 && (uintptr_t)vaddr % PAGE_SIZE2 > 0; --usize, vaddr += PAGE_SIZE, paddr += PAGE_SIZE)
		MAP_PAGE(vaddr, paddr);
	for(; usize >= PAGE_SIZE2 / PAGE_SIZE; usize -= PAGE_SIZE2 / PAGE_SIZE, vaddr += PAGE_SIZE2, paddr += PAGE_SIZE2)
		MAP_PAGE2(vaddr, paddr);
	for(; usize; --usize, vaddr += PAGE_SIZE, paddr += PAGE_SIZE)
		MAP_PAGE(vaddr, paddr);
	return 0;
}

int unmap(void* vaddr, uint64_t usize, int flags)
{
	if(flags & VMEM_FLAG_SIZE_IN_BYTES)
		usize = (usize + (get_mem_unit_size() - 1)) / PAGE_SIZE;

	for(; usize && (uintptr_t)vaddr % PAGE_SIZE2 > 0; --usize, vaddr += PAGE_SIZE) 
		UNMAP_PAGE(vaddr);
	for(; usize >= PAGE_SIZE2 / PAGE_SIZE; usize -= PAGE_SIZE2 / PAGE_SIZE, vaddr += PAGE_SIZE2)
		UNMAP_PAGE2(vaddr);
	for(; usize; --usize, vaddr += PAGE_SIZE) 
		UNMAP_PAGE(vaddr);
	return 0;
}
