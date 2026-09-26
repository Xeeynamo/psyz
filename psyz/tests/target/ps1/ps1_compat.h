#ifndef PSYZ_TESTS_PS1_COMPAT_H
#define PSYZ_TESTS_PS1_COMPAT_H

#define _STDARG_H
#define va_list __builtin_va_list
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_end(ap) __builtin_va_end(ap)
#define va_copy(dst, src) __builtin_va_copy(dst, src)

#define snprintf ztest_snprintf
#define vsnprintf ztest_vsnprintf

#define _STRINGS_H
#define _STRING_H
#define _MEMORY_H
#include <stddef.h>
void* memcpy(void* dst, const void* src, size_t n);
void* memset(void* dst, int c, size_t n);
void* memmove(void* dst, const void* src, size_t n);
int memcmp(const void* a, const void* b, size_t n);
size_t strlen(const char* s);
int strcmp(const char* a, const char* b);
int strncmp(const char* a, const char* b, size_t n);
char* strcpy(char* dst, const char* src);
char* strncpy(char* dst, const char* src, size_t n);
char* strcat(char* dst, const char* src);
char* strchr(const char* s, int c);
char* strrchr(const char* s, int c);
char* strstr(const char* haystack, const char* needle);
size_t strcspn(const char* s, const char* reject);

static inline char* getenv(const char* name) {
    (void)name;
    return NULL;
}

typedef struct ps1_file FILE;
static inline FILE* fopen(const char* path, const char* mode) {
    (void)path;
    (void)mode;
    return NULL;
}
static inline size_t fread(void* p, size_t size, size_t n, FILE* f) {
    (void)p, (void)size, (void)n, (void)f;
    return 0;
}
static inline size_t fwrite(const void* p, size_t size, size_t n, FILE* f) {
    (void)p, (void)size, (void)n, (void)f;
    return 0;
}
static inline int fclose(FILE* f) {
    (void)f;
    return -1;
}
static inline int fseek(FILE* f, long offset, int whence) {
    (void)f, (void)offset, (void)whence;
    return -1;
}
static inline long ftell(FILE* f) {
    (void)f;
    return -1;
}
#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif
static inline int remove(const char* path) {
    (void)path;
    return -1;
}

#endif
