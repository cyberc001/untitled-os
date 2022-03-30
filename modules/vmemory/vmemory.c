#include "vmemory.h"

#include "dev/uart.h"
#include "kernlib/kernmem.h"

#include "allocator.h"


#define MAX_PHYS_ADDR		48		// TODO: check what is actually reported by CPU

#define PFLAG_PRESENT		(1 << 0)
#define PFLAG_CANWRITE		(1 << 1)	// if 0, writes are not allowed
#define PFLAG_SUPERVISOR	(1 << 2)	// if 0, user-mode access is not allowed
#define PFLAG_WRITETHROUGH	(1 << 3)
#define PFLAG_CACHEDISABLE	(1 << 4)	// if 1, disables caching this page
#define PFLAG_ACCESSED		(1 << 5)
#define PFLAG_DIRTY		(1 << 6)
#define PFLAG_PSIZE		(1 << 7)	// if 1 at:
						// Page Directory Pointer Table Entry (PDPTE): maps a 1 GB page
						// Page Directory Entry (PDE): maps a 2 MB page
						// if 0, pages have size of 4 KB
#define PFLAG_GLOBAL		(1 << 8)	// if 1, translation is global (not invalidated in TLB)
#define PFLAG_PAT		(1 << 12)
#define PFLAG_XD		(1 << 63)	// if 1, does not allow instruction fetches from this page (if CPU supports it)

// PDPT entry:
#define PDPTE_PD_GET_PHYSADDR(e)		((void*)( ((e) >> 12) & ( ((uint64_t)1 << (MAX_PHYS_ADDR - 12)) - 1) ))
#define PDPTE_PD_SET_PHYSADDR(e, addr)		{ (e) |= (uint64_t)(addr) << 12; }
#define PDPTE_SIZE				(1024 * 1024 * 1024)		// 1 GiB
#define PDPT_ENTRIES				4
#define PDPT_ALIGN				32

// PD entry:
#define PDE_2MBPAGE_GET_PHYSADDR(e)		((void*)( ((e) >> 21) & ( ((uint64_t)1 << (MAX_PHYS_ADDR - 21)) - 1) ))
#define PDE_2MBPAGE_SET_PHYSADDR(e, addr)	{ (e) |= (uint64_t)(addr) << 21; }
#define PDE_SIZE				(2 * 1024 * 1024)		// 2 MiB
#define PD_ALIGN				4096
#define PD_ENTRIES				512


uint64_t* pdpt;		// Page Directory Pointer Table; allocated by kernmem.h functions in order to reside in lower half


// Functions for modifying page structures

/* Makes an entry for specified virtual address.
*  If there was already an entry, the existing entry is returned, so check for the present bit.
*  Return value:
*	Returns a pointer to this entry, or NULL if:
*	- Physical address allocated by the kernel memory manager (kernlib.h) is too high.
*	- Kernel memory manager couldn't allocate more memory.
*/
static uint64_t* make_entry(void* vaddr)
{
	uint64_t* pdpt_e = pdpt + (uint64_t)vaddr / (uint64_t)PDPTE_SIZE;
	if(!(*pdpt_e & PFLAG_PRESENT)){ // allocate space for a page directory
		uint64_t* new_pd = kmalloc_align(sizeof(uint64_t) * PD_ENTRIES, PD_ALIGN);
		if( new_pd > (uint64_t*)((uint64_t)1 << ((MAX_PHYS_ADDR - 12) - 1)) || !new_pd){
			if(new_pd)
				kfree(new_pd);
			return NULL;
		}
		for(uint64_t i = 0; i < PD_ENTRIES; ++i)
			new_pd[i] = 0x0;
		PDPTE_PD_SET_PHYSADDR(*pdpt_e, new_pd);
		*pdpt_e |= PFLAG_PRESENT;
	}

	uint64_t* pd = PDPTE_PD_GET_PHYSADDR(*pdpt_e);
	uint64_t* pd_e = pd + ((uint64_t)vaddr % PDPTE_SIZE / PDE_SIZE);
	return pd_e;
}
/* Gets entry for specified virtual address.
*  Return value:
*	Returns a pointer to corresponding entry, or NULL if it's not present.
*/
static uint64_t* get_entry(void* vaddr)
{
	uint64_t* pdpt_e = pdpt + (uint64_t)vaddr / (uint64_t)PDPTE_SIZE;
	if(!(*pdpt_e & PFLAG_PRESENT))
		return NULL;
	uint64_t* pd = PDPTE_PD_GET_PHYSADDR(*pdpt_e);
	uint64_t* pd_e = pd + ((uint64_t)vaddr % PDPTE_SIZE / PDE_SIZE);
	return pd_e;
}


// Public interface

int init(uint64_t mem_limit)
{
	allocator_init(mem_limit);

	// map a page at NULL to handle allocator errors (it returns 0 if it runs out of physical memory)
	allocator_alloc_addr(VMEM_PAGE_SIZE, NULL);

	// allocate space for PDPT and mark all pd pointers as not present
	pdpt = kmalloc_align(sizeof(uint64_t) * PDPT_ENTRIES, PDPT_ALIGN);
	for(uint64_t i = 0; i < PDPT_ENTRIES; ++i)
		pdpt[i] = 0x0;

	uart_printf("map_alloc test: %d\r\n", map_alloc((void*)0xdeadbeef, 1, 0));
	uart_printf("map_alloc repeat test: %d\r\n", map_alloc((void*)0xdeadbeef, 1, 0));

	uart_printf("map_phys test on null page: %d\r\n", map_phys((void*)0x0, (void*)0x0, 1, 0));
	uart_printf("map_phys test on 0xd0000000: %d\r\n", map_phys((void*)0xd0000000, (void*)0xd0000000, 1, 0));
	uart_printf("unmap test on 0xd0000000: %d\r\n", unmap((void*)0xd0000000, 1));
	uart_printf("map_phys test on 0xd0000000: %d\r\n", map_phys((void*)0xd0000000, (void*)0xd0000000, 1, 0));

	return 0;
}

int map_alloc(void* vaddr, uint64_t psize, int flags)
{
	void* paddr = allocator_alloc(psize * VMEM_PAGE_SIZE);
	if(!paddr)
		return VMEM_ERR_NOSPACE;

	uint64_t* pe = make_entry(vaddr);
	if(!pe)
		return VMEM_ERR_NOSPACE;
	if(*pe & PFLAG_PRESENT)
		return VMEM_ERR_VIRT_OCCUPIED;

	PDE_2MBPAGE_SET_PHYSADDR(*pe, (uint64_t)paddr >> 21);
	*pe |= PFLAG_PRESENT;

	return 0;
}

int map_phys(void* vaddr, void* paddr, uint64_t psize, int flags)
{
	paddr = allocator_alloc_addr(psize * VMEM_PAGE_SIZE, paddr);
	if(!paddr)
		return VMEM_ERR_PHYS_OCCUPIED;

	uint64_t* pe = make_entry(vaddr);
	if(!pe)
		return VMEM_ERR_NOSPACE;
	if(*pe & PFLAG_PRESENT)
		return VMEM_ERR_VIRT_OCCUPIED;

	PDE_2MBPAGE_SET_PHYSADDR(*pe, (uint64_t)paddr >> 21);
	uart_printf("map_phys::::::%p\r\n", (uint64_t)PDE_2MBPAGE_GET_PHYSADDR(*pe) << 21);
	*pe |= PFLAG_PRESENT;
	return 0;
}

int unmap(void* vaddr, uint64_t psize)
{
	uint64_t* pe = get_entry(vaddr);
	if(!pe)
		return VMEM_NOT_MAPPED;

	uart_printf("unmap::::::%p\r\n", (void*)((uint64_t)PDE_2MBPAGE_GET_PHYSADDR(*pe) << 21));
	allocator_free((void*)((uint64_t)PDE_2MBPAGE_GET_PHYSADDR(*pe) << 21), psize * VMEM_PAGE_SIZE);
	*pe &= ~PFLAG_PRESENT;
	return 0;
}
