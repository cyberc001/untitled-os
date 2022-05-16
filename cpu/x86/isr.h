#ifndef ISR_H
#define ISR_H

#include <stdint.h>
#include <stdnoreturn.h>

#include "cpu/x86/idt.h"

noreturn void isr_exception_stub_func(uint64_t isr_num);
void isr_exception_stub_func_noerr(uint64_t isr_num);

#define ISR_STUB_TABLE_SIZE 47
extern uintptr_t isr_stub_table[ISR_STUB_TABLE_SIZE];

extern uintptr_t isr_func_caller_table[IDT_VECTOR_SIZE - ISR_STUB_TABLE_SIZE];
extern uintptr_t isr_int_funcs[IDT_VECTOR_SIZE - ISR_STUB_TABLE_SIZE];

#endif
