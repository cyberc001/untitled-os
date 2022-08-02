%define CONV_ADDR	0x9000	; address in conventional memory (that fits in 16 bits)
%define DATA_OFF	0x0A00	; offset for data passed by BSP

%define abs_data_off (CONV_ADDR + DATA_OFF + ($ - data_start))
%define abs_label_off(label) (CONV_ADDR + ((label) - smp_trampoline_start))

align 0x1000
global smp_trampoline_start
smp_trampoline_start equ $

bits 16
tramp16:
	cli
	lidt [tmp_idt_ptr]
	jmp 0x0:abs_label_off(.set_cs)
	.set_cs:
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	cld

	lgdt [tmp_gdt_ptr]

	; enable PAE
	mov eax, cr4
	or eax, 0xA0
	mov cr4, eax

	; jump to long mode
	mov eax, [page_table]
	mov cr3, eax

	mov ecx, 0xC0000080		; EFER
	rdmsr
	or eax, 0x900			; long mode, NX
	wrmsr

	; Enable: paging, pmode, write protect
	mov eax, 0x80400001
	mov cr0, eax

	jmp 0x08:abs_label_off(tramp64) ; change code selector to 64-bit kernel code

bits 64
tramp64:
	; set up temporary stack for reloading segment selectors
	mov rsp, tmp_stack_end
	mov rbp, tmp_stack_end

	; Finally load proper IDT and GDT
	lidt [idt_ptr]

	lgdt [gdt_ptr]
	push 0x28
	lea rax, [abs_label_off(.set_cs64)]
	push rax
	retfq
	.set_cs64:
	mov ax, 0x30
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	cld

	; set up APIC timer for task switching
	call [ap_timer_set_func]

	; tell BSP that this AP has booted successfully
	mov al, 0x1
	lock xchg byte [boot_flag], al ; atomically write 1

	; start the endless loop waiting for jump location
	xor rax, rax
	mov rbx, [jmp_loc]
	.wait_for_jmp_loc:
	lock xadd [rbx], rax
	test rax, rax
	jz .wait_for_jmp_loc

	jmp [rbx]

	.halt:
	hlt
	jmp .halt


times DATA_OFF - ($ - smp_trampoline_start) db 0xAC ; pad the gap between code and data

data_start:
; data passed from BSP (smp_trampoline.h)
idt_ptr equ abs_data_off
	dw 0
	dq 0
gdt_ptr equ abs_data_off
	dw 0
	dq 0
page_table equ abs_data_off
	dd 0
boot_flag equ abs_data_off
	db 0
ap_timer_set_func equ abs_data_off
	dq 0
jmp_loc equ abs_data_off
	dq 0

; temporary data for real mode
tmp_gdt equ abs_data_off
	dq 0
	; 64-bit code (kernel)
	dw 0xFFFF
	dw 0
	db 0
	db 10011010b
	db 10101111b
	db 0
	; 64-bit data (kernel)
	dw 0xFFFF
	dw 0
	db 0
	db 10010010b
	db 10101111b
	db 0
tmp_gdt_end equ abs_data_off

align 16
tmp_gdt_ptr equ abs_data_off
	dw (tmp_gdt_end - tmp_gdt - 1)
	dq tmp_gdt
tmp_idt_ptr equ abs_data_off
	dw 0
	dq 0

; temporary stack
tmp_stack equ abs_data_off
		%rep 64
		db 0
		%endrep
tmp_stack_end equ abs_data_off

global smp_trampoline_end
smp_trampoline_end equ $
