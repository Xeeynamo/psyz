// Highly optimized helpers to fully use PSP hardware unique capabilities.
//
// Most functions are low-level with strict requirements. For simplicity, use:
// - memset_vfpu: memset replacement with an arbitrary size.
// - memset16_vfpu: faster memset_vfpu, requires dst and size 0x10 aligned.
// - memcpy_vfpu: memcpy replacement with an arbitrary size.
// - memcpy16_vfpu: faster memcpy_vfpu, requires dst, src, size 0x10 aligned.
// You can use the low-level variants if you know what you're doing.
//
// WARNING: functions with `vfpu` in their name must run on those threads
// flagged with PSP_THREAD_ATTR_VFPU, or they will fail bad at runtime.
//
// WARNING: most functions require pointers to be aligned by 4, 16 or 64 bytes.
// If unaligned pointers are passed to it, the PSP console WILL crash.

#ifndef PSYZ_PSP_H
#define PSYZ_PSP_H

#include <pspdmac.h>
#include <pspkernel.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Flexing point when it's more efficient to memcpy with the VFPU than with CPU
#define PSP_VFPU_COPY_MIN 192

// Flexing point when it's more efficient to memset with the VFPU than with CPU
#define PSP_VFPU_SET_MIN 256

// Flexing point when it's more efficient to memcpy via DMA than with CPU
#define PSP_DMAC_COPY_MIN 0x8000

typedef uint32_t __attribute__((may_alias)) psp_u32a;

// Claims the 0x40 RAM block at `p`; its content may become undefined, and the
// caller must overwrite the whole line before anything reads it
static inline void psp_dcache_claim_line(void* p) {
    __asm__ volatile("cache 0x18, 0(%0)" : : "r"(p) : "memory");
}

// sceKernelDcacheWritebackRange without the syscall: 10x faster on one line
static inline void psp_dcache_writeback(const void* p, size_t n) {
    uintptr_t a = (uintptr_t)p & ~(uintptr_t)63;
    uintptr_t end = (uintptr_t)p + n;
    for (; a < end; a += 64) {
        __asm__ volatile("cache 0x1A, 0(%0)" : : "r"(a) : "memory");
    }
    __asm__ volatile("sync" : : : "memory");
}

// Copy 0x10 bytes from src to dst; both pointers must be 0x10 aligned.
static inline void psp_vfpu_copy_quad(void* dst, const void* src) {
    __asm__ volatile("lv.q C000, 0(%1)\n\t"
                     "sv.q C000, 0(%0)"
                     :
                     : "r"(dst), "r"(src)
                     : "memory");
}

// Copy 0x10 bytes from src to dst; src must be 0x4 aligned, dst 0x10 aligned.
static inline void psp_vfpu_ucopy_quad(void* dst, const void* src) {
    __asm__ volatile(
        "lvl.q C000, 12(%1)\n\t"
        "lvr.q C000, 0(%1)\n\t"
        "sv.q C000, 0(%0)"
        :
        : "r"(dst), "r"(src)
        : "memory");
}

// Fastest memcpy possible between 192 bytes to 32KiB. size must be a multiple
// of 16, both src and dst must be 0x10 aligned.
static inline void psp_vfpu_copy16(
    unsigned char* dst, const unsigned char* src, size_t size) {
    // copy first 0x10, 0x20 or 0x30 bytes
    for (; size && ((uintptr_t)dst & 63); size -= 16, dst += 16, src += 16) {
        psp_vfpu_copy_quad(dst, src);
    }

    // batch copy of 0x40 blocks each
    size_t lines = size & ~(size_t)63;
    if (lines) {
        __asm__ volatile(
            ".set push\n\t"
            ".set noreorder\n"
            "1:\n\t"
            "cache 0x18, 0(%0)\n\t"
            "lv.q C000, 0(%1)\n\t"
            "lv.q C010, 16(%1)\n\t"
            "sv.q C000, 0(%0)\n\t"
            "lv.q C020, 32(%1)\n\t"
            "sv.q C010, 16(%0)\n\t"
            "lv.q C030, 48(%1)\n\t"
            "sv.q C020, 32(%0)\n\t"
            "addiu %2, %2, -64\n\t"
            "addiu %1, %1, 64\n\t"
            "sv.q C030, 48(%0)\n\t"
            "bnez %2, 1b\n\t"
            "addiu %0, %0, 64\n\t"
            ".set pop"
            : "+r"(dst), "+r"(src), "+r"(lines)
            :
            : "memory");
    }

    // copy the remaining 0x10, 0x20 or 0x30 bytes
    for (size &= 63; size; size -= 16, dst += 16, src += 16) {
        psp_vfpu_copy_quad(dst, src);
    }
}

// Slower than psp_vfpu_copy16, but allows src to be 0x4 aligned.
static inline void psp_vfpu_ucopy16(
    unsigned char* dst, const unsigned char* src, size_t size) {
    for (; size && ((uintptr_t)dst & 63); size -= 16, dst += 16, src += 16) {
        psp_vfpu_ucopy_quad(dst, src);
    }
    size_t lines = size & ~(size_t)63;
    if (lines) {
        __asm__ volatile(
            ".set push\n\t"
            ".set noreorder\n"
            "1:\n\t"
            "cache 0x18, 0(%0)\n\t"
            "lvl.q C000, 12(%1)\n\t"
            "lvr.q C000, 0(%1)\n\t"
            "lvl.q C010, 28(%1)\n\t"
            "lvr.q C010, 16(%1)\n\t"
            "sv.q C000, 0(%0)\n\t"
            "lvl.q C020, 44(%1)\n\t"
            "lvr.q C020, 32(%1)\n\t"
            "sv.q C010, 16(%0)\n\t"
            "lvl.q C030, 60(%1)\n\t"
            "lvr.q C030, 48(%1)\n\t"
            "sv.q C020, 32(%0)\n\t"
            "addiu %2, %2, -64\n\t"
            "addiu %1, %1, 64\n\t"
            "sv.q C030, 48(%0)\n\t"
            "bnez %2, 1b\n\t"
            "addiu %0, %0, 64\n\t"
            ".set pop"
            : "+r"(dst), "+r"(src), "+r"(lines)
            :
            : "memory");
    }
    for (size &= 63; size; size -= 16, dst += 16, src += 16) {
        psp_vfpu_ucopy_quad(dst, src);
    }
}

// dst and src must share their 4-byte alignment, and size must be at least 15.
static inline void psp_vfpu_memcpy(
    unsigned char* dst, const unsigned char* src, size_t size) {
    for (; (uintptr_t)dst & 3; size--) {
        *dst++ = *src++;
    }
    for (; (uintptr_t)dst & 15; size -= 4, dst += 4, src += 4) {
        *(psp_u32a*)dst = *(const psp_u32a*)src;
    }
    size_t mid = size & ~(size_t)15;
    if ((uintptr_t)src & 15) {
        psp_vfpu_ucopy16(dst, src, mid);
    } else {
        psp_vfpu_copy16(dst, src, mid);
    }
    dst += mid;
    src += mid;
    for (size &= 15; size >= 4; size -= 4, dst += 4, src += 4) {
        *(psp_u32a*)dst = *(const psp_u32a*)src;
    }
    while (size--) {
        *dst++ = *src++;
    }
}

// Copy large blocks of 0x40 bytes each via DMA, suggested for sizes bigger than
// 0x8000 bytes. DMA is used only if src and dst share their 0x10 alignment,
// otherwise it falls back to a VFPU or CPU copy.
static void* __attribute__((noipa, unused)) psp_dmac_memcpy(
    unsigned char* dst, const unsigned char* src, size_t size) {
    size_t head = -(uintptr_t)dst & 63;
    size_t lines = (size - head) & ~(size_t)63;
    if (!(((uintptr_t)dst ^ (uintptr_t)src) & 15)) {
        sceKernelDcacheWritebackRange(src + head, lines);
        sceKernelDcacheInvalidateRange(dst + head, lines);
        if (sceDmacMemcpy(dst + head, src + head, lines) >= 0) {
            memcpy(dst, src, head);
            memcpy(dst + head + lines, src + head + lines, (size - head) & 63);
            return dst;
        }
    }
    if (((uintptr_t)dst ^ (uintptr_t)src) & 3) {
        // worst scenario possible, where src and dst are unaligned.
        return memcpy(dst, src, size);
    }
    psp_vfpu_memcpy(dst, src, size);
    return dst;
}

// fastest memcpy replacement, requires dst, src, size to be 0x10 aligned.
static inline void* memcpy16_vfpu(void* dst, const void* src, size_t size) {
    if (size >= PSP_DMAC_COPY_MIN) {
        return psp_dmac_memcpy(dst, src, size);
    }
    psp_vfpu_copy16((unsigned char*)dst, (const unsigned char*)src, size);
    return dst;
}

// slower than memcpy16_vfpu, but accepts any alignment and size. The VFPU is
// used only if src and dst share their 0x4 alignment. Faster than memcpy.
static inline void* memcpy_vfpu(void* dst, const void* src, size_t size) {
    if (size >= PSP_DMAC_COPY_MIN) {
        return psp_dmac_memcpy(dst, src, size);
    }
    if (size < PSP_VFPU_COPY_MIN || (((uintptr_t)dst ^ (uintptr_t)src) & 3)) {
        return memcpy(dst, src, size);
    }
    psp_vfpu_memcpy((unsigned char*)dst, (const unsigned char*)src, size);
    return dst;
}

// Fastest memset possible from PSP_VFPU_SET_MIN. size must be a multiple of 16
// and dst must be 0x10 aligned.
static inline void psp_vfpu_set16(unsigned char* dst, int c, size_t size) {
    unsigned v = (unsigned char)c * 0x01010101u;
    __asm__ volatile(
        "mtv %0, S000\n\t"
        "mtv %0, S001\n\t"
        "mtv %0, S002\n\t"
        "mtv %0, S003"
        :
        : "r"(v));
    for (; size && ((uintptr_t)dst & 63); size -= 16, dst += 16) {
        __asm__ volatile("sv.q C000, 0(%0)" : : "r"(dst) : "memory");
    }
    size_t lines = size & ~(size_t)63;
    if (lines) {
        __asm__ volatile(
            ".set push\n\t"
            ".set noreorder\n"
            "1:\n\t"
            "cache 0x18, 0(%0)\n\t"
            "sv.q C000, 0(%0)\n\t"
            "sv.q C000, 16(%0)\n\t"
            "sv.q C000, 32(%0)\n\t"
            "addiu %1, %1, -64\n\t"
            "sv.q C000, 48(%0)\n\t"
            "bnez %1, 1b\n\t"
            "addiu %0, %0, 64\n\t"
            ".set pop"
            : "+r"(dst), "+r"(lines)
            :
            : "memory");
    }
    for (size &= 63; size; size -= 16, dst += 16) {
        __asm__ volatile("sv.q C000, 0(%0)" : : "r"(dst) : "memory");
    }
}

// fastest memset replacement, requires dst and size to be 0x10 aligned.
static inline void* memset16_vfpu(void* dst, int c, size_t size) {
    psp_vfpu_set16((unsigned char*)dst, c, size);
    return dst;
}

// slower than memset16_vfpu, but accepts any dst alignment and size.
// this is still faster than the plain memset.
static inline void* memset_vfpu(unsigned char* dst, int c, size_t size) {
    void* ret = dst;
    unsigned v = (unsigned char)c * 0x01010101u;
    if (size < PSP_VFPU_SET_MIN) {
        return memset(dst, c, size);
    }
    for (; (uintptr_t)dst & 3; size--) {
        *dst++ = (unsigned char)c;
    }
    for (; (uintptr_t)dst & 15; size -= 4, dst += 4) {
        *(psp_u32a*)dst = v;
    }
    size_t mid = size & ~(size_t)15;
    psp_vfpu_set16(dst, c, mid);
    dst += mid;
    for (size &= 15; size >= 4; size -= 4, dst += 4) {
        *(psp_u32a*)dst = v;
    }
    while (size--) {
        *dst++ = (unsigned char)c;
    }
    return ret;
}

#endif
