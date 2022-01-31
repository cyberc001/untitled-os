#ifndef ISR_H
#define ISR_H

#include <stdint.h>
#include <stdnoreturn.h>

noreturn void isr_exception_stub_func(long isr_num);
void isr_exception_stub_func_noerr(long isr_num);

#define ISR_STUB_TABLE_SIZE 47
extern uint32_t isr_stub_table[ISR_STUB_TABLE_SIZE];

#endif
