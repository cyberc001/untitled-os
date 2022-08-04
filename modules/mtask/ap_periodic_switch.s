global ap_periodic_switch

ap_periodic_switch:
	push rax
	push rbx

	xor rax, rax
	mov rbx, 0xFEE000B0
	mov [rbx], eax

	pop rbx
	pop rax
	ret
