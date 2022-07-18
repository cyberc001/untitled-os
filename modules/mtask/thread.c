#include "thread.h"

void switch_context(thread* from, thread* to)
{
	if(from){
		asm volatile("mov %%rax, %0" : "=m" (from->state.rax));
		asm volatile("mov %%rbx, %0" : "=m" (from->state.rbx));
		asm volatile("mov %%rcx, %0" : "=m" (from->state.rcx));
		asm volatile("mov %%rdx, %0" : "=m" (from->state.rdx));
		asm volatile("mov %%rsi, %0" : "=m" (from->state.rsi));
		asm volatile("mov %%rdi, %0" : "=m" (from->state.rdi));
		asm volatile("mov %%rsp, %0" : "=m" (from->state.rsp));
		asm volatile("mov %%rbp, %0" : "=m" (from->state.rbp));
		asm volatile("mov %%r8, %0" : "=m" (from->state.r8));
		asm volatile("mov %%r9, %0" : "=m" (from->state.r9));
		asm volatile("mov %%r10, %0" : "=m" (from->state.r10));
		asm volatile("mov %%r11, %0" : "=m" (from->state.r11));
		asm volatile("mov %%r12, %0" : "=m" (from->state.r12));
		asm volatile("mov %%r13, %0" : "=m" (from->state.r13));
		asm volatile("mov %%r14, %0" : "=m" (from->state.r14));
		asm volatile("mov %%r15, %0" : "=m" (from->state.r15));
		asm volatile("mov (%%rsp), %%rax\n\t mov %%rax, %0": "=m" (from->state.rip));
		asm volatile("pushf\n\t"
					 "mov (%%rsp), %%rax\n\t"
					 "popf\n\t"
					 "mov %%rax, %0" : "=m" (from->state.rflags));
		asm volatile("mov %%cr0, %%rax\n\t mov %%rax, %0" : "=m" (from->state.cr0));
		asm volatile("mov %%cr2, %%rax\n\t mov %%rax, %0" : "=m" (from->state.cr2));
		asm volatile("mov %%cr3, %%rax\n\t mov %%rax, %0" : "=m" (from->state.cr3));
		asm volatile("mov %%cr4, %%rax\n\t mov %%rax, %0" : "=m" (from->state.cr4));
		asm volatile("mov %%dr0, %%rax\n\t mov %%rax, %0" : "=m" (from->state.dr0));
		asm volatile("mov %%dr1, %%rax\n\t mov %%rax, %0" : "=m" (from->state.dr1));
		asm volatile("mov %%dr2, %%rax\n\t mov %%rax, %0" : "=m" (from->state.dr2));
		asm volatile("mov %%dr3, %%rax\n\t mov %%rax, %0" : "=m" (from->state.dr3));
		asm volatile("mov %%dr6, %%rax\n\t mov %%rax, %0" : "=m" (from->state.dr6));
		asm volatile("mov %%dr7, %%rax\n\t mov %%rax, %0" : "=m" (from->state.dr7));
		asm volatile("fxsave %0" : "=m" (from->state.fx));
	}
	if(to){
		asm volatile("mov %0, %%rax\n\t mov %%rax, (%%rsp)" :: "m" (to->state.rip));
		asm volatile("mov %0, %%rax\n\t"
					 "push %%rax\n\t"
					 "popf\n\t" :: "m" (to->state.rflags) : "cc");
		asm volatile("mov %0, %%rax\n\t mov %%rax, %%cr0" :: "m" (to->state.cr0));
		asm volatile("mov %0, %%rax\n\t mov %%rax, %%cr2" :: "m" (to->state.cr2));
		asm volatile("mov %0, %%rax\n\t mov %%rax, %%cr3" :: "m" (to->state.cr3));
		asm volatile("mov %0, %%rax\n\t mov %%rax, %%cr4" :: "m" (to->state.cr4));
		asm volatile("mov %0, %%rax\n\t mov %%rax, %%dr0" :: "m" (to->state.dr0));
		asm volatile("mov %0, %%rax\n\t mov %%rax, %%dr1" :: "m" (to->state.dr1));
		asm volatile("mov %0, %%rax\n\t mov %%rax, %%dr2" :: "m" (to->state.dr2));
		asm volatile("mov %0, %%rax\n\t mov %%rax, %%dr3" :: "m" (to->state.dr3));
		asm volatile("mov %0, %%rax\n\t mov %%rax, %%dr6" :: "m" (to->state.dr6));
		asm volatile("mov %0, %%rax\n\t mov %%rax, %%dr7" :: "m" (to->state.dr7));
		asm volatile("mov %0, %%rax" :: "m" (to->state.rax) : "rax");
		asm volatile("mov %0, %%rbx" :: "m" (to->state.rbx) : "rbx");
		asm volatile("mov %0, %%rcx" :: "m" (to->state.rcx) : "rcx");
		asm volatile("mov %0, %%rdx" :: "m" (to->state.rdx) : "rdx");
		asm volatile("mov %0, %%rsi" :: "m" (to->state.rsi) : "rsi");
		asm volatile("mov %0, %%rdi" :: "m" (to->state.rdi) : "rdi");
		asm volatile("mov %0, %%rsp" :: "m" (to->state.rsp) : "rsp"); // !
		asm volatile("mov %0, %%rbp" :: "m" (to->state.rbp) : "rbp");
		asm volatile("mov %0, %%r8" :: "m" (to->state.r8) : "r8");
		asm volatile("mov %0, %%r9" :: "m" (to->state.r9) : "r9");
		asm volatile("mov %0, %%r10" :: "m" (to->state.r10) : "r10");
		asm volatile("mov %0, %%r11" :: "m" (to->state.r11) : "r11");
		asm volatile("mov %0, %%r12" :: "m" (to->state.r12) : "r12");
		asm volatile("mov %0, %%r13" :: "m" (to->state.r13) : "r13");
		asm volatile("mov %0, %%r14" :: "m" (to->state.r14) : "r14");
		asm volatile("mov %0, %%r15" :: "m" (to->state.r15) : "r15");
		asm volatile("fxrstor %0" :: "m" (to->state.fx));

		asm volatile("ret");
	}
}
