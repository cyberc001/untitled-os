#ifndef X86_PIT_H
#define X86_PIT_H

#include <stdint.h>
#include "cpu/cpu_io.h"

void pit_sleep_ms(uint64_t ms);

#endif
