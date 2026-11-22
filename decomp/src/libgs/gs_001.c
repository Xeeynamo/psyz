#include <common.h>
#include <libgs.h>
#include <libetc.h>
#include "libgs_private.h"

// Unreferenced build stamp.
// The second byte of word 1 seems to be the SDK version
// (0x40 = 4.0, 0x41 = 4.1, 0x43 = 4.3)
static u_long gs_stamp[2] = {0x15087350, 0x00408B9C};

extern TILE Clear[2];

static void gpu_init(unsigned short x, unsigned short y, unsigned short intmode,
                     unsigned short dith, unsigned short varmmode);
static void valiable_init(unsigned short x, unsigned short y);

void GsInitGraph(unsigned short x, unsigned short y, unsigned short intmode,
                 unsigned short dith, unsigned short varmmode) {
    gpu_init(x, y, intmode, dith, varmmode);
    gte_init();
    PSDIDX = 0;
    valiable_init(x, y);
    GsSetDrawBuffClip();
    GsSetDrawBuffOffset();
}

static void gpu_init(unsigned short x, unsigned short y, unsigned short intmode,
                     unsigned short dith, unsigned short varmmode) {
    ResetGraph(((intmode >> 4) & 3) == 3 ? 3 : 0);
    GsDRAWENV.ofs[0] = GsDRAWENV.ofs[1] = 0;
    GsDRAWENV.tw.x = GsDRAWENV.tw.y = GsDRAWENV.tw.w = GsDRAWENV.tw.h = 0;
    GsDRAWENV.tpage = 0;
    GsDRAWENV.dtd = dith;
    GsDRAWENV.dfe = 0;
    GsDRAWENV.isbg = 0;
    PutDrawEnv(&GsDRAWENV);

    setRECT(&GsDISPENV.disp, 0, 0, x, y);
    setRECT(&GsDISPENV.screen, 0, 0, 0, 0);
    if (GetVideoMode() == MODE_PAL) {
        GsDISPENV.screen.y = 24;
        GsDISPENV.pad0 = 1;
    }
    GsDISPENV.isinter = intmode & 1;
    PSDGPU = intmode & 4;
    GsDISPENV.isrgb24 = varmmode;
    PutDispEnv(&GsDISPENV);
}

void GsInitGraph2(unsigned short x, unsigned short y, unsigned short intmode,
                  unsigned short dith, unsigned short vrammode) {
    GsDISPENV.disp.w = x;
    GsDISPENV.disp.h = y;
    GsDRAWENV.tpage = 0;
    GsDRAWENV.dtd = dith;
    GsDRAWENV.dfe = 0;
    GsDRAWENV.isbg = 0;
    GsDISPENV.isinter = intmode & 1;
    PSDGPU = intmode & 4;
    GsDISPENV.isrgb24 = vrammode;
    valiable_init(x, y);
}

static void valiable_init(unsigned short x, unsigned short y) {
    VWD0 = y;
    HWD0 = x;

    GsIDMATRIX.m[0][1] = GsIDMATRIX.m[0][2] = 0;
    GsIDMATRIX.m[1][0] = GsIDMATRIX.m[1][2] = 0;
    GsIDMATRIX.m[2][0] = GsIDMATRIX.m[2][1] = 0;
    GsIDMATRIX.t[0] = GsIDMATRIX.t[1] = GsIDMATRIX.t[2] = 0;
    GsIDMATRIX.m[0][0] = 4096;
    GsIDMATRIX.m[1][1] = 4096;
    GsIDMATRIX.m[2][2] = 4096;
    GsIDMATRIX2 = GsIDMATRIX;

    GsLIGHTWSMATRIX = GsIDMATRIX;
    GsLIGHTWSMATRIX.m[0][0] = GsLIGHTWSMATRIX.m[1][1] =
        GsLIGHTWSMATRIX.m[2][2] = 0;
    _LC = GsLIGHTWSMATRIX;

    PSDBASEX[0] = 0;
    PSDBASEX[1] = 0;
    PSDBASEY[0] = 0;
    PSDBASEY[1] = 0;
    POSITION.offx = POSITION.offy = 0;
    GsIDMATRIX2.m[1][1] = 4096 * 4 * VWD0 / HWD0 / 3;
    CLIP2.x = CLIP2.y = 0;
    CLIP2.w = HWD0;
    CLIP2.h = VWD0;

    setlen(&Clear[0], 3);
    setcode(&Clear[0], 2);
    setlen(&Clear[1], 3);
    setcode(&Clear[1], 2);
    PSDCNT = 1;
}

void GsSortClear(unsigned char r, unsigned char g, unsigned char b, GsOT* ot) {
    Clear[PSDIDX].r0 = r;
    Clear[PSDIDX].g0 = g;
    Clear[PSDIDX].b0 = b;
    Clear[PSDIDX].x0 = PSDOFSX[PSDIDX];
    Clear[PSDIDX].y0 = PSDOFSY[PSDIDX];
    Clear[PSDIDX].h = VWD0;
    if (GsDISPENV.isrgb24) {
        Clear[PSDIDX].w = HWD0 * 3 / 2;
    } else {
        Clear[PSDIDX].w = HWD0;
    }
    AddPrim(ot->tag, &Clear[PSDIDX]);
}
