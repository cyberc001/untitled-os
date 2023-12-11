#ifndef THREAD_H
#define THREAD_H

#include <stdint.h>

/* Thread API */

#define THREAD_FLAG_SLEEPING		0x1

#define thread_wakeup_is_later(t1, t2) ( (t1).sleep_overflow == (t2).sleep_overflow ? ((t1).sleep_until > (t2).sleep_until) : (t1).sleep_overflow > (t2).sleep_overflow )
#define thread_wakeup_is_earlier(t1, t2) (!thread_wakeup_is_later(t1, t2))
#define thread_should_wakeup_now(t, timer_val) (!(t).sleep_overflow && (t).sleep_until <= (timer_val))

typedef struct process process;

typedef struct {
	__attribute__ ((packed)) __attribute__ ((aligned(16))) struct {
		uint64_t rax, rbx, rcx, rdx, rsi, rdi;
		uint64_t rsp, rbp;
		uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
		uint64_t rip;
		uint64_t rflags;
		uint64_t cr0, cr2, cr3, cr4;
		uint64_t dr0, dr1, dr2, dr3, dr6, dr7;
		__attribute__ ((aligned(16))) unsigned char fx[512]; // memory for FXSAVE/FXRSTOR instructions
	} state;

	int flags;

	uint64_t vruntime;
	uint64_t weight;

	uint64_t sleep_until; // valid only if (flags & THREAD_FLAG_SLEEPING)
	int sleep_overflow; // 1 if current hpet timer value + sleep duration >= sizeof(uint64_t), set to 0 when hpet timer itself overflows

	process* parent_proc;

	/* bunch of shit necessary only for dequeing */
	void* tree; // rbtree that contains the node
	void* hndl; // node in rbtree that contains the thread
} thread;

#define MTASK_SAVE_CONTEXT(thread_pt)\
{\
	__label__ __end_label;\
	(thread_pt)->state.rip = (uintptr_t)&&__end_label;\
	asm volatile("push %0" :: "m"(thread_pt));\
	asm volatile(\
	"push   %%rax\n"\
	"mov    0x8(%%rsp),%%rax\n"\
	"mov    %%rbx,0x8(%%rax)\n"\
	"mov    %%rcx,0x10(%%rax)\n"\
	"mov    %%rdx,0x18(%%rax)\n"\
	"mov    %%rsi,0x20(%%rax)\n"\
	"mov    %%rdi,0x28(%%rax)\n"\
	"mov    %%rsp,0x30(%%rax)\n"\
	"subq   $0x18,0x30(%%rax)\n"\
	"mov    %%rbp,0x38(%%rax)\n"\
	"mov    %%r8,0x40(%%rax)\n"\
	"mov    %%r9,0x48(%%rax)\n"\
	"mov    %%r10,0x50(%%rax)\n"\
	"mov    %%r11,0x58(%%rax)\n"\
	"mov    %%r12,0x60(%%rax)\n"\
	"mov    %%r13,0x68(%%rax)\n"\
	"mov    %%r14,0x70(%%rax)\n"\
	"mov    %%r15,0x78(%%rax)\n"\
/*"mov    0x8(%%rsp),%%rbx\n"*/\
/*"mov    %%rbx,0x80(%%rax)\n"*/\
	"pushfq\n"\
	"mov    (%%rsp),%%rbx\n"\
	"popfq\n"\
	"mov    %%rbx,0x88(%%rax)\n"\
	"mov    %%cr0,%%rbx\n"\
	"mov    %%rbx,0x90(%%rax)\n"\
	"mov    %%cr2,%%rbx\n"\
	"mov    %%rbx,0x98(%%rax)\n"\
	"mov    %%cr3,%%rbx\n"\
	"mov    %%rbx,0xa0(%%rax)\n"\
	"mov    %%cr4,%%rbx\n"\
	"mov    %%rbx,0xa8(%%rax)\n"\
	"mov    %%db0,%%rbx\n"\
	"mov    %%rbx,0xb0(%%rax)\n"\
	"mov    %%db1,%%rbx\n"\
	"mov    %%rbx,0xb8(%%rax)\n"\
	"mov    %%db2,%%rbx\n"\
	"mov    %%rbx,0xc0(%%rax)\n"\
	"mov    %%db3,%%rbx\n"\
	"mov    %%rbx,0xc8(%%rax)\n"\
	"mov    %%db6,%%rbx\n"\
	"mov    %%rbx,0xd0(%%rax)\n"\
	"mov    %%db7,%%rbx\n"\
	"mov    %%rbx,0xd8(%%rax)\n"\
	"fxsave 0xe0(%%rax)\n"\
	"mov    %%rax,%%rdi\n"\
	"pop    %%rax\n"\
	"mov    %%rax,(%%rdi)\n"\
	"add	$0x8, %%rsp\n"\
	::: "memory", "rax", "rbx"\
	);\
	__end_label:;\
}

/* Shouldn't be called directly via C calling conventions.
*  DEFINED in ap_periodic_switch.s
*/
void load_context();

#endif
