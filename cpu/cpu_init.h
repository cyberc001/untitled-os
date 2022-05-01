#ifndef CPU_INIT_H
#define CPU_INIT_H

#define CPU_INITERR_NOCPUID		-1
#define CPU_INITERR_NOAPIC		-2

// Generic init function, underlying actions are dependant on the platform.
// For example, x86_64 initializes a flag GDT, remaps IRQs, checks for CPUID presence.
int cpu_init();

#endif
