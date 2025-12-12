#include <psyz.h>
#include <libgpu.h>
#include <libetc.h>
#include <sys/file.h>
#include <stdio.h>

#define OT_LENGTH 12
#define OTSIZE (1 << OT_LENGTH)
#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 240

typedef struct DB {
    DRAWENV draw;
    DISPENV disp;
    OT_TYPE ot[OTSIZE];
    POLY_FT4 ft4[9];
} DB;

#include "4bpp.h"
#include "8bpp.h"
#include "16bpp.h"

static int LoadTim(void* data, u_short* outTpage, u_short* outClut) {
    if (OpenTIM((u_long*)data)) {
        printf("unable to open TIM");
        return 1;
    }
    TIM_IMAGE tim;
    if (!ReadTIM(&tim)) {
        printf("unable to read TIM");
        return 1;
    }
    LoadImage(tim.prect, tim.paddr);
    if (outTpage) {
        *outTpage = GetTPage((int)tim.mode, 0, tim.prect->x, tim.prect->y);
    }
    if (tim.caddr) {
        LoadImage(tim.crect, tim.caddr);
        if (outClut) {
            *outClut = GetClut(tim.crect->x, tim.crect->y);
        }
    }
    return 0;
}
static void SetPolyF4Img(POLY_FT4* poly, int x, int y, int w, int h, int u, int v,
                         u_short tpage, u_short clut, int semitrans) {
    SetPolyFT4(poly);
    setXYWH(poly, x, y, w, h);
    setRGB0(poly, 255, 128, 128);
    setUVWH(poly, u, v, w, h);
    setSemiTrans(poly, semitrans);
    poly->tpage = tpage;
    poly->clut = clut;
}
static void initGraphics(DB* db, int is_pal) {
    SetDefDrawEnv(&db[0].draw, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDrawEnv(&db[1].draw, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDispEnv(&db[0].disp, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDispEnv(&db[1].disp, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    if (is_pal) {
        db[0].disp.isinter = 1;
        db[1].disp.isinter = 1;
        SetVideoMode(MODE_PAL);
    } else {
        SetVideoMode(MODE_NTSC);
    }
    ResetGraph(0);
    PutDrawEnv(&db[0].draw);
    PutDispEnv(&db[0].disp);

    SetDispMask(1);
    SetGraphDebug(0);
    u_short tpage, clut;
    if (!LoadTim(img_4bpp, &tpage, &clut)) {
        SetPolyF4Img(&db[0].ft4[0], 8, 16, 64, 64, 0, 0, tpage, clut, 0);
        SetPolyF4Img(&db[0].ft4[3], 8, 96, 64, 64, 0, 0, tpage | 0x20, clut, 0);
        SetPolyF4Img(&db[0].ft4[4], 80, 96, 64, 64, 0, 0, tpage | 0x40, clut, 0);
        SetPolyF4Img(&db[0].ft4[5], 152, 96, 64, 64, 0, 0, tpage | 0x60, clut, 1);
        SetPolyF4Img(&db[0].ft4[6], 8, 176, 64, 64, 0, 0, tpage, clut, 1);
        SetPolyF4Img(&db[0].ft4[7], 80, 176, 64, 64, 0, 0, tpage | 0x20, clut, 1);
        SetPolyF4Img(&db[0].ft4[8], 152, 176, 64, 64, 0, 0, tpage | 0x40, clut, 1);
    }
    if (!LoadTim(img_8bpp, &tpage, &clut)) {
        SetPolyF4Img(&db[0].ft4[1], 80, 16, 64, 64, 0, 0, tpage, clut, 0);
    }
    if (!LoadTim(img_16bpp, &tpage, &clut)) {
        SetPolyF4Img(&db[0].ft4[2], 152, 16, 64, 64, 0, 0, tpage, clut, 0);
    }
}

int main(void) {
    DB db[2];
    DB* cdb;
    int pal_type = 0;
    int invalidate_pal = 0;

    StopCallback();
    ResetCallback();
    PadInit(0);
    initGraphics(db, 0);

    FntLoad(960, 256);
    SetDumpFnt(FntOpen(8, 8, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 512));

    cdb = &db[0];
    while (1) {
        u_long pad = PadRead(0);
        if (pad & PADLleft && pal_type > 0) {
            pal_type--;
            invalidate_pal = 1;
        }
        if (pad & PADLright && pal_type < 3) {
            pal_type++;
            invalidate_pal = 1;
        }
        if (invalidate_pal) {
            RECT rect = {640, 65, 16, 1};
            u16 pal[16];
            const int keycol = 2;
            StoreImage(&rect, (u_long*)pal);
            while (DrawSync(1));
            switch (pal_type) {
            case 0:
                for (int i = 0; i < 16; i++) {
                    pal[i] &= 0x7FFF;
                }
                pal[keycol] = 0;
                break;
            case 1:
                for (int i = 0; i < 16; i++) {
                    pal[i] &= 0x7FFF;
                }
                pal[keycol] = 0x7FFF;
                break;
            case 2:
                for (int i = 0; i < 16; i++) {
                    pal[i] |= 0x8000;
                }
                pal[keycol] = 0x7FFF;
                break;
            case 3:
                for (int i = 0; i < 16; i++) {
                    pal[i] |= 0x8000;
                }
                pal[keycol] = 0xFFFF;
                break;
            }
            LoadImage(&rect, (u_long*)pal);
            while (DrawSync(1));
            invalidate_pal = 0;
            while (PadRead(0));
        }

        cdb = (cdb == &db[0]) ? &db[0] : &db[0];
        ClearOTag(cdb->ot, OTSIZE);
        AddPrim(cdb->ot, &db[0].ft4[0]);
        AddPrim(cdb->ot, &db[0].ft4[1]);
        AddPrim(cdb->ot, &db[0].ft4[2]);
        AddPrim(cdb->ot, &db[0].ft4[3]);
        AddPrim(cdb->ot, &db[0].ft4[4]);
        AddPrim(cdb->ot, &db[0].ft4[5]);
        AddPrim(cdb->ot, &db[0].ft4[6]);
        AddPrim(cdb->ot, &db[0].ft4[7]);
        AddPrim(cdb->ot, &db[0].ft4[8]);
        FntPrint("4bpp     8bpp     16bpp    %d\n\n\n\n\n\n\n\n\n\n", pal_type);
        FntPrint("abr1     abr2     semi3    \n\n\n\n\n\n\n\n\n\n");
        FntPrint("semi0    semi1    semi2     \n\n\n\n\n\n\n\n\n\n");

        DrawSync(0);
        VSync(0);
        SetDispMask(1);

        ClearImage(&cdb->draw.clip, 60, 120, 120);
        DrawOTag(cdb->ot);
        FntFlush(-1);
        PutDispEnv(&cdb->disp);
    }
}
