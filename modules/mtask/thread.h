#ifndef THREAD_H
#define THREAD_H

/* Thread API.
*/

#include <stdint.h>

typedef struct{
	struct{
		uint64_t rax, rbx, rcx, rdx, rsi, rdi;
		uint64_t rsp, rbp;
		uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
		uint64_t rip;
		uint64_t rflags;
		uint64_t cr0, cr2, cr3, cr4;
		uint64_t dr0, dr1, dr2, dr3, dr6, dr7;
		uint64_t efer;
		__attribute__ ((aligned(16))) unsigned char fx[512]; // memory for FXSAVE/FXRSTOR instructions
	} state;
} thread;

/* Switches context for the current thread (core) by changing register values
*  to ones in provided \to\ state, if \to\ is not NULL. Also writes current
*  register values into \from\ state, if \from\ is not NULL.
*/
void switch_context(thread* from, thread* to);

#endif
