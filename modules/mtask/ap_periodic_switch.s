global _ts_scheduler_advance_thread_queue
_ts_scheduler_advance_thread_queue:
	dq 0x0
global _ts_scheduler_prev_threads
_ts_scheduler_prev_threads:
	dq 0x0
global _ts_scheduler_switch_enable_flag
_ts_scheduler_switch_enable_flag:
	dq 0x0

global ap_periodic_switch
ap_periodic_switch:
	cli
	pushfq
	push rax
	push rbx
	; registers below are modified by scheduler_advance_queue() (they are caller-saved by C calling convention), and should be preserved when queue doesn't advance (one thread active for logical CPU).
	; note that only actually modified registers are saved (according to mtask.so disassembly) to shed CPU cycles.
	push rcx
	push rdx
	push rsi
	push rdi

	; signal the APIC controller so timer interrupts won't stop
	xor rax, rax
	xor rbx, rbx
	mov rbx, 0xFEE000B0
	mov [rbx], eax

	mov rax, _ts_scheduler_switch_enable_flag
	mov rax, [rax]
	test rax, rax
	jnz .sts_enabled

	pop rdi
	pop rsi
	pop rdx
	pop rcx
	pop rbx
	pop rax
	popfq
	sti
	iretq

	.sts_enabled:
	; get local APIC ID
	xor rax, rax
	xor rbx, rbx
	mov rbx, 0xFEE00020
	mov eax, [rbx]
	shr rax, 24

	; save context to the previous thread state
	mov rbx, _ts_scheduler_prev_threads
	mov rbx, [rbx]
	mov rbx, [rbx+rax*8]
	test rbx, rbx
	jz .end_ctx_save

	mov rax, [rsp+40]	; get rax from stack
	mov [rbx], rax
	mov rax, [rsp+32]	; get rbx from stack
	mov [rbx+8], rax	; rbx
	add rsp, 56			; restore rsp pointer to what it was before interrupt (7 registers saved on stack * 8 bytes)
	mov [rbx+16], rcx
	mov [rbx+24], rdx
	mov [rbx+32], rsi
	mov [rbx+40], rdi
	; RSP
	mov rax, [rsp+24]
	mov [rbx+48], rax
	; RSP
	mov [rbx+56], rbp
	mov [rbx+64], r8
	mov [rbx+72], r9
	mov [rbx+80], r10
	mov [rbx+88], r11
	mov [rbx+96], r12
	mov [rbx+104], r13
	mov [rbx+112], r14
	mov [rbx+120], r15
	; RIP
	mov rax, [rsp]
	mov [rbx+128], rax
	; RIP
	; RFLAGS
	pushfq
	mov rax, [rsp]
	popfq
	mov [rbx+136], rax
	; RFLAGS
	mov rax, cr0
	mov [rbx+144], rax
	mov rax, cr2
	mov [rbx+152], rax
	mov rax, cr3
	mov [rbx+160], rax
	mov rax, cr4
	mov [rbx+168], rax
	mov rax, dr0
	mov [rbx+176], rax
	mov rax, dr1
	mov [rbx+184], rax
	mov rax, dr2
	mov [rbx+192], rax
	mov rax, dr3
	mov [rbx+200], rax
	mov rax, dr6
	mov [rbx+208], rax
	mov rax, dr7
	mov [rbx+216], rax
	fxsave [rbx+224]
	sub rsp, 56
	.end_ctx_save:

	mov rax, _ts_scheduler_advance_thread_queue
	mov rax, [rax]		; get function pointer
	call rax			; try to switch to a new thread

	test rax, rax		; test if switch actually was performed
	jz .end_switch		; if it wasn't (for example, there are 0 threads), then just return
	push rax
	call load_context 	; load context from a new thread
	add rsp, 8

	.end_switch:
	pop rdi
	pop rsi
	pop rdx
	pop rcx
	pop rbx
	pop rax
	popfq
	sti
	iretq

global load_context
load_context:
	mov rax, [rsp+8]

	; General-purpose registers (except RAX and RBX)
	mov rcx, [rax+16]
	mov rdx, [rax+24]
	mov rsi, [rax+32]
	mov rdi, [rax+40]
	mov r8, [rax+64]
	mov r9, [rax+72]
	mov r10, [rax+80]
	mov r11, [rax+88]
	mov r12, [rax+96]
	mov r13, [rax+104]
	mov r14, [rax+112]
	mov r15, [rax+120]
	; RFLAGS
	mov rbx, [rax+136]
	and rbx, 0xFFFFFFFFFFFFFDFF
	push rbx
	popfq
	; Control registers
	mov rbx, [rax+144]
	mov cr0, rbx
	mov rbx, [rax+152]
	mov cr2, rbx
	mov rbx, [rax+160]
	mov cr3, rbx
	mov rbx, [rax+168]
	mov cr4, rbx
	; Debug registers
	mov rbx, [rax+176]
	mov dr0, rbx
	mov rbx, [rax+184]
	mov dr1, rbx
	mov rbx, [rax+192]
	mov dr2, rbx
	mov rbx, [rax+200]
	mov dr3, rbx
	mov rbx, [rax+208]
	mov dr6, rbx
	mov rbx, [rax+216]
	mov dr7, rbx
	; x87 FPU, MMX Technology, and SSE State
	fxrstor [rax+224]
	; Stack registers
	mov rsp, [rax+48]
	mov rbp, [rax+56]
	; RIP
	mov rbx, [rax+128]
	push rbx
	; RAX and RBX
	mov rbx, [rax+8]
	mov rax, [rax]
	sti
	ret
