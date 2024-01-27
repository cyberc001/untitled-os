#ifndef MODULE_VMEMORY_H
#define MODULE_VMEMORY_H

#include <stdint.h>

/* Virtual memory module, that utilizes paging.
*  It tries to be as generic as possible, but main assumption is that paging well be used.
*  To be able to fit any kind of memory model, API for a virtual memory model should utilize function get_mem_unit_size().
*/

#define VMEM_FLAG_SUPERVISOR				0b0001		// U/S set: pages can only be accessed by supervisor
#define VMEM_FLAG_WRITE						0b0010		// R/W set: pages can be written to (otherwise read-only)
#define VMEM_FLAG_SIZE_IN_BYTES				0b0100		// usize argument specifies size in bytes, not memory unit
#define VMEM_FLAG_MAINTAIN_CONTINUITY		0b1000		// allocate a continous chunk of memory (only affects map_alloc)
#define VMEM_FLAG_IGNORE_ALREADY_ALLOCATED	0b10000		// don't return with an error if memory space is already occupied

#define VMEM_ERR_NOSPACE			-1			// Not enough free space for allocation
#define VMEM_ERR_PHYS_OCCUPIED		-2			// Specified physical memory is already occupied
#define VMEM_ERR_VIRT_OCCUPIED		-3			// Specified virtual memory is already occupied
#define VMEM_NOT_MAPPED				-4			// Requested virtual memory is not mapped


/* Helper functions for managing context and memory unit size: */

/* Initializes the module (called right after loading the module, prior to any other function calls).
*  Arguments:
*       mem_limit - maximum amount of physical memory.
*                   just used to set up the initial free memory blocks structure (RB tree in this module). Identity mapping what kernel has
*                   used so far as well as various memory-mapped I/O is done by the module loader.
*  Return value:
*	0 			OK
*	non-zero 	error, see code above
*/
int vmemory_init(uint64_t mem_limit);

/* Returns size of memory unit used (page, buddy allocator chunk, byte), in bytes. */
uint64_t get_mem_unit_size();

/* Returns size of memory handler instance, used to store memory context. */
uint64_t get_mem_hndl_size();
/* Creates (initializes) a memory handler.
*  Memory for a handler is allocated externally, using get_mem_hndl_size().
*  Return value:
*	0 			OK
*	non-zero 	error, see code above
*/
int create_mem_hndl(void* hndl);
/* Changes current (active) memory handler to one provided in function arguments.
*  (Intended for use with processes)
*  Arguments:
*	hndl - memory handler
*	activate - if 0, just changes current handler, if 1, also uses it for address translation and/or memory protection.
*  Return value:
*	0 			OK
*	non-zero 	error, see code above
*/
int select_mem_hndl(void* hndl, int activate);
/* Returns currently selected memory handler (not just CR3 value,
*  but memory handler operated on by vmemory module functions.
*/
void* get_current_mem_hndl();
/* Destroys (frees) a memory handler.
*  Memory for a handler is allocated externally, using get_mem_hndl_size().
*  Return value:
*	0 			OK
*	non-zero 	error, see code above
*/
int destroy_mem_hndl(void* hndl);


/* Memory mapping functions: */

/* Maps a chunk of memory on specified virtual address to some physical address returned by the allocator.
*  Arguments:
*	vaddr - [page-aligned] virtual address to map
*	usize - size of the memory chunk in memory units
*	flags - virtual memory module flags, see above
*  Return value:
*	0			OK
*	non-zero	error, see error codes above
*/
int map_alloc(void* vaddr, uint64_t usize, int flags);

/* Maps a chunk of memory on specified virtual address to specified physical address.
*  Arguments:
*	vaddr - [page-aligned] virtual address to map
*	paddr - [page-aligned] physical address to map to
*	usize - size of the memory chunk in memory units
*	flags - virtual memory module flags, see above
*  Return value:
*	0			OK
*	non-zero	error, see error codes above
*/
int map_phys(void* vaddr, void* paddr, uint64_t usize, int flags);

/* Unmaps a chunk of memory on specified virtual address.
*  Arguments:
*	vaddr - [page-aligned] virtual address to map
*	usize - size of the memory chunk in memory units
*	flags - virtual memory module flags, see above
*  Return value:
*	0			OK
*	non-zero	error, see error codes above
*/
int unmap(void* vaddr, uint64_t usize, int flags);

#endif
