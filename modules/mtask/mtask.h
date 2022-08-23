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
#define MTASK_TSS_STACK_SIZE			1024

extern uint8_t core_num, bsp_lapic_id;

typedef struct {
	void* jmp_loc;
	void* no_code_fallback_jmp;
} core_info_t;
core_info_t* core_info;

/* Initializes the module (called right after loading the module, prior to any other function calls).
*  Return value:
*	0			OK
*	non-zero	error, see code above
*/
int mtask_init();

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

#define MTASK_SWITCH_TIMER_TIME		10				// in milliseconds
#define MTASK_SWITCH_TIMER_GATE		0x30			// interrupt gate number
/* Sets up a periodic timer for task switching (for AP this function is executed from).
*	Return value:
*	0			OK
*	non-zero	error, see code above
*/
int ap_set_timer();

#endif
