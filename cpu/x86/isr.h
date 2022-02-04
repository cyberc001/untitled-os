#ifndef ISR_H
#define ISR_H

#include <stdint.h>
#include <stdnoreturn.h>

noreturn void isr_exception_stub_func(uint64_t isr_num);
void isr_exception_stub_func_noerr(uint64_t isr_num);

#define ISR_STUB_TABLE_SIZE 47

#ifdef CPU_32BIT
extern uint32_t isr_stub_table[ISR_STUB_TABLE_SIZE];
#elif defined CPU_64BIT
extern uint64_t isr_stub_table[ISR_STUB_TABLE_SIZE];
#endif

#endif
