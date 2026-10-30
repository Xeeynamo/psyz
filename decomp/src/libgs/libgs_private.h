#ifndef LIBGS_PRIVATE_H
#define LIBGS_PRIVATE_H
#include <libgte.h>
#include <libgpu.h>

typedef struct {
    short offx, offy;
} _GsPOSITION;

extern RECT CLIP2;
extern short PSDIDX;
extern _GsPOSITION POSITION;
extern DRAWENV GsDRAWENV;
extern short PSDGPU;
extern short PSDOFSX[2];
extern short PSDOFSY[2];
extern short GsORGOFSX;
extern short GsORGOFSY;

#endif
