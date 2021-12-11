.global init_gdt_flat

#if CPU_32BIT

gdt_flat:
gdt_null:
	.quad 0
gdt_code:
	.word 0xFFFF
	.word 0
	.byte 0
	.byte 0b10011010
	.byte 0b11001111
	.byte 0
gdt_data:
	.word 0xFFFF
	.word 0
	.byte 0
	.byte 0b10010010
	.byte 0b11001111
	.byte 0
gdt_flat_end:

gdt_flat_desc:
	.word gdt_flat_end - gdt_flat
	.long gdt_flat

init_gdt_flat:

	xor %eax, %eax		// load GDT
	mov %ds, %ax
	lgdt (gdt_flat_desc)

	mov $0x10, %eax		// reload section registers (with data segment)
	mov %eax, %ds
	mov %eax, %es
	mov %eax, %fs
	mov %eax, %gs
	mov %eax, %ss

	jmp $0x8, $.clear_pipe	// reload code section register (with code segment)
	.clear_pipe:
	ret

#endif
