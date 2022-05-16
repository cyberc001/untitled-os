#ifndef CPU_INT_H
#define CPU_INT_H

#include <stdint.h>
#include <stddef.h>

static inline void cpu_interrupt_set(int enabled)
{
	if(enabled)	asm volatile ("sti");
	else		asm volatile ("cli");
}


// Generic interrupt types
#define CPU_INT_TYPE_INTERRUPT	0
#define CPU_INT_TYPE_TRAP		1
#define CPU_INT_TYPE_TASK		2

#define CPU_SYSCALL_GATE		0x80

void cpu_interrupt_init();

/* Returns 0 if gate slot is out of bounds */
int cpu_interrupt_set_gate(void (*func)(), uint32_t slot, uint8_t type);


/*      ------ SYSTEM CALLS CONVENTION USED IN THE MICROCORE ------
*  Register usage:
*   RAX - interrupt service index
*  	every other general-purpose register - interrupt service arguments
*
*  Suggested standard layout for system calls:
*   0 - exit
*   TODO
*
*  Notes:
*  - Unlike CPU interrupt vectors, space for system calls is dynamically
*  allocated. For faster access, unused slots are filled with duds that do
*  nothing. Hence the limit CPU_SYSCALL_MAX prevents faulty code from
*  using too much memory trying to set a nonsense interrupt service.
*/
typedef void (*syscall_func_t)();
#define CPU_SYSCALL_MAX			0xFFF

/* Sets a syscall service.
*  Return values:
*    0 - gate is out-of-bounds (>= CPU_SYSCALL_MAX)
*    1 - OK
*/
int cpu_interrupt_set_service(syscall_func_t func, size_t service_idx);

#endif
