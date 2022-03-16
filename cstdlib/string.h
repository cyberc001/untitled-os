#ifndef _STRING_H
#define _STRING_H

#include <stddef.h>

// memory functions
void* memcpy(void* restrict dest, const void* restrict src, size_t n);
void* memmove(void* dest, const void* src, size_t n);

void* memset(void* dest, int c, size_t n);

int memcmp(const void* s1, const void* s2, size_t n);

// string functions
size_t strlen(const char* s);

char* strcpy(char* restrict dest, const char* restrict src);
char* strncpy(char* restrict dest, const char* restrict src, size_t n);

char* strcat(char* restrict dest, const char* restrict src);
char* strncat(char* restrict dest, const char* restrict src, size_t n);

int strcmp(const char* restrict s1, const char* restrict s2);
int strncmp(const char* restrict s1, const char* restrict s2, size_t n);

const char* strchr(const char* restrict s, char c);

#endif
