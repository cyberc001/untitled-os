#ifndef _BITS_H
#define _BITS_H

#define GET_BITS_LAST(expr, amt)		((uint64_t)(expr) & (((uint64_t)1 << (amt)) - 1))
/* gets bits in range [start, end) */
#define GET_BITS(expr, start, end)		GET_BITS_LAST((uint64_t)(expr) >> (start), (end) - (start))

#define SET_BITS(expr, bits, start)		{ (expr) = (__typeof__(expr))((uint64_t)(expr) | (bits) << (start)); }

#endif
