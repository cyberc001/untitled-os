// multiboot header constants
.set ALIGN,	1 << 0 		// align loaded modules on page boundaries
.set MEMINFO,	1 << 1		// provide memory map
.set FLAGS,	ALIGN | MEMINFO
.set MAGIC,	0x1BADB002	// magic number for bootloader header
.set CHECKSUM,	-(MAGIC + FLAGS)// checksum of the above variables


// multiboot header
.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

// stack initialization
.section .bss
.align 16
stack_bottom:
.skip 16384 // 16 KiB
stack_top:

// kernel entry point
.section .text
.global _start
.type _start, @function

_start:
	mov $stack_top, %esp 	// stack init
	call kernel_main	// call main kernel module (.c)

	cli			// disable interrupts
l:	hlt			// halt CPU forever
	jmp l


.size _start, . - _start // size of _start symbol
