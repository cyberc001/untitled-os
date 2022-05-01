#ifndef MTASK_H
#define MTASK_H

#include <stdint.h>

/* Multitasking module.
*/

#define MTASK_ERR_CANT_FIND_RSDP	-1

#define MTASK_AP_BOOT_TRY_COUNT		100 	// count of boot flag checks (with 5ms break between them)

/* Initializes the module (called right after loading the module, prior to any other function calls).
*  Return value:
*	0			OK
*	non-zero	error, see code above
*/
int init();

#endif
