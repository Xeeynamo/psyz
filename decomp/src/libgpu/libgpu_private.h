#ifndef LIBGPU_PRIVATE_H
#define LIBGPU_PRIVATE_H
#include <libgpu.h>

typedef struct {
    const char* ver;
    int (*addque)(int (*exec)(u_long p1, u_long p2), u_long p1, u_long p2);
    int (*addque2)(
        int (*exec)(u_long p1, u_long p2), u_long p1, int len, u_long p2);
    int (*clr)(RECT* rect, u32 color);
    void (*ctl)(unsigned int);
    int (*cwb)(u32* data, s32 n);
    void (*cwc)(u_long* packets);
    int (*drs)(RECT* rect, u_long* data);
    int (*dws)(RECT* rect, u_long* data);
    int (*exeque)(void);
    int (*getctl)(int);
    int (*otc)(OT_TYPE* ot, s32 n);
    int (*param)(int);
    int (*reset)(int);
    u_long (*status)(void);
    int (*sync)(int mode);
} GpuVtable;

#endif
