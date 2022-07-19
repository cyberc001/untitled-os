global save_context
save_context:
	push rax
	mov rax, [rsp+16]

	; General-purpose registers (except RAX)
	mov [rax+8], rbx
	mov [rax+16], rcx
	mov [rax+24], rdx
	mov [rax+32], rsi
	mov [rax+40], rdi
	mov [rax+48], rsp
	sub qword [rax+48], 24
	mov [rax+56], rbp
	mov [rax+64], r8
	mov [rax+72], r9
	mov [rax+80], r10
	mov [rax+88], r11
	mov [rax+96], r12
	mov [rax+104], r13
	mov [rax+112], r14
	mov [rax+120], r15
	; RIP
	mov rbx, [rsp+8]
	mov [rax+128], rbx
	; RFLAGS
	pushfq
	mov rbx, [rsp]
	popfq
	mov [rax+136], rbx
	; Control registers
	mov rbx, cr0
	mov [rax+144], rbx
	mov rbx, cr2
	mov [rax+152], rbx
	mov rbx, cr3
	mov [rax+160], rbx
	mov rbx, cr4
	mov [rax+168], rbx
	; Debug registers
	mov rbx, dr0
	mov [rax+176], rbx
	mov rbx, dr1
	mov [rax+184], rbx
	mov rbx, dr2
	mov [rax+192], rbx
	mov rbx, dr3
	mov [rax+200], rbx
	mov rbx, dr6
	mov [rax+208], rbx
	mov rbx, dr7
	mov [rax+216], rbx
	; x87 FPU, MMX Technology, and SSE State
	fxsave [rax+224]
	; RAX
	mov rdi, rax
	pop rax
	mov [rdi], rax
	ret

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
	; RFLAGS !!
	mov rbx, [rax+136]
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
	ret
