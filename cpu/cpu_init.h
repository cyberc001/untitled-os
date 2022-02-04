#ifndef CPU_INIT_H
#define CPU_INIT_H

// Generic init function, underlying actions are dependant on the platform.
// For example, x86_64 initializes a GDT.
void cpu_init();

#endif
