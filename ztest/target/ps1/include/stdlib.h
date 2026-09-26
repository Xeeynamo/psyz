#ifndef ZTEST_PS1_STDLIB_H
#define ZTEST_PS1_STDLIB_H
#include <stddef.h>
void* malloc(size_t size);
void* realloc(void* ptr, size_t size);
void free(void* ptr);
int abs(int v);
long strtol(const char* s, char** end, int base);
#endif
