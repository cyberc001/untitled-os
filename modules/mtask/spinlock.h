#ifndef SPINLOCK_H
#define SPINLOCK_H

typedef int spinlock;

inline void spinlock_init(spinlock* s)
{
	*s = 0;
}
inline void spinlock_lock(spinlock* s)
{
	unsigned char set = 0;
	while(!set){
		asm volatile ("lock cmpxchgl %2, %1\n"
					  "sete %0\n"
					: "=q"(set), "=m" (*s)
					: "r" (1), "m"(*s), "a"(0)
					: "memory");
	}
}
inline void spinlock_unlock(spinlock* s)
{
	*s = 0;
}

#endif
