#ifndef MODULE_VMEMORY_H
#define MODULE_VMEMORY_H

#include <stdint.h>

/* Basic, standard virtual memory module, that utilizes paging. */

/* Initializes the module (called right after loading the module, prior to any other function calls).
*  Arguments:
*       mem_limit - maximum amount of physical memory.
*                   just used to set up the initial free memory blocks structure (RB tree in this module). Identity mapping what kernel has
*                   used so far as well as various memory-mapped I/O is done by the module loader.
*/
int init(uint64_t mem_limit);

#endif
