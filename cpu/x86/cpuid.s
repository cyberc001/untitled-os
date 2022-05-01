global cpuid_check
cpuid_check:
	pushfq
	pushfq
	xor dword [rsp], 0x200000		; invert ID in RFLAGS
	popfq
	pushfq
	pop rax							; rax now contains modified RFLAGS
	xor rax, [rsp]					; xor will leave out only changed bits
	popfq
	and rax, 0x200000				; mask out only changed ID bit
	ret

global cpuid
cpuid:
	push rbx

	; rdi - eax_in, rsi - ecx_in, r10 - eax_out, r11 - ebx_out, r8 - ecx_out, r9 - edx_out
	mov r10, rdx
	mov r11, rcx

	; first check the leaf supplied in eax is within valid range
	mov rax, 0x0
	mov rcx, 0x0
	cpuid
	cmp rax, rdi	; compare maximum CPUID value returned by CPUID with supplied leaf number
	jb .invalid_leaf
	jmp .call_cpuid

	.invalid_leaf:
	pop rbx
	mov rax, 0x0
	ret

	.call_cpuid:
	; finally call cpuid
	mov rax, rdi
	mov rcx, rsi
	cpuid

	mov [r10], eax
	mov [r11], ebx
	mov [r8], ecx
	mov [r9], edx

	pop rbx
	mov rax, 0x1
	ret
