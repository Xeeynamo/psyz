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
#ifdef _WIN64
// long on MSVC is 32-bit, unlike any other platform
typedef unsigned long long u_long;
#else
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
    int x, y, w, h;
} PsyzRect;

#ifndef NULL
#define NULL ((void*)0)
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(
    sizeof(u_long) == sizeof(void*), "u_long type must be pointer aligned");
#endif

#endif
