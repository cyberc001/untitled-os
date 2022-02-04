.extern isr_exception_stub_func

.macro isr_stub num
isr_stub_\num :
	mov $\num, %rdi
	call isr_exception_stub_func
	iretq
.endm
.macro isr_stub_noerr num
isr_stub_\num :
	pushq $\num
	call isr_exception_stub_func_noerr
	add $8, %rsp
	iretq
.endm

// CPU exceptions and traps

isr_stub 0
isr_stub 1
isr_stub 2
isr_stub 3
isr_stub 4
isr_stub 5
isr_stub 6
isr_stub 7
isr_stub 8
isr_stub 9
isr_stub 10
isr_stub 11
isr_stub 12
isr_stub 13
isr_stub 14
isr_stub 15
isr_stub 16
isr_stub 17
isr_stub 18
isr_stub 19
isr_stub 20
isr_stub 21
isr_stub 22
isr_stub 23
isr_stub 24
isr_stub 25
isr_stub 26
isr_stub 27
isr_stub 28
isr_stub 29
isr_stub 30
isr_stub 31


// PIC interrupts

isr_stub_noerr 32
isr_stub_noerr 33
isr_stub_noerr 34
isr_stub_noerr 35
isr_stub_noerr 36
isr_stub_noerr 37
isr_stub_noerr 38
isr_stub_noerr 39
isr_stub_noerr 40
isr_stub_noerr 41
isr_stub_noerr 42
isr_stub_noerr 43
isr_stub_noerr 44
isr_stub_noerr 45
isr_stub_noerr 46
isr_stub_noerr 47

.global isr_stub_table
isr_stub_table:
.irp num 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47
	.quad isr_stub_\num
.endr
