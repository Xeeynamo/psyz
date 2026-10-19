#include <string.h>
#include <psyz.h>
#include <libetc.h>
#include <libgpu.h>
#include <libgte.h>
#include <libgs.h>
#include "ztest.h"

enum {
    GS_OT_LENGTH = 1,
    GS_SCREEN_WIDTH = 256,
    GS_SCREEN_HEIGHT = 240,
};

static GsOT gs_ot[2];
static GsOT_TAG gs_ot_tag[2][1 << GS_OT_LENGTH];
static PACKET gs_packets[2][64];
static TILE gs_tile;

// 15-bit VRAM colour of an 8-bit RGB triple
static u_short gs_rgb15(int r, int g, int b) {
    return (u_short)((r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10));
}

static u_short gs_read_pixel(int x, int y) {
    RECT rect = {(short)x, (short)y, 1, 1};
    u_short pixel[2] = {0, 0};
    DrawSync(0);
    StoreImage(&rect, (u_long*)pixel);
    DrawSync(0);
    // colour only: whether drawing sets the mask bit (15) is not under test
    return pixel[0] & 0x7FFF;
}

// Clears both buffers to black, then sorts and draws one frame into the
// current draw buffer: a clear to (r, g, b) and, if drawTile, a 16x16 red
// tile at (8, 8) in screen coordinates.
static int gs_frame(int r, int g, int b, int drawTile) {
    RECT all = {0, 0, GS_SCREEN_WIDTH, 2 * GS_SCREEN_HEIGHT};
    ClearImage(&all, 0, 0, 0);
    int buf = GsGetActiveBuff();
    GsSetWorkBase(gs_packets[buf]);
    GsClearOt(0, 0, &gs_ot[buf]);
    GsSortClear((u_char)r, (u_char)g, (u_char)b, &gs_ot[buf]);
    if (drawTile) {
        setTile(&gs_tile);
        setRGB0(&gs_tile, 255, 0, 0);
        setXY0(&gs_tile, 8, 8);
        setWH(&gs_tile, 16, 16);
        AddPrim((OT_TYPE*)gs_ot[buf].tag, &gs_tile);
    }
    GsDrawOt(&gs_ot[buf]);
    DrawSync(0);
    return buf;
}

static void gs_setup(void) {
    Psyz_VideoSetDitheringMode(PSYZ_DITHER_OFF);
    Psyz_VideoSetInternalResolution(1);
    ResetGraph(0);
    GsInitGraph(GS_SCREEN_WIDTH, GS_SCREEN_HEIGHT, GsNONINTER | GsOFSGPU, 0, 0);
    GsDefDispBuff(0, 0, 0, GS_SCREEN_HEIGHT);
    for (int i = 0; i < 2; i++) {
        gs_ot[i].length = GS_OT_LENGTH;
        gs_ot[i].org = gs_ot_tag[i];
        GsClearOt(0, 0, &gs_ot[i]);
    }
}

ZTEST_SETUP(gs) { gs_setup(); }

// GsSortClear fills the whole current draw buffer, whichever it is.
ZTEST(gs, sort_clear_fills_draw_buffer) {
    u_short grey = gs_rgb15(48, 48, 48);
    zexpect_s32_eq(0, gs_frame(48, 48, 48, 0));
    zprintf("buffer 0 (y 0..239) is cleared\n");
    zexpect_u16_eq(grey, gs_read_pixel(0, 0));
    zexpect_u16_eq(grey, gs_read_pixel(GS_SCREEN_WIDTH - 1, 239));
    zexpect_u16_eq(0, gs_read_pixel(0, 240));

    GsSwapDispBuff();
    zexpect_s32_eq(1, gs_frame(48, 48, 48, 0));
    zprintf("buffer 1 (y 240..479) is cleared\n");
    zexpect_u16_eq(grey, gs_read_pixel(0, 240));
    zexpect_u16_eq(grey, gs_read_pixel(GS_SCREEN_WIDTH - 1, 479));
    zexpect_u16_eq(0, gs_read_pixel(0, 239));
}

// With GsOFSGPU, primitives in screen coordinates land in the current draw
// buffer: GsDefDispBuff's second buffer starts at (x1, y1).
ZTEST(gs, draw_offset_follows_buffer) {
    u_short red = gs_rgb15(255, 0, 0);
    zexpect_s32_eq(0, gs_frame(0, 0, 0, 1));
    zprintf("buffer 0: the tile is at (8, 8)\n");
    zexpect_u16_eq(red, gs_read_pixel(8, 8));
    zexpect_u16_eq(red, gs_read_pixel(23, 23));
    zexpect_u16_eq(0, gs_read_pixel(8, 248));

    GsSwapDispBuff();
    zexpect_s32_eq(1, gs_frame(0, 0, 0, 1));
    zprintf("buffer 1: the tile is at (8, 248)\n");
    zexpect_u16_eq(red, gs_read_pixel(8, 248));
    zexpect_u16_eq(red, gs_read_pixel(23, 263));
    zexpect_u16_eq(0, gs_read_pixel(8, 8));
}
