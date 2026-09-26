#ifndef ZTEST_PS1_STRING_H
#define ZTEST_PS1_STRING_H
#include <stddef.h>
void* memcpy(void* dst, const void* src, size_t n);
void* memmove(void* dst, const void* src, size_t n);
void* memset(void* dst, int c, size_t n);
int memcmp(const void* a, const void* b, size_t n);
size_t strlen(const char* s);
int strcmp(const char* a, const char* b);
int strncmp(const char* a, const char* b, size_t n);
char* strchr(const char* s, int c);
size_t strcspn(const char* s, const char* reject);
char* strcpy(char* dst, const char* src);
char* strcat(char* dst, const char* src);
#endif
