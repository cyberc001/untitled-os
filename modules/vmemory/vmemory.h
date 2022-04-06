#ifndef MODULE_VMEMORY_H
#define MODULE_VMEMORY_H

#include <stdint.h>

/* Basic virtual memory module, that utilizes paging.
*  It tries to be as generic as possible, but main assumption is that paging well be used.
*  To be able to fit any kind of memory model, API for a virtual memory model should define function get_mem_unit_size().
*/

#define VMEM_PFLAG_SUPERVISOR	0b001		// U/S set: pages can only be accessed by supervisor
#define VMEM_PLFAG_WRITE	0b010		// R/W set: pages can be written to (otherwise read-only)

#define VMEM_ERR_NOSPACE	-1		// Not enough free space for allocation
#define VMEM_ERR_PHYS_OCCUPIED	-2		// Specified physical memory is already occupied
#define VMEM_ERR_VIRT_OCCUPIED	-3		// Specified virtual memory is already occupied
#define VMEM_NOT_MAPPED		-4		// Requested virtual memory is not mapped

/* Returns the size of a memory unit used (page, buddy allocator chunk, byte), in bytes.
*/
uint64_t get_mem_unit_size();

/* Initializes the module (called right after loading the module, prior to any other function calls).
*  Arguments:
*       mem_limit - maximum amount of physical memory.
*                   just used to set up the initial free memory blocks structure (RB tree in this module). Identity mapping what kernel has
*                   used so far as well as various memory-mapped I/O is done by the module loader.
*/
int init(uint64_t mem_limit);

/* Enables the memory module (called after identity memory mapping by kernel).
*/
int enable();

/* Virtual address mapping functions */

/* Maps a chunk of memory on specified virtual address to some physical address returned by the allocator.
*  Arguments:
*	vaddr - [page-aligned] virtual address to map
*	usize - size of the memory chunk in memory units
*	flags - page flags, see above
*  Return value:
*	0		OK
*	non-zero	error, see error codes above
*/
int map_alloc(void* vaddr, uint64_t usize, int flags);

/* Maps a chunk of memory on specified virtual address to specified physical address.
*  Arguments:
*	vaddr - [page-aligned] virtual address to map
*	paddr - [page-aligned] physical address to map to
*	usize - size of the memory chunk in memory units
*	flags - page flags, see above
*  Return value:
*	0		OK
*	non-zero	error, see error codes above
*/
int map_phys(void* vaddr, void* paddr, uint64_t usize, int flags);

/* Unmaps a chunk of memory on specified virtual address.
*  Arguments:
*	vaddr - [page-aligned] virtual address to map
*	usize - size of the memory chunk in memory units
*  Return value:
*	0		OK
*	non-zero	error, see error codes above
*/
int unmap(void* vaddr, uint64_t usize);

#endif
