#ifndef MTASK_H
#define MTASK_H

#include <stdint.h>
#include <stddef.h>

/* Multitasking module.
*/

#define MTASK_ERR_CANT_FIND_RSDP		-1
#define MTASK_ERR_AP_IDX_DOESNT_EXIST	-2
#define MTASK_ERR_GATE_OOB				-3

#define MTASK_AP_BOOT_TRY_COUNT			100 	// count of boot flag checks (with 5ms break between them)

/* Initializes the module (called right after loading the module, prior to any other function calls).
*  Return value:
*	0			OK
*	non-zero	error, see code above
*/
int init();

/* Returns amount of cores detected on the machine. */
uint8_t get_core_num();

/* Returns index of the BSP. */
size_t get_bsp_idx();


/* Makes specified AP jump to the target address. TODO: document more
*  Return value:
*	0			OK
*	non-zero	error, see code above
*/
int ap_jump(size_t ap_idx, void* loc);

#define MTASK_SWITCH_TIMER_TIME		1000*1000		// in microseconds (us)
#define MTASK_SWITCH_TIMER_GATE		0x30				// interrupt number
/* Sets up a periodic timer for task switching (for AP this function is executed from).
*	Return value:
*	0			OK
*	non-zero	error, see code above
*/
int ap_set_timer();

#endif
