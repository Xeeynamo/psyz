#ifndef _LIBGS_H_
#define _LIBGS_H_

#include <libgte.h>
#include <libgpu.h>

typedef struct {
    unsigned p : 24;
    unsigned char num : 8;
} GsOT_TAG;

typedef struct {
    unsigned long length;
    GsOT_TAG* org;
    unsigned long offset;
    unsigned long point;
    GsOT_TAG* tag;
} GsOT;

extern short PSDBASEX[2], PSDBASEY[2];
extern u_long PSDCNT;
extern DISPENV GsDISPENV;
extern long HWD0, VWD0;
extern MATRIX GsLIGHTWSMATRIX;
extern MATRIX GsIDMATRIX;
extern MATRIX GsIDMATRIX2;

void GsSetDrawBuffClip(void);
void GsSetDrawBuffOffset(void);

#endif
