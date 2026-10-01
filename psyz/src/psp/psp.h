// Highly optimized helpers to fully use PSP hardware unique capabilities.
//
// WARNING: functions with `vfpu` in their name must run on those threads
// flagged with PSP_THREAD_ATTR_VFPU, or they will fail bad at runtime.
//
// WARNING: most functions require pointers to be aligned by 4, 16 or 64 bytes.
// If unaligned pointers are passed to it, the PSP console WILL crash.
// If unsure, memset_vfpu and memcpy_vfpu will self-correct on unalignments.
//
// Following a list of benchmarks with these helpers, measured at 222/222/111.
// Throughput exceeding around 200MiB/s goes through the PSP L1 cache.
//
// ===== psp_dcache_writeback benchmark results ===============================
// sceKernelDcacheWritebackRange syscall VS raw psp_dcache_writeback
// TL;DR:
//   - always use psp_dcache_writeback over sceKernelDcacheWritebackRange.
// On real scearios, it helps frametimes in their lows, will reduce CPU usage.
// ┌─────────┬───────────────┬───────────┬───────────────┬───────────┐
// │  size   │ dirty/syscall │ dirty/new │ clean/syscall │ clean/new │
// ├─────────┼───────────────┼───────────┼───────────────┼───────────┤
// │ 16 B    │ 0.80µs        │ 0.12µs    │ 0.75µs        │ 0.04µs    │
// ├─────────┼───────────────┼───────────┼───────────────┼───────────┤
// │ 1 KiB   │ 2.28µs        │ 1.52µs    │ 1.09µs        │ 0.38µs    │
// ├─────────┼───────────────┼───────────┼───────────────┼───────────┤
// │ 16 KiB  │ 23.39µs       │ 20.99µs   │ 6.55µs        │ 5.84µs    │
// ├─────────┼───────────────┼───────────┼───────────────┼───────────┤
// │ 64 KiB  │ 34.71µs       │ 31.31µs   │ 23.68µs       │ 15.93µs   │
// ├─────────┼───────────────┼───────────┼───────────────┼───────────┤
// │ 128 KiB │ 37.45µs       │ 31.29µs   │ 15.25µs       │ 15.93µs   │
// ├─────────┼───────────────┼───────────┼───────────────┼───────────┤
// │ 512 KiB │ 36.43µs       │ 30.14µs   │ 15.22µs       │ 15.93µs   │
// └─────────┴───────────────┴───────────┴───────────────┴───────────┘
//
// ===== psp_dcache_claim_line benchmark results ==============================
// Use when the specified memory block gets written before anything reads it,
// and the destination isn't already in the cache. Expect a 2x with sw, up to
// 2.8x with sv.q. When the destination is hot, sw still gain 33% but sv.q
// loses 12%. Never claim a line that the GE, ME or DMA writes, because the
// stale dirty line would later overwrite their data.
// ┌─────────────────┬───────────┬────────────┬────────────┬──────────────┐
// │    dst state    │  sw       │ sw + claim │ sv.q       │ sv.q + claim │
// ├─────────────────┼───────────┼────────────┼────────────┼──────────────┤
// │ cold, 64 B      │ 64.4MiB/s │ 111.4MiB/s │ 102.7MiB/s │ 195.9MiB/s   │
// ├─────────────────┼───────────┼────────────┼────────────┼──────────────┤
// │ cold, 4-512 KiB │ 75.9MiB/s │ 159.1MiB/s │ 122.0MiB/s │ 343.0MiB/s   │
// ├─────────────────┼───────────┼────────────┼────────────┼──────────────┤
// │ hot, 4 KiB      │ 119.5MiB/s│ 159.0MiB/s │ 445.4MiB/s │ 393.3MiB/s   │
// └─────────────────┴───────────┴────────────┴────────────┴──────────────┘
//
// ===== memset benchmark results =============================================
// dst and size values are the minimum alignment.
// TL;DR:
//   - dst and size 0x10 aligned: memset16_vfpu, +48% to +214% at every size.
//   - anything else: memset_vfpu, +46% to +213% from 256 bytes.
//   - threads without PSP_THREAD_ATTR_VFPU: memset.
// ┌────────────┬─────┬──────┬───────────────┬───────┬────────┬───────┐
// │ Block size │ dst │ size │ Function      │ MiB/s │ memset │ Gain  │
// ├────────────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 64 B       │ 64  │ 64   │ memset16_vfpu │ 239.0 │ 132.7  │ +80%  │
// ├────────────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 48 B       │ 16  │ 16   │ memset16_vfpu │ 182.7 │ 111.7  │ +64%  │
// ├────────────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 60 B       │ 4   │ 4    │ memset_vfpu   │ 125.7 │ 125.6  │ 0%    │
// ├────────────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 63 B       │ 1   │ 1    │ memset_vfpu   │ 131.9 │ 131.9  │ 0%    │
// ├────────────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 256 B      │ 64  │ 64   │ memset16_vfpu │ 339.4 │ 210.9  │ +61%  │
// ├────────────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 240 B      │ 16  │ 16   │ memset16_vfpu │ 308.4 │ 208.5  │ +48%  │
// ├────────────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 252 B      │ 4   │ 4    │ memset_vfpu   │ 210.2 │ 210.2  │ 0%    │
// ├────────────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 255 B      │ 1   │ 1    │ memset_vfpu   │ 212.6 │ 212.7  │ 0%    │
// ├────────────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 4080 B     │ 16  │ 16   │ memset16_vfpu │ 387.9 │ 258.9  │ +50%  │
// ├────────────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 4092 B     │ 4   │ 4    │ memset_vfpu   │ 381.5 │ 258.8  │ +47%  │
// ├────────────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 4095 B     │ 1   │ 1    │ memset_vfpu   │ 378.6 │ 259.0  │ +46%  │
// ├────────────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 32752 B    │ 16  │ 16   │ memset16_vfpu │ 341.3 │ 109.1  │ +213% │
// └────────────┴─────┴──────┴───────────────┴───────┴────────┴───────┘
//
// ===== memcpy benchmark results =============================================
// dst, src, size values are the minimum alignment.
// TL;DR:
//   - src, dst, size 0x10 aligned: memcpy16_vfpu, +40% to +88% at every size.
//   - anything else: memcpy_vfpu, +23% to +78%.
//   - never call psp_dmac_memcpy below PSP_DMAC_COPY_MIN.
//   - plain memcpy only on threads without PSP_THREAD_ATTR_VFPU.
// ┌────────────┬─────┬─────┬──────┬───────────────┬───────┬────────┬───────┐
// │ Block size │ src │ dst │ size │ Function      │ MiB/s │ memcpy │ Gain  │
// ├────────────┼─────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 64 B       │ 64  │ 64  │ 64   │ memcpy16_vfpu │ 227.1 │ 123.9  │ +83%  │
// ├────────────┼─────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 48 B       │ 16  │ 16  │ 16   │ memcpy16_vfpu │ 154.7 │ 102.6  │ +51%  │
// ├────────────┼─────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 48 B       │ 4   │ 16  │ 16   │ memcpy_vfpu   │ 98.6  │ 102.6  │ -4%   │
// ├────────────┼─────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 60 B       │ 4   │ 4   │ 4    │ memcpy_vfpu   │ 102.1 │ 105.5  │ -3%   │
// ├────────────┼─────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 63 B       │ 1   │ 1   │ 1    │ memcpy_vfpu   │ 105.5 │ 109.1  │ -3%   │
// ├────────────┼─────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 63 B       │ 1   │ 64  │ 1    │ memcpy_vfpu   │ 83.9  │ 86.3   │ -3%   │
// ├────────────┼─────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 256 B      │ 64  │ 64  │ 64   │ memcpy16_vfpu │ 304.6 │ 173.5  │ +76%  │
// ├────────────┼─────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 240 B      │ 16  │ 16  │ 16   │ memcpy16_vfpu │ 267.4 │ 169.1  │ +58%  │
// ├────────────┼─────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 240 B      │ 4   │ 16  │ 16   │ memcpy_vfpu   │ 221.4 │ 169.2  │ +31%  │
// ├────────────┼─────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 252 B      │ 4   │ 4   │ 4    │ memcpy_vfpu   │ 218.1 │ 165.8  │ +31%  │
// ├────────────┼─────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 255 B      │ 1   │ 1   │ 1    │ memcpy_vfpu   │ 204.6 │ 166.9  │ +23%  │
// ├────────────┼─────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 255 B      │ 1   │ 64  │ 1    │ memcpy_vfpu   │ 131.5 │ 133.5  │ -1%   │
// ├────────────┼─────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 4 KiB      │ 64  │ 64  │ 64   │ memcpy16_vfpu │ 340.4 │ 197.8  │ +72%  │
// ├────────────┼─────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 4080 B     │ 16  │ 16  │ 16   │ memcpy16_vfpu │ 337.5 │ 197.5  │ +71%  │
// ├────────────┼─────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 4080 B     │ 4   │ 16  │ 16   │ memcpy_vfpu   │ 310.7 │ 197.5  │ +57%  │
// ├────────────┼─────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 4092 B     │ 4   │ 4   │ 4    │ memcpy_vfpu   │ 331.5 │ 197.1  │ +68%  │
// ├────────────┼─────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 4095 B     │ 1   │ 1   │ 1    │ memcpy_vfpu   │ 329.3 │ 197.2  │ +67%  │
// ├────────────┼─────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 4095 B     │ 1   │ 64  │ 1    │ memcpy_vfpu   │ 158.8 │ 158.9  │ 0%    │
// ├────────────┼─────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 32 KiB     │ 64  │ 64  │ 64   │ memcpy16_vfpu │ 95.7  │ 63.1   │ +52%  │
// ├────────────┼─────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 32752 B    │ 16  │ 16  │ 16   │ memcpy16_vfpu │ 88.2  │ 63.0   │ +40%  │
// ├────────────┼─────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 32752 B    │ 4   │ 16  │ 16   │ memcpy_vfpu   │ 85.5  │ 62.7   │ +36%  │
// ├────────────┼─────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 32764 B    │ 4   │ 4   │ 4    │ memcpy_vfpu   │ 88.2  │ 66.7   │ +32%  │
// ├────────────┼─────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 32767 B    │ 1   │ 1   │ 1    │ memcpy_vfpu   │ 88.2  │ 66.7   │ +32%  │
// ├────────────┼─────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 32767 B    │ 1   │ 64  │ 1    │ memcpy_vfpu   │ 59.0  │ 59.1   │ 0%    │
// ├────────────┼─────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 512 KiB    │ 64  │ 64  │ 64   │ memcpy16_vfpu │ 119.0 │ 63.2   │ +88%  │
// ├────────────┼─────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 524272 B   │ 16  │ 16  │ 16   │ memcpy16_vfpu │ 119.0 │ 63.2   │ +88%  │
// ├────────────┼─────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 524272 B   │ 4   │ 16  │ 16   │ memcpy_vfpu   │ 85.5  │ 62.7   │ +36%  │
// ├────────────┼─────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 524284 B   │ 4   │ 4   │ 4    │ memcpy_vfpu   │ 119.0 │ 66.8   │ +78%  │
// ├────────────┼─────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 524287 B   │ 1   │ 1   │ 1    │ memcpy_vfpu   │ 119.0 │ 66.8   │ +78%  │
// ├────────────┼─────┼─────┼──────┼───────────────┼───────┼────────┼───────┤
// │ 524287 B   │ 1   │ 64  │ 1    │ memcpy_vfpu   │ 59.2  │ 59.2   │ 0%    │
// └────────────┴─────┴─────┴──────┴───────────────┴───────┴────────┴───────┘

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

// Flexing point when writing back the whole D-cache beats a per-line writeback
#define PSP_DCACHE_WB_ALL_MIN 0xC000

typedef uint32_t __attribute__((may_alias)) psp_u32a;

// Claims the 0x40 RAM block at `p`; its content may become undefined, and the
// caller must overwrite the whole line before anything reads it.
static inline void psp_dcache_claim_line(void* p) {
    __asm__ volatile("cache 0x18, 0(%0)" : : "r"(p) : "memory");
}

// Faster sceKernelDcacheWritebackRange implementation. Below 48KiB uses a
// custom implementation without the syscall overhead. Above that size, it uses
// the faster sceKernelDcacheWritebackAll syscall.
static inline void psp_dcache_writeback(const void* p, size_t n) {
    if (n >= PSP_DCACHE_WB_ALL_MIN) {
        sceKernelDcacheWritebackAll();
        return;
    }
    uintptr_t a = (uintptr_t)p;
    uintptr_t end = ((a + n + 63) & ~(uintptr_t)63) | (a & 63);
    if (n) {
        __asm__ volatile(
            ".set push\n\t"
            ".set noreorder\n"
            "1:\n\t"
            "addiu %0, %0, 64\n\t"
            "bne %0, %1, 1b\n\t"
            "cache 0x1A, -64(%0)\n\t"
            ".set pop"
            : "+r"(a)
            : "r"(end)
            : "memory");
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
        psp_dcache_writeback(src + head, lines);
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

// Similar to memcpy16_vfpu, but accepts any alignment and size. The VFPU is
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
// only use when alignments are known, wrapped in memset_vfpu anyway.
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
