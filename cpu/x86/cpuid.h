#ifndef X86_CPUID
#define X86_CPUID

#include <stdint.h>

#define CPUID_FEAT_EDX_APIC		(1 << 9)

int cpuid_check();
/* Return value: 0 if eax_in (leaf) is out of valid range, 1 otherwise */
int cpuid(uint64_t eax_in, uint64_t ecx_in,
			uint32_t* eax_out, uint32_t* ebx_out, uint32_t* ecx_out, uint32_t* edx_out);

#endif
