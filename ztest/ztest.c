#include "ztest.h"

#include <stdlib.h>
#include <string.h>

#if defined(__psx__) || defined(__PSX__)
#define ZT_PS1 1
#endif

#ifdef ZT_PS1
// Needs no libc: nugget ships no setjmp.h. The jump value is always 1, state
// lives in statics.
typedef void* zt_jmp_buf[5];
#define ZT_SETJMP(b) __builtin_setjmp(b)
#define ZT_LONGJMP(b) __builtin_longjmp(b, 1)
#else
#include <setjmp.h>
typedef jmp_buf zt_jmp_buf;
#define ZT_SETJMP(b) setjmp(b)
#define ZT_LONGJMP(b) longjmp(b, 1)
#endif

#if defined(ZTEST_HEAP_SIZE) && !defined(ZTEST_MALLOC)
static void* zt_heap_malloc(size_t size);
static void* zt_heap_realloc(void* ptr, size_t size);
static void zt_heap_free(void* ptr);
#define ZTEST_MALLOC(sz) zt_heap_malloc(sz)
#define ZTEST_REALLOC(p, sz) zt_heap_realloc(p, sz)
#define ZTEST_FREE(p) zt_heap_free(p)
#endif

#ifndef ZTEST_MALLOC
#define ZTEST_MALLOC(sz) malloc(sz)
#define ZTEST_REALLOC(p, sz) realloc(p, sz)
#define ZTEST_FREE(p) free(p)
#endif

#ifndef ZTEST_LOG_SIZE
#define ZTEST_LOG_SIZE 8192
#endif

#if defined(__PSP__)
#include <pspdisplay.h>
#include <pspiofilemgr.h>
#define ZT_HOSTED 0
#elif defined(ZT_PS1)
#define ZT_HOSTED 0
#elif defined(N64)
#define ZT_N64 1
#define ZT_HOSTED 0
#include <libdragon.h>
#include <stdio.h>
#elif defined(__NDS__)
#define ZT_NDS 1
#define ZT_HOSTED 0
#include <filesystem.h>
#include <nds.h>
#include <stdio.h>
#else
#define ZT_HOSTED 1
#include <stdio.h>
#if defined(_WIN32)
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif
#endif

#if defined(__GNUC__) && !defined(__clang__)
#define ZT_LIBC __attribute__((optimize("no-tree-loop-distribute-patterns")))
#else
#define ZT_LIBC
#endif

#ifdef ZTEST_NEED_MEMCPY
ZT_LIBC void* memcpy(void* dst, const void* src, size_t n) {
    unsigned char* d = dst;
    const unsigned char* s = src;
    while (n--) {
        *d++ = *s++;
    }
    return dst;
}
#endif

#ifdef ZTEST_NEED_MEMMOVE
ZT_LIBC void* memmove(void* dst, const void* src, size_t n) {
    unsigned char* d = dst;
    const unsigned char* s = src;
    if (d < s) {
        while (n--) {
            *d++ = *s++;
        }
    } else {
        while (n--) {
            d[n] = s[n];
        }
    }
    return dst;
}
#endif

#ifdef ZTEST_NEED_MEMSET
ZT_LIBC void* memset(void* dst, int c, size_t n) {
    unsigned char* d = dst;
    while (n--) {
        *d++ = (unsigned char)c;
    }
    return dst;
}
#endif

#ifdef ZTEST_NEED_MEMCMP
int memcmp(const void* a, const void* b, size_t n) {
    const unsigned char *x = a, *y = b;
    for (; n; n--, x++, y++) {
        if (*x != *y) {
            return *x - *y;
        }
    }
    return 0;
}
#endif

#ifdef ZTEST_NEED_STRLEN
size_t strlen(const char* s) {
    size_t n = 0;
    while (s[n]) {
        n++;
    }
    return n;
}
#endif

#ifdef ZTEST_NEED_STRCMP
int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}
#endif

#ifdef ZTEST_NEED_STRNCMP
int strncmp(const char* a, const char* b, size_t n) {
    for (; n; n--, a++, b++) {
        if (*a != *b || !*a) {
            return (unsigned char)*a - (unsigned char)*b;
        }
    }
    return 0;
}
#endif

#ifdef ZTEST_NEED_STRCHR
char* strchr(const char* s, int c) {
    for (;; s++) {
        if (*s == (char)c) {
            return (char*)s;
        }
        if (!*s) {
            return NULL;
        }
    }
}
#endif

#ifdef ZTEST_NEED_STRCSPN
size_t strcspn(const char* s, const char* reject) {
    size_t n = 0;
    while (s[n] && !strchr(reject, s[n])) {
        n++;
    }
    return n;
}
#endif

#ifdef ZTEST_NEED_STRCPY
ZT_LIBC char* strcpy(char* dst, const char* src) {
    char* d = dst;
    while ((*d++ = *src++)) {
    }
    return dst;
}
#endif

#ifdef ZTEST_NEED_STRCAT
char* strcat(char* dst, const char* src) {
    strcpy(dst + strlen(dst), src);
    return dst;
}
#endif

#ifdef ZTEST_NEED_ABS
int abs(int v) { return v < 0 ? -v : v; }
#endif

#ifdef ZTEST_HEAP_SIZE
// First-fit heap for freestanding builds without a usable allocator.
typedef union zt_chunk {
    struct {
        size_t size; // payload bytes, excluding this header
        int used;
        union zt_chunk* next;
    } h;
    double align;
    char pad[16];
} zt_chunk;

static zt_chunk zt_heap_mem[ZTEST_HEAP_SIZE / sizeof(zt_chunk)];
static zt_chunk* zt_heap_head;

static void* zt_heap_malloc(size_t size) {
    zt_chunk* c;
    size = (size + sizeof(zt_chunk) - 1) / sizeof(zt_chunk) * sizeof(zt_chunk);
    if (!zt_heap_head) {
        zt_heap_head = zt_heap_mem;
        zt_heap_head->h.size = sizeof(zt_heap_mem) - sizeof(zt_chunk);
        zt_heap_head->h.used = 0;
        zt_heap_head->h.next = NULL;
    }
    for (c = zt_heap_head; c; c = c->h.next) {
        if (c->h.used || c->h.size < size) {
            continue;
        }
        if (c->h.size >= size + 2 * sizeof(zt_chunk)) {
            zt_chunk* rest = c + 1 + size / sizeof(zt_chunk);
            rest->h.size = c->h.size - size - sizeof(zt_chunk);
            rest->h.used = 0;
            rest->h.next = c->h.next;
            c->h.next = rest;
            c->h.size = size;
        }
        c->h.used = 1;
        return c + 1;
    }
    return NULL;
}

static void zt_heap_free(void* ptr) {
    zt_chunk* c;
    if (!ptr) {
        return;
    }
    ((zt_chunk*)ptr - 1)->h.used = 0;
    for (c = zt_heap_head; c; c = c->h.next) {
        while (!c->h.used && c->h.next && !c->h.next->h.used) {
            c->h.size += sizeof(zt_chunk) + c->h.next->h.size;
            c->h.next = c->h.next->h.next;
        }
    }
}

static void* zt_heap_realloc(void* ptr, size_t size) {
    void* p;
    size_t old;
    if (!ptr) {
        return zt_heap_malloc(size);
    }
    old = ((zt_chunk*)ptr - 1)->h.size;
    if (old >= size) {
        return ptr;
    }
    p = zt_heap_malloc(size);
    if (p) {
        memcpy(p, ptr, old);
        zt_heap_free(ptr);
    }
    return p;
}
#endif

// ---------------------------------------------------------------------------
// Platform layer
// ---------------------------------------------------------------------------

static void zt_plat_write(const char* s, size_t n);
static void zt_plat_flush(void);
static void* zt_plat_read_file(const char* path, size_t* size);
static int zt_plat_write_file(const char* path, const void* data, size_t size);
static void zt_plat_remove_file(const char* path);

#if defined(__PSP__)

static void zt_plat_write(const char* s, size_t n) { sceIoWrite(1, s, n); }
static void zt_plat_flush(void) {}

static void* zt_plat_read_file(const char* path, size_t* size) {
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd < 0) {
        return NULL;
    }
    SceOff len = sceIoLseek(fd, 0, PSP_SEEK_END);
    sceIoLseek(fd, 0, PSP_SEEK_SET);
    unsigned char* data = len > 0 ? ZTEST_MALLOC((size_t)len) : NULL;
    size_t got = 0;
    while (data && got < (size_t)len) {
        int n = sceIoRead(fd, data + got, (SceSize)((size_t)len - got));
        if (n <= 0) {
            break;
        }
        got += (size_t)n;
    }
    sceIoClose(fd);
    if (data && got != (size_t)len) {
        ZTEST_FREE(data);
        return NULL;
    }
    *size = got;
    return data;
}

static int zt_plat_write_file(const char* path, const void* data, size_t size) {
    SceUID fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0) {
        return 0;
    }
    int n = sceIoWrite(fd, data, (SceSize)size);
    sceIoClose(fd);
    return n == (int)size;
}

static void zt_plat_remove_file(const char* path) { sceIoRemove(path); }

#elif defined(ZT_PS1)

#define ZT_HW8(a) (*(volatile unsigned char*)(a))
#define ZT_HW16(a) (*(volatile unsigned short*)(a))
#define ZT_HW32(a) (*(volatile unsigned int*)(a))

static int zt_ps1_redux(void) { return ZT_HW32(0x1F802080) == 0x58534350; }

static void zt_ps1_putchar(int c) {
    register int n __asm__("t1") = 0x3C;
    __asm__ volatile("" : "=r"(n) : "r"(n));
    ((void (*)(int))0xA0)(c);
}

static void zt_plat_flush(void) {}

// PCDrv as pcsx-redux traps it: break instructions, results in v0/v1.
static int zt_brk_open(const char* name, int flags) {
    register int a2 __asm__("a2") = flags;
    register const char* a0 __asm__("a0") = name;
    register const char* a1 __asm__("a1") = name;
    register int v0 __asm__("v0");
    register int v1 __asm__("v1");
    __asm__ volatile("break 0, 0x103\n"
                     : "=r"(v0), "=r"(v1)
                     : "r"(a0), "r"(a1), "r"(a2));
    return v0 == 0 ? v1 : -1;
}

static int zt_brk_creat(const char* name) {
    register const char* a0 __asm__("a0") = name;
    register const char* a1 __asm__("a1") = name;
    register int a2 __asm__("a2") = 0;
    register int v0 __asm__("v0");
    register int v1 __asm__("v1");
    __asm__ volatile("break 0, 0x102\n"
                     : "=r"(v0), "=r"(v1)
                     : "r"(a0), "r"(a1), "r"(a2));
    return v0 == 0 ? v1 : -1;
}

static void zt_brk_close(int fd) {
    register int a0 __asm__("a0") = fd;
    register int a1 __asm__("a1") = fd;
    register int v0 __asm__("v0");
    __asm__ volatile("break 0, 0x104\n" : "=r"(v0) : "r"(a0), "r"(a1) : "v1");
    (void)v0;
}

static int zt_brk_read(int fd, void* buf, int len) {
    register int a0 __asm__("a0") = 0;
    register int a1 __asm__("a1") = fd;
    register int a2 __asm__("a2") = len;
    register void* a3 __asm__("a3") = buf;
    register int v0 __asm__("v0");
    register int v1 __asm__("v1");
    __asm__ volatile("break 0, 0x105\n"
                     : "=r"(v0), "=r"(v1)
                     : "r"(a0), "r"(a1), "r"(a2), "r"(a3)
                     : "memory");
    return v0 == 0 ? v1 : -1;
}

static int zt_brk_write(int fd, const void* buf, int len) {
    register int a0 __asm__("a0") = 0;
    register int a1 __asm__("a1") = fd;
    register int a2 __asm__("a2") = len;
    register const void* a3 __asm__("a3") = buf;
    register int v0 __asm__("v0");
    register int v1 __asm__("v1");
    __asm__ volatile("break 0, 0x106\n"
                     : "=r"(v0), "=r"(v1)
                     : "r"(a0), "r"(a1), "r"(a2), "r"(a3)
                     : "memory");
    return v0 == 0 ? v1 : -1;
}

static int zt_brk_lseek(int fd, int offset, int whence) {
    register int a3 __asm__("a3") = whence;
    register int a2 __asm__("a2") = offset;
    register int a0 __asm__("a0") = fd;
    register int a1 __asm__("a1") = fd;
    register int v0 __asm__("v0");
    register int v1 __asm__("v1");
    __asm__ volatile("break 0, 0x107\n"
                     : "=r"(v0), "=r"(v1)
                     : "r"(a0), "r"(a1), "r"(a2), "r"(a3));
    return v0 == 0 ? v1 : -1;
}

// PCDrv on real hardware: the NOTPSXSerial (nops /m) protocol over SIO1. A
// call is the escape {0x00, 'p'}, a u32 function code and the host's "OKAY".
#define ZT_SIO_DATA 0x1F801050
#define ZT_SIO_STAT 0x1F801054

static void zt_sio_put(unsigned char b) {
    while (!(ZT_HW32(ZT_SIO_STAT) & 1)) {
    }
    ZT_HW8(ZT_SIO_DATA) = b;
}

static unsigned char zt_sio_get(void) {
    while (!(ZT_HW32(ZT_SIO_STAT) & 2)) {
    }
    return ZT_HW8(ZT_SIO_DATA);
}

static void zt_sio_put32(unsigned v) {
    zt_sio_put((unsigned char)v);
    zt_sio_put((unsigned char)(v >> 8));
    zt_sio_put((unsigned char)(v >> 16));
    zt_sio_put((unsigned char)(v >> 24));
}

static unsigned zt_sio_get32(void) {
    unsigned v = zt_sio_get();
    v |= (unsigned)zt_sio_get() << 8;
    v |= (unsigned)zt_sio_get() << 16;
    return v | (unsigned)zt_sio_get() << 24;
}

static void zt_sio_puts4(const char* s) {
    int i;
    for (i = 0; i < 4; i++) {
        zt_sio_put((unsigned char)s[i]);
    }
}

static int zt_sio_okay(void) {
    char r[4];
    int i;
    for (i = 0; i < 4; i++) {
        r[i] = (char)zt_sio_get();
    }
    return r[0] == 'O' && r[1] == 'K' && r[2] == 'A' && r[3] == 'Y';
}

static int zt_sio_begin(unsigned func) {
    zt_sio_put(0x00);
    zt_sio_put('p');
    zt_sio_put32(func);
    return zt_sio_okay();
}

static void zt_sio_name(const char* name) {
    while (*name) {
        zt_sio_put((unsigned char)*name++);
    }
    zt_sio_put(0);
}

static int zt_sio_open(const char* name, int mode) {
    if (!zt_sio_begin(0x103)) {
        return -1;
    }
    zt_sio_name(name);
    zt_sio_put32((unsigned)mode);
    return zt_sio_okay() ? (int)zt_sio_get32() : -1;
}

static int zt_sio_creat(const char* name) {
    if (!zt_sio_begin(0x102)) {
        return -1;
    }
    zt_sio_name(name);
    zt_sio_put32(0);
    return zt_sio_okay() ? (int)zt_sio_get32() : -1;
}

static void zt_sio_close(int fd) {
    if (!zt_sio_begin(0x104)) {
        return;
    }
    zt_sio_put32((unsigned)fd);
    zt_sio_put32(0);
    zt_sio_put32(0);
    if (zt_sio_okay()) {
        zt_sio_get32();
    }
}

// The host answers with the requested length, zero padding a short file, and
// runs a CHEK/MORE handshake after every 2048-byte chunk.
static int zt_sio_read(int fd, void* buf, int len) {
    unsigned char* out = buf;
    int avail, done = 0, i;
    if (!zt_sio_begin(0x105)) {
        return -1;
    }
    zt_sio_put32((unsigned)fd);
    zt_sio_put32((unsigned)len);
    zt_sio_put32((unsigned)(uintptr_t)buf);
    if (!zt_sio_okay()) {
        return -1;
    }
    avail = (int)zt_sio_get32();
    zt_sio_get32();
    while (done < avail) {
        int chunk = avail - done > 2048 ? 2048 : avail - done;
        unsigned sum = 0;
        for (i = 0; i < chunk; i++) {
            out[done + i] = zt_sio_get();
            sum += out[done + i];
        }
        zt_sio_puts4("CHEK");
        if (zt_sio_get32() != sum) {
            zt_sio_puts4("ERR!");
            continue;
        }
        zt_sio_puts4("MORE");
        done += chunk;
    }
    return avail;
}

// The host echoes "MORE" on every 2048-byte boundary and checks a DJB2 sum.
static int zt_sio_write(int fd, const void* buf, int len) {
    const unsigned char* in = buf;
    unsigned sum = 5381;
    int i;
    if (!zt_sio_begin(0x106)) {
        return -1;
    }
    zt_sio_put32((unsigned)fd);
    zt_sio_put32((unsigned)len);
    zt_sio_put32((unsigned)(uintptr_t)buf);
    if (!zt_sio_okay()) {
        return -1;
    }
    for (i = 0; i < len; i++) {
        zt_sio_put(in[i]);
        sum = ((sum << 5) + sum) ^ in[i];
        if ((i + 1) % 2048 == 0) {
            zt_sio_get32();
        }
    }
    zt_sio_put32(sum);
    return zt_sio_okay() ? (int)zt_sio_get32() : -1;
}

static int zt_sio_lseek(int fd, int offset, int whence) {
    if (!zt_sio_begin(0x107)) {
        return -1;
    }
    zt_sio_put32((unsigned)fd);
    zt_sio_put32((unsigned)offset);
    zt_sio_put32((unsigned)whence);
    return zt_sio_okay() ? (int)zt_sio_get32() : -1;
}

static int zt_pc_ready;
static int zt_pc_redux;
static int zt_ps1_log = -1;

static void zt_pc_init(void) {
    if (zt_pc_ready) {
        return;
    }
    zt_pc_ready = 1;
    zt_pc_redux = zt_ps1_redux();
    if (!zt_pc_redux && zt_sio_begin(0x101)) {
        zt_sio_get(); // PCINIT replies OKAY and a single byte
        zt_ps1_log = zt_sio_creat("ztest.log");
    }
}

// On hardware nops narrates PCDrv traffic over the same TTY, so the output
// goes to ztest.log on the host instead, which the runner tails.
static void zt_plat_write(const char* s, size_t n) {
    zt_pc_init();
    if (zt_ps1_log >= 0) {
        zt_sio_write(zt_ps1_log, s, (int)n);
        return;
    }
    while (n--) {
        zt_ps1_putchar(*s++);
    }
}

static void* zt_plat_read_file(const char* path, size_t* size) {
    int fd, len;
    unsigned char* data;
    zt_pc_init();
    fd = zt_pc_redux ? zt_brk_open(path, 0) : zt_sio_open(path, 0);
    if (fd < 0) {
        return NULL;
    }
    len = zt_pc_redux ? zt_brk_lseek(fd, 0, 2) : zt_sio_lseek(fd, 0, 2);
    if (zt_pc_redux) {
        zt_brk_lseek(fd, 0, 0);
    } else {
        zt_sio_lseek(fd, 0, 0);
    }
    data = len > 0 ? ZTEST_MALLOC((size_t)len) : NULL;
    if (data && (zt_pc_redux ? zt_brk_read(fd, data, len)
                             : zt_sio_read(fd, data, len)) != len) {
        ZTEST_FREE(data);
        data = NULL;
    }
    if (zt_pc_redux) {
        zt_brk_close(fd);
    } else {
        zt_sio_close(fd);
    }
    if (data) {
        *size = (size_t)len;
    }
    return data;
}

static int zt_plat_write_file(const char* path, const void* data, size_t size) {
    int fd, n;
    zt_pc_init();
    fd = zt_pc_redux ? zt_brk_creat(path) : zt_sio_creat(path);
    if (fd < 0) {
        return 0;
    }
    n = zt_pc_redux ? zt_brk_write(fd, data, (int)size)
                    : zt_sio_write(fd, data, (int)size);
    if (zt_pc_redux) {
        zt_brk_close(fd);
    } else {
        zt_sio_close(fd);
    }
    return n == (int)size;
}

// PCDrv has no unlink.
static void zt_plat_remove_file(const char* path) { (void)path; }

#elif defined(ZT_N64)

// The runner patches this marker in a copy of the ROM with the arguments.
__attribute__((used)) char ztest_n64_args[256] = "@ZTEST_ARGS@";

// Breakpoint target for the runner's gdb, as emulators rarely route
// ISViewer to a terminal; the text also goes to ISViewer through stderr.
__attribute__((noinline, used)) void ztest_n64_log(const char* text) {
    __asm__ volatile("" : : "r"(text) : "memory");
}

static void zt_plat_write(const char* s, size_t n) {
    char buf[128];
    fwrite(s, 1, n, stderr);
    while (n) {
        size_t len = n < sizeof(buf) - 1 ? n : sizeof(buf) - 1;
        memcpy(buf, s, len);
        buf[len] = '\0';
        ztest_n64_log(buf);
        s += len;
        n -= len;
    }
}

static void zt_plat_flush(void) { fflush(stderr); }

static void* zt_stdio_read(const char* path, size_t* size);

static void* zt_plat_read_file(const char* path, size_t* size) {
    return zt_stdio_read(path, size);
}

// No host filesystem: files travel as base64 blocks in the output.
static int zt_plat_write_file(const char* path, const void* data, size_t size) {
    static const char b64[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const unsigned char* p = data;
    char line[80];
    size_t i = 0;
    ztest_snprintf(
        line, sizeof(line), "@ztest-file %s %u\n", path, (unsigned)size);
    zt_plat_write(line, strlen(line));
    while (i < size) {
        size_t n = 0, end = i + 57 < size ? i + 57 : size;
        for (; i < end; i += 3) {
            unsigned v = p[i] << 16;
            v |= i + 1 < size ? p[i + 1] << 8 : 0;
            v |= i + 2 < size ? p[i + 2] : 0;
            line[n++] = b64[(v >> 18) & 63];
            line[n++] = b64[(v >> 12) & 63];
            line[n++] = i + 1 < size ? b64[(v >> 6) & 63] : '=';
            line[n++] = i + 2 < size ? b64[v & 63] : '=';
        }
        line[n++] = '\n';
        zt_plat_write(line, n);
    }
    zt_plat_write("@ztest-end\n", 11);
    zt_plat_flush();
    return 1;
}

static void zt_plat_remove_file(const char* path) { (void)path; }

#elif defined(ZT_NDS)

// Breakpoint targets for the runner's gdb: it prints the text and dumps the
// file to the host. The asm keeps the calls and their arguments alive.
__attribute__((noinline, used)) void ztest_nds_log(const char* text) {
    __asm__ volatile("" : : "r"(text) : "memory");
}

__attribute__((noinline, used)) void ztest_nds_dump(
    const char* path, const void* data, size_t size) {
    __asm__ volatile("" : : "r"(path), "r"(data), "r"(size) : "memory");
}

// Cleared by gdb once attached; argument text is written by gdb too.
volatile int ztest_nds_wait = 1;
char ztest_nds_args[256];

static void zt_plat_write(const char* s, size_t n) {
    char buf[128];
    while (n) {
        size_t len = n < sizeof(buf) - 1 ? n : sizeof(buf) - 1;
        memcpy(buf, s, len);
        buf[len] = '\0';
        ztest_nds_log(buf);
        s += len;
        n -= len;
    }
}

static void zt_plat_flush(void) {}

static void* zt_stdio_read(const char* path, size_t* size);

static void* zt_plat_read_file(const char* path, size_t* size) {
    return zt_stdio_read(path, size);
}

static int zt_plat_write_file(const char* path, const void* data, size_t size) {
    ztest_nds_dump(path, data, size);
    return 1;
}

static void zt_plat_remove_file(const char* path) { (void)path; }

#else

static void zt_plat_write(const char* s, size_t n) { fwrite(s, 1, n, stdout); }
static void zt_plat_flush(void) { fflush(stdout); }

static void* zt_stdio_read(const char* path, size_t* size);

static void* zt_plat_read_file(const char* path, size_t* size) {
    return zt_stdio_read(path, size);
}

static int zt_plat_write_file(const char* path, const void* data, size_t size) {
    FILE* f = fopen(path, "wb");
    if (!f) {
        return 0;
    }
    size_t n = fwrite(data, 1, size, f);
    fclose(f);
    return n == size;
}

static void zt_plat_remove_file(const char* path) { remove(path); }

#endif

#if ZT_HOSTED || defined(ZT_NDS) || defined(ZT_N64)
static void* zt_stdio_read(const char* path, size_t* size) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char* data = len > 0 ? ZTEST_MALLOC((size_t)len) : NULL;
    if (data && fread(data, 1, (size_t)len, f) != (size_t)len) {
        ZTEST_FREE(data);
        data = NULL;
    }
    fclose(f);
    if (data) {
        *size = (size_t)len;
    }
    return data;
}
#endif

// ---------------------------------------------------------------------------
// Formatter
// ---------------------------------------------------------------------------

typedef struct {
    char* buf;
    size_t cap;
    size_t len;
} zt_sink;

static void zt_put(zt_sink* s, char c) {
    if (s->len + 1 < s->cap) {
        s->buf[s->len] = c;
    }
    s->len++;
}

static void zt_put_padded(zt_sink* s, const char* prefix, const char* body,
                          size_t body_len, int width, int left, int zero) {
    size_t prefix_len = strlen(prefix);
    size_t len = prefix_len + body_len;
    size_t pad = width > 0 && (size_t)width > len ? (size_t)width - len : 0;
    size_t i;
    if (!left && !zero) {
        for (i = 0; i < pad; i++) {
            zt_put(s, ' ');
        }
    }
    for (i = 0; i < prefix_len; i++) {
        zt_put(s, prefix[i]);
    }
    if (!left && zero) {
        for (i = 0; i < pad; i++) {
            zt_put(s, '0');
        }
    }
    for (i = 0; i < body_len; i++) {
        zt_put(s, body[i]);
    }
    if (left) {
        for (i = 0; i < pad; i++) {
            zt_put(s, ' ');
        }
    }
}

static size_t zt_utoa(
    char* out, unsigned long long v, unsigned base, int upper) {
    const char* digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char tmp[24];
    size_t n = 0, i;
    do {
        tmp[n++] = digits[v % base];
        v /= base;
    } while (v);
    for (i = 0; i < n; i++) {
        out[i] = tmp[n - 1 - i];
    }
    return n;
}

static size_t zt_ftoa(char* out, double v, int prec) {
    unsigned long long scale = 1, whole, frac;
    size_t n;
    int i;
    if (prec > 9) {
        prec = 9;
    }
    for (i = 0; i < prec; i++) {
        scale *= 10;
    }
    if (v >= 1.8e19) {
        memcpy(out, "inf", 3);
        return 3;
    }
    whole = (unsigned long long)v;
    frac = (unsigned long long)((v - (double)whole) * (double)scale + 0.5);
    if (frac >= scale) {
        whole++;
        frac -= scale;
    }
    n = zt_utoa(out, whole, 10, 0);
    if (prec > 0) {
        char digits[16];
        size_t dn = zt_utoa(digits, frac, 10, 0);
        out[n++] = '.';
        for (i = (int)dn; i < prec; i++) {
            out[n++] = '0';
        }
        memcpy(out + n, digits, dn);
        n += dn;
    }
    return n;
}

int ztest_vsnprintf(char* buf, size_t cap, const char* fmt, va_list ap) {
    zt_sink s = {buf, cap, 0};
    while (*fmt) {
        int left = 0, zero = 0, plus = 0, space = 0, width = 0, prec = -1;
        int size = 0; // 0 int, 1 long, 2 long long, 3 size_t, -1 short, -2 char
        char body[48];
        size_t body_len = 0;
        const char* prefix = "";
        char c = *fmt++;
        if (c != '%') {
            zt_put(&s, c);
            continue;
        }
        for (;; fmt++) {
            if (*fmt == '-') {
                left = 1;
            } else if (*fmt == '0') {
                zero = 1;
            } else if (*fmt == '+') {
                plus = 1;
            } else if (*fmt == ' ') {
                space = 1;
            } else {
                break;
            }
        }
        if (*fmt == '*') {
            width = va_arg(ap, int);
            if (width < 0) {
                left = 1;
                width = -width;
            }
            fmt++;
        } else {
            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt++ - '0');
            }
        }
        if (*fmt == '.') {
            fmt++;
            prec = 0;
            if (*fmt == '*') {
                prec = va_arg(ap, int);
                fmt++;
            } else {
                while (*fmt >= '0' && *fmt <= '9') {
                    prec = prec * 10 + (*fmt++ - '0');
                }
            }
        }
        for (;; fmt++) {
            if (*fmt == 'l') {
                size = size == 1 ? 2 : 1;
            } else if (*fmt == 'h') {
                size = size == -1 ? -2 : -1;
            } else if (*fmt == 'z' || *fmt == 't') {
                size = 3;
            } else if (*fmt == 'j') {
                size = 2;
            } else {
                break;
            }
        }
        c = *fmt;
        if (c) {
            fmt++;
        }
        switch (c) {
        case 'd':
        case 'i': {
            long long v;
            unsigned long long u;
            if (size == 2) {
                v = va_arg(ap, long long);
            } else if (size == 1) {
                v = va_arg(ap, long);
            } else if (size == 3) {
                v = (long long)va_arg(ap, intptr_t);
            } else {
                v = va_arg(ap, int);
                v = size == -1 ? (short)v : size == -2 ? (signed char)v : v;
            }
            u = v < 0 ? 0ull - (unsigned long long)v : (unsigned long long)v;
            prefix = v < 0 ? "-" : plus ? "+" : space ? " " : "";
            body_len = zt_utoa(body, u, 10, 0);
            zt_put_padded(&s, prefix, body, body_len, width, left, zero);
            break;
        }
        case 'u':
        case 'x':
        case 'X':
        case 'o': {
            unsigned long long v;
            if (size == 2) {
                v = va_arg(ap, unsigned long long);
            } else if (size == 1) {
                v = va_arg(ap, unsigned long);
            } else if (size == 3) {
                v = va_arg(ap, size_t);
            } else {
                v = va_arg(ap, unsigned int);
                v = size == -1   ? (unsigned short)v
                    : size == -2 ? (unsigned char)v
                                 : v;
            }
            body_len = zt_utoa(body, v,
                               c == 'o'   ? 8
                               : c == 'u' ? 10
                                          : 16,
                               c == 'X');
            zt_put_padded(&s, "", body, body_len, width, left, zero);
            break;
        }
        case 'p': {
            uintptr_t v = (uintptr_t)va_arg(ap, void*);
            body_len = zt_utoa(body, v, 16, 0);
            zt_put_padded(&s, "0x", body, body_len, width, left, 0);
            break;
        }
        case 'c':
            body[0] = (char)va_arg(ap, int);
            zt_put_padded(&s, "", body, 1, width, left, 0);
            break;
        case 's': {
            const char* str = va_arg(ap, const char*);
            size_t len;
            if (!str) {
                str = "(null)";
            }
            len = strlen(str);
            if (prec >= 0 && (size_t)prec < len) {
                len = (size_t)prec;
            }
            zt_put_padded(&s, "", str, len, width, left, 0);
            break;
        }
        case 'f':
        case 'F':
        case 'e':
        case 'g': {
            double v = va_arg(ap, double);
            if (v != v) {
                zt_put_padded(&s, "", "nan", 3, width, left, 0);
                break;
            }
            prefix = v < 0 ? "-" : plus ? "+" : space ? " " : "";
            body_len = zt_ftoa(body, v < 0 ? -v : v, prec < 0 ? 6 : prec);
            zt_put_padded(&s, prefix, body, body_len, width, left, zero);
            break;
        }
        case '%':
            zt_put(&s, '%');
            break;
        default:
            zt_put(&s, '%');
            if (c) {
                zt_put(&s, c);
            }
            break;
        }
    }
    if (cap) {
        buf[s.len < cap ? s.len : cap - 1] = '\0';
    }
    return (int)s.len;
}

int ztest_snprintf(char* buf, size_t n, const char* fmt, ...) {
    va_list ap;
    int r;
    va_start(ap, fmt);
    r = ztest_vsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return r;
}

// ---------------------------------------------------------------------------
// State and output
// ---------------------------------------------------------------------------

enum { ZT_PLAIN, ZT_COLOR, ZT_EMOJI };
enum { ZT_PASS = 1, ZT_FAIL, ZT_SKIP };
enum { ZT_JMP_ABORT = 1, ZT_JMP_SKIP, ZT_JMP_TERMINATE };

static ztest_case* zt_cases;
static ztest_hook* zt_hooks;
static ztest_case* zt_current;
static int zt_failed;
static int zt_skipped;
static int zt_terminated;
static int zt_verbose;
static int zt_mode = ZT_PLAIN;
static zt_jmp_buf zt_jmp;
static int zt_jmp_valid;
static int zt_frontbuffer_missing;
static char zt_log[ZTEST_LOG_SIZE];
static size_t zt_log_len;
static int zt_log_bol = 1;
static char zt_tags[256] = ZTEST_TAGS;
static char zt_read_root[128];
static char zt_write_root[128];
static int zt_roots_set;

static void zt_out(const char* s) { zt_plat_write(s, strlen(s)); }

static void zt_outf(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    ztest_vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    zt_out(buf);
}

static void zt_log_put(const char* s, size_t n) {
    size_t room = sizeof(zt_log) - 1 - zt_log_len;
    if (n > room) {
        n = room;
    }
    memcpy(zt_log + zt_log_len, s, n);
    zt_log_len += n;
    zt_log[zt_log_len] = '\0';
}

// Appends text to the per-test log, indenting every line by three spaces.
static void zt_log_text(const char* s, int red) {
    const char* red_on = red && zt_mode != ZT_PLAIN ? "\x1b[31m" : "";
    const char* red_off = red && zt_mode != ZT_PLAIN ? "\x1b[0m" : "";
    if (!zt_current) {
        zt_out(s);
        return;
    }
    while (*s) {
        const char* end = strchr(s, '\n');
        size_t n = end ? (size_t)(end - s) : strlen(s);
        if (zt_log_bol) {
            zt_log_put("   ", 3);
        }
        zt_log_put(red_on, strlen(red_on));
        zt_log_put(s, n);
        zt_log_put(red_off, strlen(red_off));
        s += n;
        zt_log_bol = 0;
        if (*s == '\n') {
            zt_log_put("\n", 1);
            zt_log_bol = 1;
            s++;
        }
    }
}

static void zt_logv(int red, const char* fmt, va_list ap) {
    char buf[1024];
    ztest_vsnprintf(buf, sizeof(buf), fmt, ap);
    zt_log_text(buf, red);
}

void zprintf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    zt_logv(0, fmt, ap);
    va_end(ap);
}

void zerrorf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    zt_logv(1, fmt, ap);
    va_end(ap);
}

// Starts a "Line N  " entry; returns the width of that prefix for alignment.
static int zt_line_start(int line) {
    char buf[32];
    if (!zt_log_bol) {
        zt_log_text("\n", 0);
    }
    buf[0] = '\0';
    if (line > 0) {
        ztest_snprintf(buf, sizeof(buf), "Line %d  ", line);
    }
    zt_log_text(buf, 0);
    return (int)strlen(buf);
}

static void zt_line(int line, const char* fmt, ...) {
    va_list ap;
    zt_line_start(line);
    va_start(ap, fmt);
    zt_logv(0, fmt, ap);
    va_end(ap);
    zt_log_text("\n", 0);
}

static void zt_cont(int indent, const char* fmt, ...) {
    va_list ap;
    char pad[32];
    if (indent > (int)sizeof(pad) - 1) {
        indent = (int)sizeof(pad) - 1;
    }
    memset(pad, ' ', (size_t)indent);
    pad[indent] = '\0';
    zt_log_text(pad, 0);
    va_start(ap, fmt);
    zt_logv(0, fmt, ap);
    va_end(ap);
    zt_log_text("\n", 0);
}

#if defined(__GNUC__)
__attribute__((noinline))
#endif
static void zt_jump(int code) {
    (void)code;
    if (zt_jmp_valid) {
        ZT_LONGJMP(zt_jmp);
    }
}

static int zt_after_fail(int abort) {
    zt_failed = 1;
    if (abort) {
        zt_jump(ZT_JMP_ABORT);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Registry, targets, control
// ---------------------------------------------------------------------------

static int zt_case_order(const ztest_case* a, const ztest_case* b) {
    int c = a->file == b->file ? 0 : strcmp(a->file, b->file);
    return c ? c : a->line - b->line;
}

// Cases of one file usually register in order, so resume from the last one.
void ztest__register(ztest_case* c) {
    static ztest_case* last;
    ztest_case** p = &zt_cases;
    if (last && zt_case_order(last, c) <= 0) {
        p = &last->next;
    }
    while (*p && zt_case_order(*p, c) <= 0) {
        p = &(*p)->next;
    }
    c->next = *p;
    *p = c;
    last = c;
}

void ztest__register_hook(ztest_hook* h) {
    h->next = zt_hooks;
    zt_hooks = h;
}

static ztest_hook* zt_find_hook(const char* group, int kind) {
    ztest_hook* h;
    for (h = zt_hooks; h; h = h->next) {
        if (h->kind == kind && !strcmp(h->group, group)) {
            return h;
        }
    }
    return NULL;
}

const char* ztest_target(void) { return ZTEST_TARGET; }

void ztest_add_tag(const char* tag) {
    size_t len = strlen(zt_tags);
    if (len + strlen(tag) + 2 > sizeof(zt_tags) || ztest_is_target(tag)) {
        return;
    }
    if (len) {
        zt_tags[len++] = ';';
    }
    strcpy(zt_tags + len, tag);
}

static int zt_list_has(const char* list, const char* word, size_t word_len) {
    while (*list) {
        size_t n = strcspn(list, ";,");
        if (n == word_len && !strncmp(list, word, n)) {
            return 1;
        }
        list += n;
        if (*list) {
            list++;
        }
    }
    return 0;
}

int ztest_is_target(const char* list) {
    const char* tags = zt_tags;
    if (zt_list_has(list, ZTEST_TARGET, strlen(ZTEST_TARGET))) {
        return 1;
    }
    while (*tags) {
        size_t n = strcspn(tags, ";,");
        if (n && zt_list_has(list, tags, n)) {
            return 1;
        }
        tags += n;
        if (*tags) {
            tags++;
        }
    }
    return 0;
}

static void zt_set_root(char* dst, size_t cap, const char* root) {
    size_t len = strlen(root);
    if (len + 2 > cap) {
        return;
    }
    strcpy(dst, root);
    if (len && root[len - 1] != '/' && root[len - 1] != ':') {
        strcat(dst, "/");
    }
}

void ztest_set_read_root(const char* root) {
    zt_set_root(zt_read_root, sizeof(zt_read_root), root);
    zt_roots_set = 1;
}

void ztest_set_write_root(const char* root) {
    zt_set_root(zt_write_root, sizeof(zt_write_root), root);
    zt_roots_set = 1;
}

static void* zt_read_file(const char* path, size_t* size) {
    char full[384];
    ztest_snprintf(full, sizeof(full), "%s%s", zt_read_root, path);
    return zt_plat_read_file(full, size);
}

static int zt_write_file(const char* path, const void* data, size_t size) {
    char full[384];
    ztest_snprintf(full, sizeof(full), "%s%s", zt_write_root, path);
    return zt_plat_write_file(full, data, size);
}

static void zt_remove_file(const char* path) {
    char full[384];
    ztest_snprintf(full, sizeof(full), "%s%s", zt_write_root, path);
    zt_plat_remove_file(full);
}

static void zt_log_message(
    int line, const char* fallback, const char* fmt, va_list ap) {
    char buf[512];
    ztest_vsnprintf(buf, sizeof(buf), fmt, ap);
    zt_line(line, "%s", *buf ? buf : fallback);
}

void ztest__fail(int line, int abort, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    zt_log_message(line, "Failed", fmt, ap);
    va_end(ap);
    zt_after_fail(abort);
}

void ztest__skip(int line, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    zt_log_message(line, "Skipped", fmt, ap);
    va_end(ap);
    zt_skipped = 1;
    zt_jump(ZT_JMP_SKIP);
}

void ztest__skip_targets(int line, const char* list) {
    if (ztest_is_target(list)) {
        zt_line(line, "Skipped on %s", list);
        zt_skipped = 1;
        zt_jump(ZT_JMP_SKIP);
    }
}

void ztest__terminate(int line, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    zt_log_message(line, "Terminated", fmt, ap);
    va_end(ap);
    zt_failed = 1;
    zt_terminated = 1;
    zt_jump(ZT_JMP_TERMINATE);
}

// ---------------------------------------------------------------------------
// Scalar assertions
// ---------------------------------------------------------------------------

static const char* const zt_ops[] = {"", "!= ", "> ", ">= ", "< ", "<= "};

static int zt_holds(int op, int c) {
    switch (op) {
    case ZT_EQ:
        return c == 0;
    case ZT_NE:
        return c != 0;
    case ZT_GT:
        return c > 0;
    case ZT_GE:
        return c >= 0;
    case ZT_LT:
        return c < 0;
    default:
        return c <= 0;
    }
}

int ztest__s(int line, int op, int abort, long exp, long act) {
    if (zt_holds(op, (act > exp) - (act < exp))) {
        return 1;
    }
    zt_line(line, "Expected: %s%ld  Actual: %ld", zt_ops[op], exp, act);
    return zt_after_fail(abort);
}

static void zt_fmt_u(char* buf, size_t n, unsigned long v) {
    if (v > 9) {
        ztest_snprintf(buf, n, "%lu (0x%lX)", v, v);
    } else {
        ztest_snprintf(buf, n, "%lu", v);
    }
}

int ztest__u(
    int line, int op, int abort, unsigned long exp, unsigned long act) {
    char e[48], a[48];
    if (zt_holds(op, (act > exp) - (act < exp))) {
        return 1;
    }
    zt_fmt_u(e, sizeof(e), exp);
    zt_fmt_u(a, sizeof(a), act);
    zt_line(line, "Expected: %s%s  Actual: %s", zt_ops[op], e, a);
    return zt_after_fail(abort);
}

static void zt_fmt_c(char* buf, size_t n, unsigned char c) {
    if (c >= 0x20 && c < 0x7F) {
        ztest_snprintf(buf, n, "'%c' (0x%02X)", c, c);
    } else {
        ztest_snprintf(buf, n, "0x%02X", c);
    }
}

int ztest__c(int line, int op, int abort, char exp, char act) {
    unsigned char e = (unsigned char)exp, a = (unsigned char)act;
    char es[16], as[16];
    if (zt_holds(op, (a > e) - (a < e))) {
        return 1;
    }
    zt_fmt_c(es, sizeof(es), e);
    zt_fmt_c(as, sizeof(as), a);
    zt_line(line, "Expected: %s%s  Actual: %s", zt_ops[op], es, as);
    return zt_after_fail(abort);
}

static void zt_fmt_p(char* buf, size_t n, uintptr_t v) {
    if (v) {
        ztest_snprintf(buf, n, "0x%llX", (unsigned long long)v);
    } else {
        ztest_snprintf(buf, n, "NULL");
    }
}

int ztest__p(int line, int op, int abort, const void* exp, const void* act) {
    uintptr_t e = (uintptr_t)exp, a = (uintptr_t)act;
    char es[24], as[24];
    if (zt_holds(op, (a > e) - (a < e))) {
        return 1;
    }
    zt_fmt_p(es, sizeof(es), e);
    zt_fmt_p(as, sizeof(as), a);
    zt_line(line, "Expected: %s%s  Actual: %s", zt_ops[op], es, as);
    return zt_after_fail(abort);
}

int ztest__up(int line, int op, int abort, uintptr_t exp, uintptr_t act) {
    if (zt_holds(op, (act > exp) - (act < exp))) {
        return 1;
    }
    zt_line(line, "Expected: %s0x%llX  Actual: 0x%llX", zt_ops[op],
            (unsigned long long)exp, (unsigned long long)act);
    return zt_after_fail(abort);
}

// ---------------------------------------------------------------------------
// Array and string diffs
// ---------------------------------------------------------------------------

typedef struct {
    const unsigned char* e;
    size_t elen;
    const unsigned char* a;
    size_t alen;
    int is_str;
} zt_diff;

static int zt_differs(const zt_diff* d, size_t i) {
    int e_in = i < d->elen, a_in = i < d->alen;
    return e_in != a_in || (e_in && d->e[i] != d->a[i]);
}

static void zt_window_side(
    char* out, size_t cap, const zt_diff* d, const unsigned char* p, size_t len,
    size_t from, size_t off, size_t shown, int long_run, size_t after) {
    zt_sink s = {out, cap, 0};
    size_t i;
    char hex[4];
    const char* more = d->is_str ? "..." : " ...";
    if (d->is_str) {
        zt_put(&s, '"');
    }
    for (i = from; i < off + shown + after; i++) {
        if (!d->is_str && i > from) {
            zt_put(&s, ' ');
        }
        if (i == off) {
            zt_put(&s, '[');
        }
        if (i < len && d->is_str) {
            zt_put(&s, p[i] >= 0x20 && p[i] < 0x7F ? (char)p[i] : '.');
        } else if (i < len) {
            ztest_snprintf(hex, sizeof(hex), "%02X", p[i]);
            zt_put(&s, hex[0]);
            zt_put(&s, hex[1]);
        }
        if (i + 1 == off + shown) {
            const char* m = long_run && off + shown < len ? more : "";
            while (*m) {
                zt_put(&s, *m++);
            }
            zt_put(&s, ']');
        }
    }
    if (d->is_str) {
        zt_put(&s, '"');
    }
    out[s.len < cap ? s.len : cap - 1] = '\0';
}

// Prints the header line and the aligned Expected/Actual window.
static void zt_window(
    int line, const zt_diff* d, size_t off, const char* header) {
    size_t total = d->elen > d->alen ? d->elen : d->alen;
    size_t ctx = d->is_str ? 12 : 4;
    size_t max_shown = d->is_str ? 16 : 4;
    size_t run = 0, shown, before, after = 0, i;
    int long_run, indent;
    char e[128], a[128];
    while (off + run < total && zt_differs(d, off + run) && run <= max_shown) {
        run++;
    }
    long_run = run > max_shown;
    shown = long_run ? max_shown : run;
    before = off < ctx ? off : ctx;
    if (!long_run) {
        for (i = off + shown; i < total && after < ctx; i++) {
            after++;
        }
    }
    zt_window_side(e, sizeof(e), d, d->e, d->elen, off - before, off, shown,
                   long_run, after);
    zt_window_side(a, sizeof(a), d, d->a, d->alen, off - before, off, shown,
                   long_run, after);
    indent = zt_line_start(line);
    zt_log_text(header, 0);
    zt_log_text("\n", 0);
    zt_cont(indent, "Expected: %s", e);
    zt_cont(indent, "Actual:   %s", a);
}

int ztest__bytes(
    int line, int op, int abort, const void* exp, const void* act, size_t len) {
    zt_diff d = {exp, len, act, len, 0};
    size_t off, ndiff = 0, i;
    int c = memcmp(act, exp, len);
    char header[96];
    if (zt_holds(op, (c > 0) - (c < 0))) {
        return 1;
    }
    for (off = 0; off < len && !zt_differs(&d, off); off++) {
    }
    if (off == len) {
        zt_line(line, "Expected: act %sexp  Actual: all %u bytes identical",
                zt_ops[op], (unsigned)len);
        return zt_after_fail(abort);
    }
    for (i = off; i < len; i++) {
        ndiff += zt_differs(&d, i);
    }
    ztest_snprintf(
        header, sizeof(header), "%sOffset: 0x%06X  (%u %s)",
        op == ZT_EQ ? "" : zt_ops[op], (unsigned)off, (unsigned)ndiff,
        ndiff == 1 ? "byte differs" : "bytes differ");
    zt_window(line, &d, off, header);
    return zt_after_fail(abort);
}

int ztest__str(int line, int op, int abort, const char* exp, const char* act) {
    int c;
    if (!exp || !act) {
        c = (act != NULL) - (exp != NULL);
    } else {
        c = strcmp(act, exp);
        c = (c > 0) - (c < 0);
    }
    if (zt_holds(op, c)) {
        return 1;
    }
    if (!exp || !act) {
        zt_line(line, "Expected: %s%s%s%s  Actual: %s%s%s", zt_ops[op],
                exp ? "\"" : "", exp ? exp : "NULL", exp ? "\"" : "",
                act ? "\"" : "", act ? act : "NULL", act ? "\"" : "");
        return zt_after_fail(abort);
    }
    {
        size_t elen = strlen(exp), alen = strlen(act), off;
        char digits[16];
        int width = ztest_snprintf(digits, sizeof(digits), "%d", line);
        // "   Line N  Expected: <op>"..."  Actual: "..."" must fit in 80.
        if (3 + 5 + width + 2 + 10 + (int)strlen(zt_ops[op]) + (int)elen + 2 +
                10 + (int)alen + 2 <=
            80) {
            zt_line(line, "Expected: %s\"%s\"  Actual: \"%s\"", zt_ops[op], exp,
                    act);
        } else {
            zt_diff d = {(const unsigned char*)exp, elen,
                         (const unsigned char*)act, alen, 1};
            char header[96];
            for (off = 0; !zt_differs(&d, off) && off < elen; off++) {
            }
            ztest_snprintf(
                header, sizeof(header), "%sOffset: %u  Length: %u vs %u",
                op == ZT_EQ ? "" : zt_ops[op], (unsigned)off, (unsigned)elen,
                (unsigned)alen);
            zt_window(line, &d, off, header);
        }
    }
    return zt_after_fail(abort);
}

// ---------------------------------------------------------------------------
// Images
// ---------------------------------------------------------------------------

#if defined(__GNUC__)
// stb reports its unused static declarations at the end of the file.
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
static int zt_stb_line;
static void zt_stb_assert(const char* file, int line, const char* expr) {
    ztest__fail(
        zt_stb_line, 1, "%s:%d: assertion failed: %s", file, line, expr);
}

#define STBI_MALLOC(sz) ZTEST_MALLOC(sz)
#define STBI_REALLOC(p, sz) ZTEST_REALLOC(p, sz)
#define STBI_FREE(p) ZTEST_FREE(p)
#define STBI_ASSERT(x)                                                         \
    ((x) ? (void)0 : zt_stb_assert("stb_image.h", __LINE__, #x))
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_FAILURE_STRINGS
#define STBI_NO_THREAD_LOCALS
#include "stb_image.h"
#define STBIW_MALLOC(sz) ZTEST_MALLOC(sz)
#define STBIW_REALLOC(p, sz) ZTEST_REALLOC(p, sz)
#define STBIW_FREE(p) ZTEST_FREE(p)
#define STBIW_ASSERT(x)                                                        \
    ((x) ? (void)0 : zt_stb_assert("stb_image_write.h", __LINE__, #x))
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "stb_image_write.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

typedef struct zt_block {
    void* ptr;
    struct zt_block* next;
} zt_block;

static zt_block* zt_arena;

static void zt_arena_add(void* ptr) {
    zt_block* b = ZTEST_MALLOC(sizeof(zt_block));
    if (!b) {
        return;
    }
    b->ptr = ptr;
    b->next = zt_arena;
    zt_arena = b;
}

static void zt_arena_release(void* ptr) {
    zt_block** p;
    for (p = &zt_arena; *p; p = &(*p)->next) {
        if ((*p)->ptr == ptr) {
            zt_block* b = *p;
            *p = b->next;
            ZTEST_FREE(b->ptr);
            ZTEST_FREE(b);
            return;
        }
    }
}

static void zt_arena_free_all(void) {
    while (zt_arena) {
        zt_block* b = zt_arena;
        zt_arena = b->next;
        ZTEST_FREE(b->ptr);
        ZTEST_FREE(b);
    }
}

static int zt_bpp(zimage_format f) {
    switch (f) {
    case ZIMAGE_RGB888:
        return 3;
    case ZIMAGE_RGBA8888:
        return 4;
    case ZIMAGE_R5G5B5:
    case ZIMAGE_R5G6B5:
    case ZIMAGE_RGBA5551_BE:
        return 2;
    default:
        return 0;
    }
}

zimage zimage_alloc(int width, int height, zimage_format format) {
    zimage img = {0, 0, ZIMAGE_NONE, 0, NULL};
    size_t size = (size_t)width * (size_t)height * (size_t)zt_bpp(format);
    if (width <= 0 || height <= 0 || !size) {
        return img;
    }
    img.data = ZTEST_MALLOC(size);
    if (!img.data) {
        return img;
    }
    memset(img.data, 0, size);
    zt_arena_add(img.data);
    img.width = width;
    img.height = height;
    img.format = format;
    img.stride = width * zt_bpp(format);
    return img;
}

void zimage_free(zimage* img) {
    if (img->data) {
        zt_arena_release(img->data);
    }
    img->data = NULL;
    img->width = img->height = img->stride = 0;
    img->format = ZIMAGE_NONE;
}

// Returns the pixel as 5-bit channels, the precision the comparators use.
static void zt_px5(const zimage* img, int x, int y, unsigned char c[3]) {
    const unsigned char* p = img->data + (size_t)y * (size_t)img->stride +
                             (size_t)x * (size_t)zt_bpp(img->format);
    unsigned v;
    switch (img->format) {
    case ZIMAGE_RGB888:
    case ZIMAGE_RGBA8888:
        c[0] = p[0] >> 3;
        c[1] = p[1] >> 3;
        c[2] = p[2] >> 3;
        break;
    case ZIMAGE_R5G5B5:
        v = p[0] | (p[1] << 8);
        c[0] = v & 31;
        c[1] = (v >> 5) & 31;
        c[2] = (v >> 10) & 31;
        break;
    case ZIMAGE_R5G6B5:
        v = p[0] | (p[1] << 8);
        c[0] = v & 31;
        c[1] = (v >> 6) & 31;
        c[2] = (v >> 11) & 31;
        break;
    case ZIMAGE_RGBA5551_BE:
        v = (p[0] << 8) | p[1];
        c[0] = (v >> 11) & 31;
        c[1] = (v >> 6) & 31;
        c[2] = (v >> 1) & 31;
        break;
    default:
        c[0] = c[1] = c[2] = 0;
        break;
    }
}

void zimage_get_rgb(const zimage* img, int x, int y, unsigned char rgb[3]) {
    const unsigned char* p = img->data + (size_t)y * (size_t)img->stride +
                             (size_t)x * (size_t)zt_bpp(img->format);
    if (img->format == ZIMAGE_RGB888 || img->format == ZIMAGE_RGBA8888) {
        rgb[0] = p[0];
        rgb[1] = p[1];
        rgb[2] = p[2];
        return;
    }
    zt_px5(img, x, y, rgb);
    if (img->format == ZIMAGE_R5G6B5) {
        unsigned g6 = ((p[0] | (p[1] << 8)) >> 5) & 63;
        rgb[1] = (unsigned char)((g6 << 2) | (g6 >> 4));
    } else {
        rgb[1] = (unsigned char)((rgb[1] << 3) | (rgb[1] >> 2));
    }
    rgb[0] = (unsigned char)((rgb[0] << 3) | (rgb[0] >> 2));
    rgb[2] = (unsigned char)((rgb[2] << 3) | (rgb[2] >> 2));
}

void zimage_set_rgb(zimage* img, int x, int y, const unsigned char rgb[3]) {
    unsigned char* p = img->data + (size_t)y * (size_t)img->stride +
                       (size_t)x * (size_t)zt_bpp(img->format);
    unsigned r5 = rgb[0] >> 3, g5 = rgb[1] >> 3, b5 = rgb[2] >> 3, v;
    switch (img->format) {
    case ZIMAGE_RGBA8888:
        p[3] = 0xFF;
        // fallthrough
    case ZIMAGE_RGB888:
        p[0] = rgb[0];
        p[1] = rgb[1];
        p[2] = rgb[2];
        break;
    case ZIMAGE_R5G5B5:
        v = r5 | (g5 << 5) | (b5 << 10);
        p[0] = (unsigned char)v;
        p[1] = (unsigned char)(v >> 8);
        break;
    case ZIMAGE_R5G6B5:
        v = r5 | ((unsigned)(rgb[1] >> 2) << 5) | (b5 << 11);
        p[0] = (unsigned char)v;
        p[1] = (unsigned char)(v >> 8);
        break;
    case ZIMAGE_RGBA5551_BE:
        v = (r5 << 11) | (g5 << 6) | (b5 << 1) | 1;
        p[0] = (unsigned char)(v >> 8);
        p[1] = (unsigned char)v;
        break;
    default:
        break;
    }
}

zimage zimage_png(const char* path) {
    zimage img = {0, 0, ZIMAGE_NONE, 0, NULL};
    size_t size = 0;
    unsigned char* file = zt_read_file(path, &size);
    int w, h, comp, req;
    unsigned char* pixels;
    if (!file) {
        return img;
    }
    if (!stbi_info_from_memory(file, (int)size, &w, &h, &comp)) {
        ZTEST_FREE(file);
        return img;
    }
    req = comp == 2 || comp == 4 ? 4 : 3;
    pixels = stbi_load_from_memory(file, (int)size, &w, &h, &comp, req);
    ZTEST_FREE(file);
    if (!pixels) {
        return img;
    }
    zt_arena_add(pixels);
    img.width = w;
    img.height = h;
    img.format = req == 4 ? ZIMAGE_RGBA8888 : ZIMAGE_RGB888;
    img.stride = w * req;
    img.data = pixels;
    return img;
}

static void zt_exp_paths(
    const char* name, char* target_path, char* plain_path, size_t cap) {
    ztest_snprintf(target_path, cap, "expected/%s.%s.png", name, ZTEST_TARGET);
    ztest_snprintf(plain_path, cap, "expected/%s.png", name);
}

zimage zimage_exp_png(const char* name) {
    char target_path[256], plain_path[256];
    zimage img;
    zt_exp_paths(name, target_path, plain_path, sizeof(target_path));
    img = zimage_png(target_path);
    return img.data ? img : zimage_png(plain_path);
}

#if defined(__PSP__)
static zimage zt_native_frontbuffer(void) {
    void* top = NULL;
    int stride = 0, format = 0, x, y;
    zimage img = {0, 0, ZIMAGE_NONE, 0, NULL};
    const int w = 480, h = 272;
    sceDisplayGetFrameBuf(&top, &stride, &format, PSP_DISPLAY_SETBUF_IMMEDIATE);
    if (!top || stride <= 0) {
        return img;
    }
    top = (void*)((uintptr_t)top | 0x40000000u); // uncached mirror
    if (format == PSP_DISPLAY_PIXEL_FORMAT_8888) {
        img = zimage_alloc(w, h, ZIMAGE_RGBA8888);
        for (y = 0; img.data && y < h; y++) {
            memcpy(img.data + y * img.stride,
                   (unsigned char*)top + y * stride * 4, (size_t)w * 4);
        }
    } else if (format == PSP_DISPLAY_PIXEL_FORMAT_4444) {
        img = zimage_alloc(w, h, ZIMAGE_RGB888);
        for (y = 0; img.data && y < h; y++) {
            const unsigned short* src = (const unsigned short*)top + y * stride;
            for (x = 0; x < w; x++) {
                unsigned char rgb[3];
                rgb[0] = (unsigned char)((src[x] & 15) * 17);
                rgb[1] = (unsigned char)(((src[x] >> 4) & 15) * 17);
                rgb[2] = (unsigned char)(((src[x] >> 8) & 15) * 17);
                zimage_set_rgb(&img, x, y, rgb);
            }
        }
    } else {
        img = zimage_alloc(
            w, h,
            format == PSP_DISPLAY_PIXEL_FORMAT_565 ? ZIMAGE_R5G6B5
                                                   : ZIMAGE_R5G5B5);
        for (y = 0; img.data && y < h; y++) {
            memcpy(img.data + y * img.stride,
                   (unsigned char*)top + y * stride * 2, (size_t)w * 2);
        }
    }
    return img;
}
#define ZT_HAS_NATIVE_FRONTBUFFER
#endif

#ifdef ZT_PS1
static int zt_disp_x, zt_disp_y;

void zimage_set_display_origin(int x, int y) {
    zt_disp_x = x;
    zt_disp_y = y;
}

// VRAM to CPU by polling GPUREAD, so it works without DMA or callbacks.
zimage zimage_vram(int x, int y, int w, int h) {
    zimage img = zimage_alloc(w, h, ZIMAGE_R5G5B5);
    unsigned n, i;
    if (!img.data) {
        return img;
    }
    while (ZT_HW32(0x1F8010A8) & (1u << 24)) { // GPU DMA still busy
    }
    while (!(ZT_HW32(0x1F801814) & (1u << 26))) {
    }
    ZT_HW32(0x1F801810) = 0x01000000; // flush the texture cache
    while (!(ZT_HW32(0x1F801814) & (1u << 26))) {
    }
    ZT_HW32(0x1F801810) = 0xC0000000;
    ZT_HW32(0x1F801810) = ((unsigned)y << 16) | ((unsigned)x & 0xFFFF);
    ZT_HW32(0x1F801810) = ((unsigned)h << 16) | ((unsigned)w & 0xFFFF);
    while (!(ZT_HW32(0x1F801814) & (1u << 27))) {
    }
    n = ((unsigned)w * (unsigned)h + 1) / 2;
    for (i = 0; i < n; i++) {
        unsigned v = ZT_HW32(0x1F801810);
        unsigned px = i * 2;
        img.data[px * 2] = (unsigned char)v;
        img.data[px * 2 + 1] = (unsigned char)(v >> 8);
        if (px + 1 < (unsigned)w * (unsigned)h) {
            img.data[px * 2 + 2] = (unsigned char)(v >> 16);
            img.data[px * 2 + 3] = (unsigned char)(v >> 24);
        }
    }
    return img;
}

// The display start (GP1 05h) cannot be read back, only its size.
static zimage zt_native_frontbuffer(void) {
    static const int widths[] = {256, 320, 512, 640};
    unsigned stat = ZT_HW32(0x1F801814);
    int w = stat & (1u << 16) ? 368 : widths[(stat >> 17) & 3];
    int h = (stat & (1u << 19)) && (stat & (1u << 22)) ? 480 : 240;
    return zimage_vram(zt_disp_x, zt_disp_y, w, h);
}
#define ZT_HAS_NATIVE_FRONTBUFFER
#endif

#ifdef ZT_N64
// Reads whatever the VI is scanning out, so it does not depend on display.h.
static zimage zt_native_frontbuffer(void) {
    volatile unsigned* vi = (volatile unsigned*)0xA4400000;
    zimage img = {0, 0, ZIMAGE_NONE, 0, NULL};
    unsigned type = vi[0] & 3, origin = vi[1] & 0xFFFFFF,
             stride = vi[2] & 0xFFF;
    unsigned vvideo = vi[10], yscale = vi[13] & 0xFFF, xscale = vi[12] & 0xFFF;
    int w, h, y, bpp = type == 3 ? 4 : 2;
    const unsigned char* src;
    if (type < 2 || !stride) {
        return img;
    }
    w = (int)((((vi[9] & 0x3FF) - ((vi[9] >> 16) & 0x3FF)) * xscale) / 1024);
    h = (int)(((((vvideo & 0x3FF) - ((vvideo >> 16) & 0x3FF)) / 2) * yscale) /
              1024);
    if (w <= 0 || w > (int)stride) {
        w = (int)stride;
    }
    img = zimage_alloc(w, h, type == 3 ? ZIMAGE_RGBA8888 : ZIMAGE_RGBA5551_BE);
    src = (const unsigned char*)(uintptr_t)(0xA0000000u | origin);
    for (y = 0; img.data && y < h; y++) {
        memcpy(
            img.data + y * img.stride, src + y * stride * bpp, (size_t)w * bpp);
    }
    return img;
}
#define ZT_HAS_NATIVE_FRONTBUFFER
#endif

#ifdef ZT_NDS
// Captures the main engine output into VRAM bank D, whose mapping is restored.
static zimage zt_native_frontbuffer(void) {
    zimage img = zimage_alloc(256, 192, ZIMAGE_R5G5B5);
    unsigned char bank = VRAM_D_CR;
    int y;
    if (!img.data) {
        return img;
    }
    VRAM_D_CR = VRAM_ENABLE | VRAM_D_LCD;
    swiWaitForVBlank();
    REG_DISPCAPCNT = DCAP_ENABLE | DCAP_MODE(DCAP_MODE_A) |
                     DCAP_SRC_A(DCAP_SRC_A_COMPOSITED) |
                     DCAP_SIZE(DCAP_SIZE_256x192) | DCAP_BANK(DCAP_BANK_VRAM_D);
    swiWaitForVBlank();
    REG_DISPCAPCNT = 0;
    for (y = 0; y < 192; y++) {
        memcpy(img.data + y * img.stride, (const void*)&VRAM_D[y * 256], 512);
    }
    VRAM_D_CR = bank;
    return img;
}
#define ZT_HAS_NATIVE_FRONTBUFFER
#endif

#if defined(_MSC_VER)
#pragma comment(linker, "/alternatename:" ZTEST__SYM_PREFIX                    \
                        "zimage_frontbuffer=" ZTEST__SYM_PREFIX                \
                        "ztest__frontbuffer_default")
zimage ztest__frontbuffer_default(void)
#else
__attribute__((weak)) zimage zimage_frontbuffer(void)
#endif
{
#ifdef ZT_HAS_NATIVE_FRONTBUFFER
    return zt_native_frontbuffer();
#else
    zimage img = {0, 0, ZIMAGE_NONE, 0, NULL};
    zt_frontbuffer_missing = 1;
    return img;
#endif
}

void zimage_cmp_r5g5b5(const zimage* exp, const zimage* act,
                       const zimage_cmp* cmp, zimage_result* res) {
    int x0 = 0, y0 = 0, w = exp->width, h = exp->height, x, y, i;
    int first = 1;
    memset(res, 0, sizeof(*res));
    if (cmp->w) {
        x0 = cmp->x;
        y0 = cmp->y;
        w = cmp->w;
        h = cmp->h;
        if (x0 < 0 || y0 < 0 || h <= 0 || x0 + w > exp->width ||
            y0 + h > exp->height || x0 + w > act->width ||
            y0 + h > act->height) {
            res->status = ZIMAGE_OUT_OF_BOUNDS;
            return;
        }
    } else if (exp->width != act->width || exp->height != act->height) {
        res->status = ZIMAGE_SIZE_MISMATCH;
        return;
    }
    for (y = y0; y < y0 + h; y++) {
        for (x = x0; x < x0 + w; x++) {
            unsigned char e[3], a[3];
            int bad = 0;
            zt_px5(exp, x, y, e);
            zt_px5(act, x, y, a);
            for (i = 0; i < 3; i++) {
                int d = e[i] > a[i] ? e[i] - a[i] : a[i] - e[i];
                if (d <= cmp->tolerance) {
                    res->matches++;
                } else {
                    bad = 1;
                }
                if (d > res->max_diff) {
                    res->max_diff = d;
                }
            }
            if (bad && first) {
                first = 0;
                res->diff_x = x;
                res->diff_y = y;
                memcpy(res->exp_rgb, e, 3);
                memcpy(res->act_rgb, a, 3);
            }
        }
    }
    res->total = (unsigned long)w * (unsigned long)h * 3;
    res->status =
        !res->total || (float)res->matches / (float)res->total >= cmp->precision
            ? ZIMAGE_MATCH
            : ZIMAGE_MISMATCH;
}

typedef struct {
    unsigned char* data;
    size_t len;
    size_t cap;
} zt_membuf;

static void zt_png_sink(void* ctx, void* data, int size) {
    zt_membuf* m = ctx;
    if (!m->data && m->cap) {
        return; // a previous allocation failed
    }
    if (m->len + (size_t)size > m->cap) {
        size_t cap = (m->len + (size_t)size) * 2;
        unsigned char* p = ZTEST_REALLOC(m->data, cap);
        if (!p) {
            ZTEST_FREE(m->data);
            m->data = NULL;
            m->cap = 1;
            return;
        }
        m->data = p;
        m->cap = cap;
    }
    memcpy(m->data + m->len, data, (size_t)size);
    m->len += (size_t)size;
}

static int zt_write_png(const char* path, const zimage* img) {
    zt_membuf m = {NULL, 0, 0};
    unsigned char* rgb = ZTEST_MALLOC((size_t)img->width * img->height * 3);
    int x, y, ok = 0;
    if (!rgb) {
        return 0;
    }
    for (y = 0; y < img->height; y++) {
        for (x = 0; x < img->width; x++) {
            zimage_get_rgb(img, x, y, rgb + ((size_t)y * img->width + x) * 3);
        }
    }
    stbi_write_png_to_func(
        zt_png_sink, &m, img->width, img->height, 3, rgb, img->width * 3);
    ZTEST_FREE(rgb);
    if (m.data) {
        ok = zt_write_file(path, m.data, m.len);
        ZTEST_FREE(m.data);
    }
    return ok;
}

static void zt_write_actual(int indent, const char* name, const zimage* act) {
    char path[256];
    ztest_snprintf(
        path, sizeof(path), "expected/%s.%s.actual.png", name, ZTEST_TARGET);
    if (zt_write_png(path, act)) {
        zt_cont(indent, "Wrote %s", path);
    } else {
        zt_cont(indent, "Could not write %s", path);
    }
}

int ztest__image(int line, int op, int abort, const char* name, zimage act,
                 const zimage_cmp* cmp) {
    char target_path[256], plain_path[256];
    zimage exp;
    zimage_result res;
    int indent, ok = 0;
    zt_stb_line = line;
    zt_exp_paths(name, target_path, plain_path, sizeof(target_path));
    if (!act.data) {
        indent = zt_line_start(line);
        zt_log_text("Image: ", 0);
        zt_log_text(name, 0);
        zt_log_text("\n", 0);
        if (zt_frontbuffer_missing) {
            zt_cont(indent, "zimage_frontbuffer is not implemented for %s",
                    ZTEST_TARGET);
        } else {
            zt_cont(indent, "Actual image is empty");
        }
        return zt_after_fail(abort);
    }
    exp = zimage_png(target_path);
    if (!exp.data) {
        exp = zimage_png(plain_path);
    }
    if (!exp.data) {
        indent = zt_line_start(line);
        zt_log_text("Image: ", 0);
        zt_log_text(name, 0);
        zt_log_text("\n", 0);
        zt_cont(indent, "Missing %s", plain_path);
        zt_write_actual(indent, name, &act);
        zimage_free(&act);
        return zt_after_fail(abort);
    }
    cmp->compare(&exp, &act, cmp, &res);
    ok = op == ZT_EQ ? res.status == ZIMAGE_MATCH : res.status != ZIMAGE_MATCH;
    if (ok && op == ZT_EQ && ZT_HOSTED) {
        char path[256];
        ztest_snprintf(path, sizeof(path), "expected/%s.%s.actual.png", name,
                       ZTEST_TARGET);
        zt_remove_file(path);
    }
    if (!ok) {
        indent = zt_line_start(line);
        zt_log_text("Image: ", 0);
        zt_log_text(name, 0);
        zt_log_text("\n", 0);
        if (op != ZT_EQ) {
            zt_cont(indent, "Expected a difference, but the images match");
        } else if (res.status == ZIMAGE_SIZE_MISMATCH) {
            zt_cont(indent, "Expected: %dx%d  Actual: %dx%d", exp.width,
                    exp.height, act.width, act.height);
            zt_write_actual(indent, name, &act);
        } else if (res.status == ZIMAGE_OUT_OF_BOUNDS) {
            zt_cont(indent, "Region %d,%d %dx%d is outside %dx%d or %dx%d",
                    cmp->x, cmp->y, cmp->w, cmp->h, exp.width, exp.height,
                    act.width, act.height);
        } else {
            zt_cont(indent, "Match: %.2f%% (min %.2f%%)  Tolerance: %d",
                    res.total ? res.matches * 100.0 / res.total : 100.0,
                    cmp->precision * 100.0, cmp->tolerance);
            zt_cont(
                indent,
                "First diff at (%d,%d)  Expected: (%d,%d,%d)  "
                "Actual: (%d,%d,%d)",
                res.diff_x, res.diff_y, res.exp_rgb[0], res.exp_rgb[1],
                res.exp_rgb[2], res.act_rgb[0], res.act_rgb[1], res.act_rgb[2]);
            zt_write_actual(indent, name, &act);
        }
    }
    zimage_free(&exp);
    zimage_free(&act);
    return ok ? 1 : zt_after_fail(abort);
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

static int zt_glob(const char* pat, const char* s) {
    if (*pat == '\0') {
        return *s == '\0';
    }
    if (*pat == '*') {
        return zt_glob(pat + 1, s) || (*s && zt_glob(pat, s + 1));
    }
    if (*s && (*pat == '?' || *pat == *s)) {
        return zt_glob(pat + 1, s + 1);
    }
    return 0;
}

static const char* zt_filter;

static int zt_selected(const ztest_case* c) {
    char full[256], pat[256];
    const char* f = zt_filter;
    int has_positive = 0, positive = 0;
    if (!f || !*f) {
        return 1;
    }
    ztest_snprintf(full, sizeof(full), "%s::%s", c->group, c->name);
    while (*f) {
        size_t n = strcspn(f, ",");
        int neg = *f == '-';
        if (n - (size_t)neg < sizeof(pat)) {
            memcpy(pat, f + neg, n - (size_t)neg);
            pat[n - (size_t)neg] = '\0';
            if (neg && zt_glob(pat, full)) {
                return 0;
            }
            if (!neg) {
                has_positive = 1;
                positive |= zt_glob(pat, full);
            }
        }
        f += n;
        if (*f) {
            f++;
        }
    }
    return !has_positive || positive;
}

static void zt_print_start(const ztest_case* c) {
    if (zt_mode == ZT_EMOJI) {
        zt_outf("\xE2\x8F\xB3 %s::%s", c->group, c->name);
    } else if (zt_mode == ZT_COLOR) {
        zt_outf("RUN  %s::%s", c->group, c->name);
    } else if (ZT_HOSTED) {
        zt_outf("%s::%s", c->group, c->name);
    } else {
        // Consoles interleave SDK logs with this output and the runner renders
        // it anyway, so the name gets its own line and the result follows.
        zt_outf("%s::%s\n", c->group, c->name);
    }
    zt_plat_flush();
}

static void zt_print_result(const ztest_case* c, int newline_first) {
    static const char* const emoji[] = {
        "", "\xF0\x9F\x9F\xA2", "\xE2\x9D\x8C", "\xF0\x9F\x9F\xA1"};
    static const char* const color[] = {
        "", "\x1b[32mPASS\x1b[0m", "\x1b[31mFAIL\x1b[0m",
        "\x1b[33mSKIP\x1b[0m"};
    static const char* const plain[] = {"", "PASS", "FAIL", "SKIP"};
    if (zt_mode == ZT_PLAIN) {
        zt_outf(" %s\n", plain[c->result]);
        return;
    }
    zt_outf("%s%s %s::%s\n", newline_first ? "\r\x1b[K" : "",
            zt_mode == ZT_EMOJI ? emoji[c->result] : color[c->result], c->group,
            c->name);
}

// Static so they survive the longjmp back into zt_run.
static ztest_hook* zt_setup;
static ztest_hook* zt_teardown;

static void zt_run(ztest_case* c) {
    zt_setup = zt_find_hook(c->group, 0);
    zt_teardown = zt_find_hook(c->group, 1);
    zt_current = c;
    zt_stb_line = 0;
    zt_failed = 0;
    zt_skipped = 0;
    zt_log_len = 0;
    zt_log[0] = '\0';
    zt_log_bol = 1;
    zt_frontbuffer_missing = 0;
    zt_print_start(c);
    zt_jmp_valid = 1;
    if (ZT_SETJMP(zt_jmp) == 0) {
        if (zt_setup) {
            zt_setup->fn();
        }
        zt_current->fn();
    }
    if (zt_teardown && ZT_SETJMP(zt_jmp) == 0) {
        zt_teardown->fn();
    }
    zt_jmp_valid = 0;
    zt_arena_free_all();
    c = zt_current;
    c->result = zt_failed ? ZT_FAIL : zt_skipped ? ZT_SKIP : ZT_PASS;
    zt_print_result(c, 1);
    if (zt_log_len && (zt_failed || zt_verbose)) {
        if (!zt_log_bol) {
            zt_log_put("\n", 1);
        }
        zt_out(zt_log);
    }
    zt_plat_flush();
    zt_current = NULL;
}

#if ZT_HOSTED
static int zt_utf8_locale(void) {
    const char* vars[] = {"LC_ALL", "LC_CTYPE", "LANG"};
    size_t i;
    for (i = 0; i < 3; i++) {
        const char* v = getenv(vars[i]);
        if (v && *v) {
            for (; *v; v++) {
                if ((v[0] == 'U' || v[0] == 'u') &&
                    (v[1] == 'T' || v[1] == 't') &&
                    (v[2] == 'F' || v[2] == 'f') &&
                    (v[3] == '8' || (v[3] == '-' && v[4] == '8'))) {
                    return 1;
                }
            }
            return 0;
        }
    }
    return 0;
}

static int zt_detect_mode(void) {
    const char* term = getenv("TERM");
#if defined(_WIN32)
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (!_isatty(_fileno(stdout)) || !GetConsoleMode(out, &mode) ||
        !SetConsoleMode(out, mode | 0x0004)) {
        return ZT_PLAIN;
    }
    if (getenv("NO_COLOR")) {
        return ZT_PLAIN;
    }
    SetConsoleOutputCP(65001);
    return getenv("WT_SESSION") || getenv("TERM_PROGRAM") ? ZT_EMOJI : ZT_COLOR;
#else
    if (!isatty(fileno(stdout)) || getenv("NO_COLOR") || !term ||
        !strcmp(term, "dumb")) {
        return ZT_PLAIN;
    }
    if (!strcmp(term, "linux")) {
        return ZT_COLOR;
    }
    return zt_utf8_locale() ? ZT_EMOJI : ZT_COLOR;
#endif
}
#endif

static int zt_parse_mode(const char* s) {
    if (!strcmp(s, "emoji")) {
        return ZT_EMOJI;
    }
    if (!strcmp(s, "color")) {
        return ZT_COLOR;
    }
    if (!strcmp(s, "plain")) {
        return ZT_PLAIN;
    }
    return -1;
}

#define ZT_MAX_ARGS 32

// Every return from ztest_main goes through here so emulators can exit.
static int zt_finish(int code) {
#ifdef ZT_PS1
    if (zt_ps1_redux()) {
        ZT_HW16(0x1F802082) = (unsigned short)code;
    }
#endif
    return code;
}

#ifdef ZT_PS1
extern void (*__init_array_start[])(void) __attribute__((weak));
extern void (*__init_array_end[])(void) __attribute__((weak));

// nugget's crt0 does not run constructors; do it when nothing registered yet.
static void zt_run_ctors(void) {
    void (**fn)(void);
    if (zt_cases || !__init_array_start) {
        return;
    }
    for (fn = __init_array_start; fn != __init_array_end; fn++) {
        (*fn)();
    }
}
#endif

int ztest_main(int argc, char** argv) {
    static char args_buf[512];
    static char* args[ZT_MAX_ARGS];
    int list = 0, mode = -1, i;
    int passed = 0, failed = 0, skipped = 0, not_run = 0;
    ztest_case* c;
#ifdef ZT_PS1
    zt_run_ctors();
    if (zt_ps1_redux()) {
        ztest_add_tag("pcsx-redux");
    }
#endif
#if defined(__PSP__)
    // PPSSPP -r and usbhostfs_pc both map host0:/ to the host work directory.
    if (!zt_roots_set) {
        ztest_set_read_root("host0:/");
        ztest_set_write_root("host0:/");
    }
#endif
#ifdef ZT_NDS
    for (i = 0; i < 600 && ztest_nds_wait; i++) { // up to 10s for the debugger
        swiWaitForVBlank();
    }
    if (!zt_roots_set && nitroFSInit(NULL)) {
        ztest_set_read_root("nitro:/");
        zt_write_root[0] = '\0';
    }
    if (argc <= 1 && ztest_nds_args[0]) {
        size_t size = strlen(ztest_nds_args);
        char* file = ZTEST_MALLOC(size + 1);
        memcpy(file, ztest_nds_args, size + 1);
#elif defined(ZT_N64)
    debug_init_emulog();
    if (!display_get_width()) {
        display_init(
            RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, FILTERS_RESAMPLE);
    }
    if (!zt_roots_set && dfs_init(DFS_DEFAULT_LOCATION) == DFS_ESUCCESS) {
        ztest_set_read_root("rom:/");
    }
    if (argc <= 1 && ztest_n64_args[0] && ztest_n64_args[0] != '@') {
        size_t size = strlen(ztest_n64_args);
        char* file = ZTEST_MALLOC(size + 1);
        memcpy(file, ztest_n64_args, size + 1);
#else
    if (argc <= 1) {
        size_t size = 0;
        char* file = zt_read_file("ztest.args", &size);
#endif
        if (file) {
            char* p = args_buf;
            if (size >= sizeof(args_buf)) {
                size = sizeof(args_buf) - 1;
            }
            memcpy(args_buf, file, size);
            args_buf[size] = '\0';
            ZTEST_FREE(file);
            argc = 1;
            args[0] = argv && argc ? argv[0] : (char*)"ztest";
            while (*p && argc < ZT_MAX_ARGS) {
                while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
                    *p++ = '\0';
                }
                if (!*p) {
                    break;
                }
                args[argc++] = p;
                while (
                    *p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') {
                    p++;
                }
            }
            argv = args;
        }
    }
    for (i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (!strcmp(a, "--verbose") || !strcmp(a, "-v")) {
            zt_verbose = 1;
        } else if (!strcmp(a, "--list")) {
            list = 1;
        } else if (!strncmp(a, "--filter=", 9)) {
            zt_filter = a + 9;
        } else if (!strncmp(a, "--output=", 9)) {
            mode = zt_parse_mode(a + 9);
        } else if (!strcmp(a, "--help") || !strcmp(a, "-h")) {
            zt_out("usage: [--verbose] [--list] [--filter=glob,-glob,...] "
                   "[--output=emoji|color|plain]\n");
            return zt_finish(0);
        } else {
            zt_outf("ztest: unknown option %s\n", a);
        }
    }
#if ZT_HOSTED
    if (mode < 0 && getenv("ZTEST_OUTPUT")) {
        mode = zt_parse_mode(getenv("ZTEST_OUTPUT"));
    }
    zt_mode = mode >= 0 ? mode : zt_detect_mode();
#else
    zt_mode = mode >= 0 ? mode : ZT_PLAIN;
#endif
    if (list) {
        for (c = zt_cases; c; c = c->next) {
            if (zt_selected(c)) {
                zt_outf("%s::%s\n", c->group, c->name);
            }
        }
        zt_plat_flush();
        return zt_finish(0);
    }
    for (c = zt_cases; c; c = c->next) {
        c->result = 0;
        if (!zt_selected(c)) {
            continue;
        }
        if (zt_terminated) {
            not_run++;
            continue;
        }
        zt_run(c);
        passed += c->result == ZT_PASS;
        failed += c->result == ZT_FAIL;
        skipped += c->result == ZT_SKIP;
    }
    if (zt_mode != ZT_PLAIN && failed) {
        zt_out("\n");
        for (c = zt_cases; c; c = c->next) {
            if (c->result == ZT_FAIL) {
                zt_print_result(c, 0);
            }
        }
    }
    zt_outf("ztest: %d passed, %d failed, %d skipped", passed, failed, skipped);
    if (not_run) {
        zt_outf(", %d not run", not_run);
    }
    zt_out("\n");
    zt_plat_flush();
    return zt_finish(failed || zt_terminated ? 1 : 0);
}
