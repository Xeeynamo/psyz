#ifndef PSYZ_TYPES_H
#define PSYZ_TYPES_H

#include <stdint.h>
#include <stddef.h>

// BSD-style unsigned types for PSX compatibility
#ifdef __psyz
#include <sys/types.h>

// sys/types.h may not define these on all platforms
#ifndef _BSD_SOURCE
typedef unsigned char u_char;
typedef unsigned short u_short;
#endif

// u_long* is widely used in PSY-Q, it should reflect the OS max pointer size
#if defined(_WIN64) && !defined(_WINSOCK2API_) && !defined(_WINSOCKAPI_)
// long on MSVC is 32-bit, unlike any other platform
typedef unsigned long long u_long;
#elif !defined(_WIN64)
#ifndef _BSD_SOURCE
typedef unsigned long u_long;
#endif
#endif

#elif !defined(_WINSOCK2API_) && !defined(_WINSOCKAPI_)
// winsock2.h/winsock.h already typedef these with Windows-compatible sizes
typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned long u_long;
#endif

#if !defined(LIBNDS_NDS_NDSTYPES_H__) && !defined(_PSPTYPES_H_)
typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
// rpcndr.h (pulled in by windows.h) already typedefs byte as unsigned char
#ifndef __RPCNDR_H__
typedef signed char byte;
#endif
#endif

typedef struct {
    int w, h;
} PsyzSize;

typedef struct {
    int sx, sy, ex, ey;
} PsyzRect;

#ifndef NULL
#define NULL ((void*)0)
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L &&                \
    !defined(_WINSOCK2API_) && !defined(_WINSOCKAPI_)
_Static_assert(
    sizeof(u_long) == sizeof(void*), "u_long type must be pointer aligned");
#endif

static inline PsyzRect Psyz_RectFromSize(int x, int y, int w, int h) {
    return (PsyzRect){x, y, x + w, y + h};
}

static inline unsigned int Psyz_GetRectWidth(PsyzRect r) {
    return r.ex > r.sx ? (unsigned int)(r.ex - r.sx) : 0;
}

static inline unsigned int Psyz_GetRectHeight(PsyzRect r) {
    return r.ey > r.sy ? (unsigned int)(r.ey - r.sy) : 0;
}

static inline PsyzRect Psyz_TranslateRect(PsyzRect r, int x, int y) {
    return (PsyzRect){r.sx + x, r.sy + y, r.ex + x, r.ey + y};
}

#endif
