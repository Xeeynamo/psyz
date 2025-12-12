#include <kernel.h>
#include <libetc.h>
#include <libgpu.h>

#define OT_LENGTH 1
#define OTSIZE 1 << OT_LENGTH
#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 240

typedef struct DB {
    DRAWENV draw;
    DISPENV disp;
    OT_TYPE ot[OTSIZE];
    POLY_FT4 ft4[4];
    SPRT sprt[4];
} DB;
DB db[2];
DB* cdb;

#include "../tests/res/4bpp.h"

static int LoadTim(void* data, u_short* outTpage, u_short* outClut) {
    if (OpenTIM((u_long*)data)) {
        return 1;
    }
    TIM_IMAGE tim;
    if (!ReadTIM(&tim)) {
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
void RunTest() {
    u_short tpage, clut;
    if (LoadTim(img_4bpp, &tpage, &clut)) {
        return;
    }

    DRAWENV drawEnv = {0};
    drawEnv.clip.x = 964;
    drawEnv.clip.y = 16;
    drawEnv.clip.w = 8;
    drawEnv.clip.h = 32;
    drawEnv.ofs[0] = drawEnv.clip.x;
    drawEnv.ofs[1] = drawEnv.clip.y;
    drawEnv.r0 = 255;
    drawEnv.g0 = drawEnv.b0 = 0;
    drawEnv.isbg = 1;
    PutDrawEnv(&drawEnv);

    cdb->draw.tpage = tpage;
    PutDrawEnv(&cdb->draw);

    SetSprt(&cdb->sprt[0]);
    SetSemiTrans(&cdb->sprt[0], 0);
    SetShadeTex(&cdb->sprt[0], 1);
    setXY0(&cdb->sprt[0], 16, 16);
    setWH(&cdb->sprt[0], 64, 64);
    setUV0(&cdb->sprt[0], 0, 0);
    cdb->sprt[0].clut = clut;
    AddPrim(cdb->ot, &cdb->sprt[0]);

    ClearImage(&cdb->draw.clip, 60, 120, 120);
    DrawOTag(cdb->ot);
    DrawSync(0);
    VSync(0);
    PutDispEnv(&cdb->disp);
}

void SetUp() {
    SetDefDrawEnv(&db[0].draw, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDrawEnv(&db[1].draw, SCREEN_WIDTH, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDispEnv(&db[0].disp, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDispEnv(&db[1].disp, SCREEN_WIDTH, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetVideoMode(MODE_NTSC);
    ResetGraph(0);
    PutDrawEnv(&db[0].draw);
    PutDispEnv(&db[0].disp);
    ClearOTag(db[0].ot, OTSIZE);
    ClearOTag(db[1].ot, OTSIZE);
    SetDispMask(1);
    cdb = &db[0];
}

void TearDown() { ResetGraph(0); }

int main() {
    SetUp();
    RunTest();
    TearDown();
    return 0;
}
