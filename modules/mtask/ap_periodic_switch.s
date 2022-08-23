global _ts_scheduler_advance_thread_queue
_ts_scheduler_advance_thread_queue:
	dq 0x0

global ap_periodic_switch
ap_periodic_switch:
	cli
	pushfq
	push rax
	push rbx
	push rcx
	push rsi
	push rdi
	push r8
	push r9
	push r10
	push r11
	push r12
	push r13
	push r14

	xor rax, rax
	xor rbx, rbx
	mov rbx, 0xFEE000B0
	mov [rbx], eax

	mov rax, _ts_scheduler_advance_thread_queue
	mov rax, [rax]
	test rax, rax
	jz .skip_call1
	call rax
	.skip_call1:

	pop r14
	pop r13
	pop r12
	pop r11
	pop r10
	pop r9
	pop r8
	pop rdi
	pop rsi
	pop rcx
	pop rbx
	pop rax
	popfq
	sti
	iretq
