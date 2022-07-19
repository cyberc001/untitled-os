#include "pit.h"

void pit_sleep_ms(uint64_t ms)
{
	uint64_t total_count = 0x4A9 * ms;
	do {
		uint16_t count = total_count > 0xFFFFU ? 0xFFFFU : total_count;
		cpu_out8(0x43, 0x30);
		cpu_out8(0x40, count & 0xFF);
		cpu_out8(0x40, count >> 8);
		do {
			__builtin_ia32_pause();
			cpu_out8(0x43, 0xE2);
		} while ((cpu_in8(0x40) & (1 << 7)) == 0);
		total_count -= count;
	} while ((total_count & ~0xFFFF) != 0);
}
