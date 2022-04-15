#ifndef MTASK_H
#define MTASK_H

#include <stdint.h>

/* Multitasking module.
*/

/* Initializes the module (called right after loading the module, prior to any other function calls).
*  Arguments:
*	cpu_count - count of logical CPUs (what you can run instructions on)
*	cpu_lapic_ids - array of lAPIC CPU IDs, of size cpu_count
*  Return value:
*	0			OK
*	non-zero	error, see code above
*/
int init(uint64_t cpu_count, uint32_t* cpu_lapic_ids);

#endif
