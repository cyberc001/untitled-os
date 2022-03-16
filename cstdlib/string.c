#include "string.h"

// memory functions
void* memcpy(void* restrict dest, const void* restrict src, size_t n)
{
	// assume that size_t is of machine word size
	void* destret = dest;
	size_t* _dest = dest; const size_t* _src = src;

	const size_t* _src_end1 = _src + n / sizeof(size_t);
	for(; _src != _src_end1; ++_src, ++_dest)
		*_dest = *_src;

	const void* src_end = ((void*)_src_end1) + n % sizeof(size_t);
	for(dest = _dest, src = _src; src != src_end; ++src, ++dest)
		*((char*)dest) = *((char*)src);

	return destret;
}
void* memmove(void* restrict dest, const void* restrict src, size_t n)
{
	return memcpy(dest, src, n); // TODO: implement memmove using a dynamically allocated buffer
}

void* memset(void* dest, int c, size_t n)
{
	void* destret = dest;
	for(void* dest_end = dest + n; dest != dest_end; ++dest)
		*((char*)dest) = (char)c;
	return destret;
}

int memcmp(const void* s1, const void* s2, size_t n)
{
	for(const void* s1_end = s1 + n; s1 != s1_end; ++s1, ++s2)
		if( *((char*)s1) < *((char*)s2) ) return -1;
		else if( *((char*)s1) > *((char*)s2) ) return 1;
	return 0;
}

// string functions
size_t strlen(const char* s)
{
	const char* sbeg = s;
	for(; *s != '\0'; ++s) ;
	return s - sbeg;
}

char* strcpy(char* restrict dest, const char* restrict src)
{
	memcpy(dest, src, strlen(src)+1);
	return dest;
}
char* strncpy(char* restrict dest, const char* restrict src, size_t n)
{
	memcpy(dest, src, (strlen(src)+1) < n ? (strlen(src)+1) : n);
	return dest;
}

char* strcat(char* restrict dest, const char* restrict src)
{
	dest += strlen(dest);
	memcpy(dest, src, strlen(src)+1);
	return dest;
}
char* strncat(char* restrict dest, const char* restrict src, size_t n)
{
	dest += strlen(dest);
	if(n <= strlen(dest)+1) return dest;
	n -= strlen(dest)+1;
	memcpy(dest, src, (strlen(src)+1) < n ? (strlen(src)+1) : n);
	return dest;
}

int strcmp(const char* restrict s1, const char* restrict s2)
{
	for(; *s1 && *s2; ++s1, ++s2)
		if( *s1 < *s2 ) return -1;
		else if( *s1 > *s2 ) return 1;
	if( *s1 < *s2 ) return -1;
	else if( *s1 > *s2 ) return 1;
	return 0;
}
int strncmp(const char* restrict s1, const char* restrict s2, size_t n)
{
	// TODO
	return memcmp(s1, s2, strlen(s1)+1 < n ? strlen(s1)+1 : n);
}

const char* strchr(const char* restrict s, char c)
{
	for(; *s; ++s)
		if(*s == c)
			return s;
	return NULL;
}
