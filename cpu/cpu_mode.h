#ifndef CPU_MODE_H
#define CPU_MODE_H

// CPU mode manipulation

#define CPU_MODE_REAL		0
#define CPU_MODE_PROTECTED	1

#ifdef CPU_I386

	#ifdef CPU_32BIT
	static inline void cpu_mode_set(int mode)
	{
		switch(mode)
		{
			case CPU_MODE_REAL: // clear PE (protection enable) bit in control register 0
				asm volatile ("movl %cr0, %eax\n"
					      "andb %al, $0xFE\n"
					      "movl %eax, %cr0");
				break;
			case CPU_MODE_PROTECTED: // set PE (protection enable) bit in control register 0
				asm volatile ("movl %cr0, %eax\n"
					      "orb %al, 1\n"
					      "movl %eax, %cr0");
				break;
		}
	}
	#elif defined CPU_64BIT
	static inline void cpu_mode_set(int mode)
	{
		switch(mode)
		{
			case CPU_MODE_REAL: // clear PE (protection enable) bit in control register 0
				asm volatile ("movq %cr0, %rax\n"
					      "andb %al, $0xFE\n"
					      "movq %rax, %cr0");
				break;
			case CPU_MODE_PROTECTED: // set PE (protection enable) bit in control register 0
				asm volatile ("movq %cr0, %rax\n"
					      "orb %al, 1\n"
					      "movq %rax, %cr0");
				break;
		}
	}
	#else
		#error manipulating CPU modes is not supported for this platform
	#endif
#endif

#endif
