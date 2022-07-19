#ifndef THREAD_H
#define THREAD_H

/* Thread API.
*/

#include <stdint.h>

typedef struct{
	__attribute__ ((packed)) struct{
		uint64_t rax, rbx, rcx, rdx, rsi, rdi;
		uint64_t rsp, rbp;
		uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
		uint64_t rip;
		uint64_t rflags;
		uint64_t cr0, cr2, cr3, cr4;
		uint64_t dr0, dr1, dr2, dr3, dr6, dr7;
		__attribute__ ((aligned(16))) unsigned char fx[512]; // memory for FXSAVE/FXRSTOR instructions
	} state;
} thread;

#define MTASK_CALL_CONTEXT_FUNC(func_ptr, stack_arg)\
{\
	asm volatile("push %0" :: "m"(stack_arg) : "memory", "rsp");\
	asm volatile("call *%0" :: "m"(func_ptr));\
	asm volatile("add %%rsp, 8" ::: "rsp");\
}
/* Shouldn't be called directly via C calling conventions! */
void save_context();
void load_context();

#endif

