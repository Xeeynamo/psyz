#ifndef ZTEST_H
#define ZTEST_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ZTEST_TARGET
#if defined(__PSP__)
#define ZTEST_TARGET "psp"
#elif defined(__psx__) || defined(__PSX__)
#define ZTEST_TARGET "ps1"
#elif defined(__NDS__)
#define ZTEST_TARGET "nds"
#elif defined(N64) || defined(__N64__)
#define ZTEST_TARGET "n64"
#elif defined(__ANDROID__)
#define ZTEST_TARGET "android"
#elif defined(_WIN32)
#define ZTEST_TARGET "windows"
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#define ZTEST_TARGET "ios"
#else
#define ZTEST_TARGET "macos"
#endif
#elif defined(__linux__)
#define ZTEST_TARGET "linux"
#else
#define ZTEST_TARGET "unknown"
#endif
#endif

// Semicolon-separated extra tags matched by zskip_targets and ztest_is_target.
#ifndef ZTEST_TAGS
#define ZTEST_TAGS ""
#endif

typedef struct ztest_case {
    const char* group;
    const char* name;
    void (*fn)(void);
    const char* file;
    int line;
    int result;
    struct ztest_case* next;
} ztest_case;

typedef struct ztest_hook {
    const char* group;
    void (*fn)(void);
    int kind;
    struct ztest_hook* next;
} ztest_hook;

void ztest__register(ztest_case* c);
void ztest__register_hook(ztest_hook* h);

#if defined(_MSC_VER)
// MSVC expands macros inside #pragma section, and a project may #define read
#pragma push_macro("read")
#undef read
#pragma section(".CRT$XCU", read)
#pragma pop_macro("read")
#if defined(_M_IX86)
#define ZTEST__SYM_PREFIX "_"
#else
#define ZTEST__SYM_PREFIX ""
#endif
#define ZTEST__CTOR(f)                                                         \
    static void __cdecl f(void);                                               \
    __pragma(comment(linker, "/include:" ZTEST__SYM_PREFIX #f "_"))            \
        __declspec(allocate(".CRT$XCU")) void(__cdecl * f##_)(void) = f;       \
    static void __cdecl f(void)
#else
#define ZTEST__CTOR(f)                                                         \
    static void f(void) __attribute__((constructor));                          \
    static void f(void)
#endif

// Usage: ZTEST(group, name) { ... }
#define ZTEST(group, name)                                                     \
    static void ztest__fn_##group##__##name(void);                             \
    static ztest_case ztest__case_##group##__##name = {                        \
        #group, #name, ztest__fn_##group##__##name, __FILE__, __LINE__, 0, 0}; \
    ZTEST__CTOR(ztest__reg_##group##__##name) {                                \
        ztest__register(&ztest__case_##group##__##name);                       \
    }                                                                          \
    static void ztest__fn_##group##__##name(void)

#define ZTEST__HOOK(group, kind, id)                                           \
    static void ztest__##kind##_##group(void);                                 \
    static ztest_hook ztest__##kind##_node_##group = {                         \
        #group, ztest__##kind##_##group, id, 0};                               \
    ZTEST__CTOR(ztest__##kind##_reg_##group) {                                 \
        ztest__register_hook(&ztest__##kind##_node_##group);                   \
    }                                                                          \
    static void ztest__##kind##_##group(void)

// Usage: ZTEST_SETUP(group) { ... }, runs before every test of the group.
#define ZTEST_SETUP(group) ZTEST__HOOK(group, setup, 0)
// Usage: ZTEST_TEARDOWN(group) { ... }, runs after every test of the group,
// even after zassert or zskip.
#define ZTEST_TEARDOWN(group) ZTEST__HOOK(group, teardown, 1)

#ifdef N64
// libdragon calls main without arguments.
#define ZTEST_MAIN                                                             \
    int main(void) { return ztest_main(0, NULL); }
#else
#define ZTEST_MAIN                                                             \
    int main(int argc, char** argv) { return ztest_main(argc, argv); }
#endif

int ztest_main(int argc, char** argv);

const char* ztest_target(void);
void ztest_add_tag(const char* tag);
int ztest_is_target(const char* list);
void ztest_set_read_root(const char* root);
void ztest_set_write_root(const char* root);

void zprintf(const char* fmt, ...);
void zerrorf(const char* fmt, ...);
int ztest_snprintf(char* buf, size_t n, const char* fmt, ...);
int ztest_vsnprintf(char* buf, size_t n, const char* fmt, va_list ap);

// The message is optional and must be a string literal when present.
#define zfail(...) ztest__fail(__LINE__, 0, "" __VA_ARGS__)
#define zabort(...) ztest__fail(__LINE__, 1, "" __VA_ARGS__)
#define zskip(...) ztest__skip(__LINE__, "" __VA_ARGS__)
#define zterminate(...) ztest__terminate(__LINE__, "" __VA_ARGS__)
#define zskip_targets(list) ztest__skip_targets(__LINE__, (list))

void ztest__fail(int line, int abort, const char* fmt, ...);
void ztest__skip(int line, const char* fmt, ...);
void ztest__terminate(int line, const char* fmt, ...);
void ztest__skip_targets(int line, const char* list);

enum { ZT_EQ, ZT_NE, ZT_GT, ZT_GE, ZT_LT, ZT_LE };

int ztest__s(int line, int op, int abort, long exp, long act);
int ztest__u(int line, int op, int abort, unsigned long exp, unsigned long act);
int ztest__c(int line, int op, int abort, char exp, char act);
int ztest__str(int line, int op, int abort, const char* exp, const char* act);
int ztest__p(int line, int op, int abort, const void* exp, const void* act);
int ztest__up(int line, int op, int abort, uintptr_t exp, uintptr_t act);
int ztest__bytes(
    int line, int op, int abort, const void* exp, const void* act, size_t len);

typedef enum {
    ZIMAGE_NONE,
    ZIMAGE_RGB888,
    ZIMAGE_RGBA8888,
    ZIMAGE_R5G5B5,
    ZIMAGE_R5G6B5,
    ZIMAGE_RGBA5551_BE,
} zimage_format;

typedef struct zimage {
    int width;
    int height;
    zimage_format format;
    int stride;
    unsigned char* data;
} zimage;

typedef enum {
    ZIMAGE_MATCH,
    ZIMAGE_MISMATCH,
    ZIMAGE_SIZE_MISMATCH,
    ZIMAGE_OUT_OF_BOUNDS,
} zimage_status;

typedef struct zimage_result {
    zimage_status status;
    unsigned long matches;
    unsigned long total;
    int max_diff;
    int diff_x, diff_y;
    unsigned char exp_rgb[3];
    unsigned char act_rgb[3];
} zimage_result;

typedef struct zimage_cmp zimage_cmp;
struct zimage_cmp {
    void (*compare)(const zimage* exp, const zimage* act, const zimage_cmp* cmp,
                    zimage_result* res);
    int tolerance;
    float precision;
    int x, y, w, h;
};

void zimage_cmp_r5g5b5(const zimage* exp, const zimage* act,
                       const zimage_cmp* cmp, zimage_result* res);

#define zimage_r5g5b5_with_tol_prec(tol, prec)                                 \
    (&(const zimage_cmp){zimage_cmp_r5g5b5, (tol), (prec), 0, 0, 0, 0})
#define zimage_r5g5b5_exact zimage_r5g5b5_with_tol_prec(0, 1.0f)
#define zimage_r5g5b5_with_tolerance(tol) zimage_r5g5b5_with_tol_prec(tol, 1.0f)
#define zimage_r5g5b5_with_precision(prec) zimage_r5g5b5_with_tol_prec(0, prec)
#define zimage_r5g5b5_region(x, y, w, h, tol, prec)                            \
    (&(const zimage_cmp){zimage_cmp_r5g5b5, (tol), (prec), (x), (y), (w), (h)})

zimage zimage_alloc(int width, int height, zimage_format format);
void zimage_free(zimage* img);
zimage zimage_png(const char* path);
zimage zimage_exp_png(const char* name);
zimage zimage_frontbuffer(void);
void zimage_get_rgb(const zimage* img, int x, int y, unsigned char rgb[3]);
void zimage_set_rgb(zimage* img, int x, int y, const unsigned char rgb[3]);
#if defined(__psx__) || defined(__PSX__)
zimage zimage_vram(int x, int y, int w, int h);
void zimage_set_display_origin(int x, int y);
#endif

int ztest__image(int line, int op, int abort, const char* name, zimage act,
                 const zimage_cmp* cmp);

#define zassert_s32_eq(exp, act)                                               \
    ztest__s(__LINE__, ZT_EQ, 1, (long)(int32_t)(exp), (long)(int32_t)(act))
#define zexpect_s32_eq(exp, act)                                               \
    ztest__s(__LINE__, ZT_EQ, 0, (long)(int32_t)(exp), (long)(int32_t)(act))
#define zassert_s32_ne(exp, act)                                               \
    ztest__s(__LINE__, ZT_NE, 1, (long)(int32_t)(exp), (long)(int32_t)(act))
#define zexpect_s32_ne(exp, act)                                               \
    ztest__s(__LINE__, ZT_NE, 0, (long)(int32_t)(exp), (long)(int32_t)(act))
#define zassert_s32_gt(exp, act)                                               \
    ztest__s(__LINE__, ZT_GT, 1, (long)(int32_t)(exp), (long)(int32_t)(act))
#define zexpect_s32_gt(exp, act)                                               \
    ztest__s(__LINE__, ZT_GT, 0, (long)(int32_t)(exp), (long)(int32_t)(act))
#define zassert_s32_ge(exp, act)                                               \
    ztest__s(__LINE__, ZT_GE, 1, (long)(int32_t)(exp), (long)(int32_t)(act))
#define zexpect_s32_ge(exp, act)                                               \
    ztest__s(__LINE__, ZT_GE, 0, (long)(int32_t)(exp), (long)(int32_t)(act))
#define zassert_s32_lt(exp, act)                                               \
    ztest__s(__LINE__, ZT_LT, 1, (long)(int32_t)(exp), (long)(int32_t)(act))
#define zexpect_s32_lt(exp, act)                                               \
    ztest__s(__LINE__, ZT_LT, 0, (long)(int32_t)(exp), (long)(int32_t)(act))
#define zassert_s32_le(exp, act)                                               \
    ztest__s(__LINE__, ZT_LE, 1, (long)(int32_t)(exp), (long)(int32_t)(act))
#define zexpect_s32_le(exp, act)                                               \
    ztest__s(__LINE__, ZT_LE, 0, (long)(int32_t)(exp), (long)(int32_t)(act))

#define zassert_s16_eq(exp, act)                                               \
    ztest__s(__LINE__, ZT_EQ, 1, (long)(int16_t)(exp), (long)(int16_t)(act))
#define zexpect_s16_eq(exp, act)                                               \
    ztest__s(__LINE__, ZT_EQ, 0, (long)(int16_t)(exp), (long)(int16_t)(act))
#define zassert_s16_ne(exp, act)                                               \
    ztest__s(__LINE__, ZT_NE, 1, (long)(int16_t)(exp), (long)(int16_t)(act))
#define zexpect_s16_ne(exp, act)                                               \
    ztest__s(__LINE__, ZT_NE, 0, (long)(int16_t)(exp), (long)(int16_t)(act))
#define zassert_s16_gt(exp, act)                                               \
    ztest__s(__LINE__, ZT_GT, 1, (long)(int16_t)(exp), (long)(int16_t)(act))
#define zexpect_s16_gt(exp, act)                                               \
    ztest__s(__LINE__, ZT_GT, 0, (long)(int16_t)(exp), (long)(int16_t)(act))
#define zassert_s16_ge(exp, act)                                               \
    ztest__s(__LINE__, ZT_GE, 1, (long)(int16_t)(exp), (long)(int16_t)(act))
#define zexpect_s16_ge(exp, act)                                               \
    ztest__s(__LINE__, ZT_GE, 0, (long)(int16_t)(exp), (long)(int16_t)(act))
#define zassert_s16_lt(exp, act)                                               \
    ztest__s(__LINE__, ZT_LT, 1, (long)(int16_t)(exp), (long)(int16_t)(act))
#define zexpect_s16_lt(exp, act)                                               \
    ztest__s(__LINE__, ZT_LT, 0, (long)(int16_t)(exp), (long)(int16_t)(act))
#define zassert_s16_le(exp, act)                                               \
    ztest__s(__LINE__, ZT_LE, 1, (long)(int16_t)(exp), (long)(int16_t)(act))
#define zexpect_s16_le(exp, act)                                               \
    ztest__s(__LINE__, ZT_LE, 0, (long)(int16_t)(exp), (long)(int16_t)(act))

#define zassert_s8_eq(exp, act)                                                \
    ztest__s(__LINE__, ZT_EQ, 1, (long)(int8_t)(exp), (long)(int8_t)(act))
#define zexpect_s8_eq(exp, act)                                                \
    ztest__s(__LINE__, ZT_EQ, 0, (long)(int8_t)(exp), (long)(int8_t)(act))
#define zassert_s8_ne(exp, act)                                                \
    ztest__s(__LINE__, ZT_NE, 1, (long)(int8_t)(exp), (long)(int8_t)(act))
#define zexpect_s8_ne(exp, act)                                                \
    ztest__s(__LINE__, ZT_NE, 0, (long)(int8_t)(exp), (long)(int8_t)(act))
#define zassert_s8_gt(exp, act)                                                \
    ztest__s(__LINE__, ZT_GT, 1, (long)(int8_t)(exp), (long)(int8_t)(act))
#define zexpect_s8_gt(exp, act)                                                \
    ztest__s(__LINE__, ZT_GT, 0, (long)(int8_t)(exp), (long)(int8_t)(act))
#define zassert_s8_ge(exp, act)                                                \
    ztest__s(__LINE__, ZT_GE, 1, (long)(int8_t)(exp), (long)(int8_t)(act))
#define zexpect_s8_ge(exp, act)                                                \
    ztest__s(__LINE__, ZT_GE, 0, (long)(int8_t)(exp), (long)(int8_t)(act))
#define zassert_s8_lt(exp, act)                                                \
    ztest__s(__LINE__, ZT_LT, 1, (long)(int8_t)(exp), (long)(int8_t)(act))
#define zexpect_s8_lt(exp, act)                                                \
    ztest__s(__LINE__, ZT_LT, 0, (long)(int8_t)(exp), (long)(int8_t)(act))
#define zassert_s8_le(exp, act)                                                \
    ztest__s(__LINE__, ZT_LE, 1, (long)(int8_t)(exp), (long)(int8_t)(act))
#define zexpect_s8_le(exp, act)                                                \
    ztest__s(__LINE__, ZT_LE, 0, (long)(int8_t)(exp), (long)(int8_t)(act))

#define zassert_u32_eq(exp, act)                                               \
    ztest__u(__LINE__, ZT_EQ, 1, (unsigned long)(uint32_t)(exp),               \
             (unsigned long)(uint32_t)(act))
#define zexpect_u32_eq(exp, act)                                               \
    ztest__u(__LINE__, ZT_EQ, 0, (unsigned long)(uint32_t)(exp),               \
             (unsigned long)(uint32_t)(act))
#define zassert_u32_ne(exp, act)                                               \
    ztest__u(__LINE__, ZT_NE, 1, (unsigned long)(uint32_t)(exp),               \
             (unsigned long)(uint32_t)(act))
#define zexpect_u32_ne(exp, act)                                               \
    ztest__u(__LINE__, ZT_NE, 0, (unsigned long)(uint32_t)(exp),               \
             (unsigned long)(uint32_t)(act))
#define zassert_u32_gt(exp, act)                                               \
    ztest__u(__LINE__, ZT_GT, 1, (unsigned long)(uint32_t)(exp),               \
             (unsigned long)(uint32_t)(act))
#define zexpect_u32_gt(exp, act)                                               \
    ztest__u(__LINE__, ZT_GT, 0, (unsigned long)(uint32_t)(exp),               \
             (unsigned long)(uint32_t)(act))
#define zassert_u32_ge(exp, act)                                               \
    ztest__u(__LINE__, ZT_GE, 1, (unsigned long)(uint32_t)(exp),               \
             (unsigned long)(uint32_t)(act))
#define zexpect_u32_ge(exp, act)                                               \
    ztest__u(__LINE__, ZT_GE, 0, (unsigned long)(uint32_t)(exp),               \
             (unsigned long)(uint32_t)(act))
#define zassert_u32_lt(exp, act)                                               \
    ztest__u(__LINE__, ZT_LT, 1, (unsigned long)(uint32_t)(exp),               \
             (unsigned long)(uint32_t)(act))
#define zexpect_u32_lt(exp, act)                                               \
    ztest__u(__LINE__, ZT_LT, 0, (unsigned long)(uint32_t)(exp),               \
             (unsigned long)(uint32_t)(act))
#define zassert_u32_le(exp, act)                                               \
    ztest__u(__LINE__, ZT_LE, 1, (unsigned long)(uint32_t)(exp),               \
             (unsigned long)(uint32_t)(act))
#define zexpect_u32_le(exp, act)                                               \
    ztest__u(__LINE__, ZT_LE, 0, (unsigned long)(uint32_t)(exp),               \
             (unsigned long)(uint32_t)(act))

#define zassert_u16_eq(exp, act)                                               \
    ztest__u(__LINE__, ZT_EQ, 1, (unsigned long)(uint16_t)(exp),               \
             (unsigned long)(uint16_t)(act))
#define zexpect_u16_eq(exp, act)                                               \
    ztest__u(__LINE__, ZT_EQ, 0, (unsigned long)(uint16_t)(exp),               \
             (unsigned long)(uint16_t)(act))
#define zassert_u16_ne(exp, act)                                               \
    ztest__u(__LINE__, ZT_NE, 1, (unsigned long)(uint16_t)(exp),               \
             (unsigned long)(uint16_t)(act))
#define zexpect_u16_ne(exp, act)                                               \
    ztest__u(__LINE__, ZT_NE, 0, (unsigned long)(uint16_t)(exp),               \
             (unsigned long)(uint16_t)(act))
#define zassert_u16_gt(exp, act)                                               \
    ztest__u(__LINE__, ZT_GT, 1, (unsigned long)(uint16_t)(exp),               \
             (unsigned long)(uint16_t)(act))
#define zexpect_u16_gt(exp, act)                                               \
    ztest__u(__LINE__, ZT_GT, 0, (unsigned long)(uint16_t)(exp),               \
             (unsigned long)(uint16_t)(act))
#define zassert_u16_ge(exp, act)                                               \
    ztest__u(__LINE__, ZT_GE, 1, (unsigned long)(uint16_t)(exp),               \
             (unsigned long)(uint16_t)(act))
#define zexpect_u16_ge(exp, act)                                               \
    ztest__u(__LINE__, ZT_GE, 0, (unsigned long)(uint16_t)(exp),               \
             (unsigned long)(uint16_t)(act))
#define zassert_u16_lt(exp, act)                                               \
    ztest__u(__LINE__, ZT_LT, 1, (unsigned long)(uint16_t)(exp),               \
             (unsigned long)(uint16_t)(act))
#define zexpect_u16_lt(exp, act)                                               \
    ztest__u(__LINE__, ZT_LT, 0, (unsigned long)(uint16_t)(exp),               \
             (unsigned long)(uint16_t)(act))
#define zassert_u16_le(exp, act)                                               \
    ztest__u(__LINE__, ZT_LE, 1, (unsigned long)(uint16_t)(exp),               \
             (unsigned long)(uint16_t)(act))
#define zexpect_u16_le(exp, act)                                               \
    ztest__u(__LINE__, ZT_LE, 0, (unsigned long)(uint16_t)(exp),               \
             (unsigned long)(uint16_t)(act))

#define zassert_u8_eq(exp, act)                                                \
    ztest__u(__LINE__, ZT_EQ, 1, (unsigned long)(uint8_t)(exp),                \
             (unsigned long)(uint8_t)(act))
#define zexpect_u8_eq(exp, act)                                                \
    ztest__u(__LINE__, ZT_EQ, 0, (unsigned long)(uint8_t)(exp),                \
             (unsigned long)(uint8_t)(act))
#define zassert_u8_ne(exp, act)                                                \
    ztest__u(__LINE__, ZT_NE, 1, (unsigned long)(uint8_t)(exp),                \
             (unsigned long)(uint8_t)(act))
#define zexpect_u8_ne(exp, act)                                                \
    ztest__u(__LINE__, ZT_NE, 0, (unsigned long)(uint8_t)(exp),                \
             (unsigned long)(uint8_t)(act))
#define zassert_u8_gt(exp, act)                                                \
    ztest__u(__LINE__, ZT_GT, 1, (unsigned long)(uint8_t)(exp),                \
             (unsigned long)(uint8_t)(act))
#define zexpect_u8_gt(exp, act)                                                \
    ztest__u(__LINE__, ZT_GT, 0, (unsigned long)(uint8_t)(exp),                \
             (unsigned long)(uint8_t)(act))
#define zassert_u8_ge(exp, act)                                                \
    ztest__u(__LINE__, ZT_GE, 1, (unsigned long)(uint8_t)(exp),                \
             (unsigned long)(uint8_t)(act))
#define zexpect_u8_ge(exp, act)                                                \
    ztest__u(__LINE__, ZT_GE, 0, (unsigned long)(uint8_t)(exp),                \
             (unsigned long)(uint8_t)(act))
#define zassert_u8_lt(exp, act)                                                \
    ztest__u(__LINE__, ZT_LT, 1, (unsigned long)(uint8_t)(exp),                \
             (unsigned long)(uint8_t)(act))
#define zexpect_u8_lt(exp, act)                                                \
    ztest__u(__LINE__, ZT_LT, 0, (unsigned long)(uint8_t)(exp),                \
             (unsigned long)(uint8_t)(act))
#define zassert_u8_le(exp, act)                                                \
    ztest__u(__LINE__, ZT_LE, 1, (unsigned long)(uint8_t)(exp),                \
             (unsigned long)(uint8_t)(act))
#define zexpect_u8_le(exp, act)                                                \
    ztest__u(__LINE__, ZT_LE, 0, (unsigned long)(uint8_t)(exp),                \
             (unsigned long)(uint8_t)(act))

#define zassert_char_eq(exp, act)                                              \
    ztest__c(__LINE__, ZT_EQ, 1, (char)(exp), (char)(act))
#define zexpect_char_eq(exp, act)                                              \
    ztest__c(__LINE__, ZT_EQ, 0, (char)(exp), (char)(act))
#define zassert_char_ne(exp, act)                                              \
    ztest__c(__LINE__, ZT_NE, 1, (char)(exp), (char)(act))
#define zexpect_char_ne(exp, act)                                              \
    ztest__c(__LINE__, ZT_NE, 0, (char)(exp), (char)(act))
#define zassert_char_gt(exp, act)                                              \
    ztest__c(__LINE__, ZT_GT, 1, (char)(exp), (char)(act))
#define zexpect_char_gt(exp, act)                                              \
    ztest__c(__LINE__, ZT_GT, 0, (char)(exp), (char)(act))
#define zassert_char_ge(exp, act)                                              \
    ztest__c(__LINE__, ZT_GE, 1, (char)(exp), (char)(act))
#define zexpect_char_ge(exp, act)                                              \
    ztest__c(__LINE__, ZT_GE, 0, (char)(exp), (char)(act))
#define zassert_char_lt(exp, act)                                              \
    ztest__c(__LINE__, ZT_LT, 1, (char)(exp), (char)(act))
#define zexpect_char_lt(exp, act)                                              \
    ztest__c(__LINE__, ZT_LT, 0, (char)(exp), (char)(act))
#define zassert_char_le(exp, act)                                              \
    ztest__c(__LINE__, ZT_LE, 1, (char)(exp), (char)(act))
#define zexpect_char_le(exp, act)                                              \
    ztest__c(__LINE__, ZT_LE, 0, (char)(exp), (char)(act))

#define zassert_str_eq(exp, act)                                               \
    ztest__str(__LINE__, ZT_EQ, 1, (const char*)(exp), (const char*)(act))
#define zexpect_str_eq(exp, act)                                               \
    ztest__str(__LINE__, ZT_EQ, 0, (const char*)(exp), (const char*)(act))
#define zassert_str_ne(exp, act)                                               \
    ztest__str(__LINE__, ZT_NE, 1, (const char*)(exp), (const char*)(act))
#define zexpect_str_ne(exp, act)                                               \
    ztest__str(__LINE__, ZT_NE, 0, (const char*)(exp), (const char*)(act))
#define zassert_str_gt(exp, act)                                               \
    ztest__str(__LINE__, ZT_GT, 1, (const char*)(exp), (const char*)(act))
#define zexpect_str_gt(exp, act)                                               \
    ztest__str(__LINE__, ZT_GT, 0, (const char*)(exp), (const char*)(act))
#define zassert_str_ge(exp, act)                                               \
    ztest__str(__LINE__, ZT_GE, 1, (const char*)(exp), (const char*)(act))
#define zexpect_str_ge(exp, act)                                               \
    ztest__str(__LINE__, ZT_GE, 0, (const char*)(exp), (const char*)(act))
#define zassert_str_lt(exp, act)                                               \
    ztest__str(__LINE__, ZT_LT, 1, (const char*)(exp), (const char*)(act))
#define zexpect_str_lt(exp, act)                                               \
    ztest__str(__LINE__, ZT_LT, 0, (const char*)(exp), (const char*)(act))
#define zassert_str_le(exp, act)                                               \
    ztest__str(__LINE__, ZT_LE, 1, (const char*)(exp), (const char*)(act))
#define zexpect_str_le(exp, act)                                               \
    ztest__str(__LINE__, ZT_LE, 0, (const char*)(exp), (const char*)(act))

#define zassert_ptr_eq(exp, act)                                               \
    ztest__p(__LINE__, ZT_EQ, 1, (const void*)(exp), (const void*)(act))
#define zexpect_ptr_eq(exp, act)                                               \
    ztest__p(__LINE__, ZT_EQ, 0, (const void*)(exp), (const void*)(act))
#define zassert_ptr_ne(exp, act)                                               \
    ztest__p(__LINE__, ZT_NE, 1, (const void*)(exp), (const void*)(act))
#define zexpect_ptr_ne(exp, act)                                               \
    ztest__p(__LINE__, ZT_NE, 0, (const void*)(exp), (const void*)(act))
#define zassert_ptr_gt(exp, act)                                               \
    ztest__p(__LINE__, ZT_GT, 1, (const void*)(exp), (const void*)(act))
#define zexpect_ptr_gt(exp, act)                                               \
    ztest__p(__LINE__, ZT_GT, 0, (const void*)(exp), (const void*)(act))
#define zassert_ptr_ge(exp, act)                                               \
    ztest__p(__LINE__, ZT_GE, 1, (const void*)(exp), (const void*)(act))
#define zexpect_ptr_ge(exp, act)                                               \
    ztest__p(__LINE__, ZT_GE, 0, (const void*)(exp), (const void*)(act))
#define zassert_ptr_lt(exp, act)                                               \
    ztest__p(__LINE__, ZT_LT, 1, (const void*)(exp), (const void*)(act))
#define zexpect_ptr_lt(exp, act)                                               \
    ztest__p(__LINE__, ZT_LT, 0, (const void*)(exp), (const void*)(act))
#define zassert_ptr_le(exp, act)                                               \
    ztest__p(__LINE__, ZT_LE, 1, (const void*)(exp), (const void*)(act))
#define zexpect_ptr_le(exp, act)                                               \
    ztest__p(__LINE__, ZT_LE, 0, (const void*)(exp), (const void*)(act))

#define zassert_uintptr_eq(exp, act)                                           \
    ztest__up(__LINE__, ZT_EQ, 1, (uintptr_t)(exp), (uintptr_t)(act))
#define zexpect_uintptr_eq(exp, act)                                           \
    ztest__up(__LINE__, ZT_EQ, 0, (uintptr_t)(exp), (uintptr_t)(act))
#define zassert_uintptr_ne(exp, act)                                           \
    ztest__up(__LINE__, ZT_NE, 1, (uintptr_t)(exp), (uintptr_t)(act))
#define zexpect_uintptr_ne(exp, act)                                           \
    ztest__up(__LINE__, ZT_NE, 0, (uintptr_t)(exp), (uintptr_t)(act))
#define zassert_uintptr_gt(exp, act)                                           \
    ztest__up(__LINE__, ZT_GT, 1, (uintptr_t)(exp), (uintptr_t)(act))
#define zexpect_uintptr_gt(exp, act)                                           \
    ztest__up(__LINE__, ZT_GT, 0, (uintptr_t)(exp), (uintptr_t)(act))
#define zassert_uintptr_ge(exp, act)                                           \
    ztest__up(__LINE__, ZT_GE, 1, (uintptr_t)(exp), (uintptr_t)(act))
#define zexpect_uintptr_ge(exp, act)                                           \
    ztest__up(__LINE__, ZT_GE, 0, (uintptr_t)(exp), (uintptr_t)(act))
#define zassert_uintptr_lt(exp, act)                                           \
    ztest__up(__LINE__, ZT_LT, 1, (uintptr_t)(exp), (uintptr_t)(act))
#define zexpect_uintptr_lt(exp, act)                                           \
    ztest__up(__LINE__, ZT_LT, 0, (uintptr_t)(exp), (uintptr_t)(act))
#define zassert_uintptr_le(exp, act)                                           \
    ztest__up(__LINE__, ZT_LE, 1, (uintptr_t)(exp), (uintptr_t)(act))
#define zexpect_uintptr_le(exp, act)                                           \
    ztest__up(__LINE__, ZT_LE, 0, (uintptr_t)(exp), (uintptr_t)(act))

#define zassert_u8array_eq(exp, act, len)                                      \
    ztest__bytes(__LINE__, ZT_EQ, 1, (exp), (act), (size_t)(len))
#define zexpect_u8array_eq(exp, act, len)                                      \
    ztest__bytes(__LINE__, ZT_EQ, 0, (exp), (act), (size_t)(len))
#define zassert_u8array_ne(exp, act, len)                                      \
    ztest__bytes(__LINE__, ZT_NE, 1, (exp), (act), (size_t)(len))
#define zexpect_u8array_ne(exp, act, len)                                      \
    ztest__bytes(__LINE__, ZT_NE, 0, (exp), (act), (size_t)(len))
#define zassert_u8array_gt(exp, act, len)                                      \
    ztest__bytes(__LINE__, ZT_GT, 1, (exp), (act), (size_t)(len))
#define zexpect_u8array_gt(exp, act, len)                                      \
    ztest__bytes(__LINE__, ZT_GT, 0, (exp), (act), (size_t)(len))
#define zassert_u8array_ge(exp, act, len)                                      \
    ztest__bytes(__LINE__, ZT_GE, 1, (exp), (act), (size_t)(len))
#define zexpect_u8array_ge(exp, act, len)                                      \
    ztest__bytes(__LINE__, ZT_GE, 0, (exp), (act), (size_t)(len))
#define zassert_u8array_lt(exp, act, len)                                      \
    ztest__bytes(__LINE__, ZT_LT, 1, (exp), (act), (size_t)(len))
#define zexpect_u8array_lt(exp, act, len)                                      \
    ztest__bytes(__LINE__, ZT_LT, 0, (exp), (act), (size_t)(len))
#define zassert_u8array_le(exp, act, len)                                      \
    ztest__bytes(__LINE__, ZT_LE, 1, (exp), (act), (size_t)(len))
#define zexpect_u8array_le(exp, act, len)                                      \
    ztest__bytes(__LINE__, ZT_LE, 0, (exp), (act), (size_t)(len))

#define zassert_image_eq(name, act, cmp)                                       \
    ztest__image(__LINE__, ZT_EQ, 1, (name), (act), (cmp))
#define zexpect_image_eq(name, act, cmp)                                       \
    ztest__image(__LINE__, ZT_EQ, 0, (name), (act), (cmp))
#define zassert_image_ne(name, act, cmp)                                       \
    ztest__image(__LINE__, ZT_NE, 1, (name), (act), (cmp))
#define zexpect_image_ne(name, act, cmp)                                       \
    ztest__image(__LINE__, ZT_NE, 0, (name), (act), (cmp))

#ifdef __cplusplus
}
#endif

#endif
