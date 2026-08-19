#include <libetc.h>
#include <libgpu.h>
#include <psyz.h>

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

#define GRID_COMMAND_OPCODE 0x04u
#define GRID_COMMAND(source_width)                                             \
    ((GRID_COMMAND_OPCODE << 24) | ((source_width) & 0xFFFFu))

#define TILE_SIZE 8
#define PACK_WIDTH 40
#define PACK_HEIGHT 16
#define PACK_COUNT (SCREEN_WIDTH / PACK_WIDTH)

#define SATURN_Y 56
#define PSX_Y 112

#define GLYPH_COUNT 10
#define GLYPH_W 8
#define GLYPH_H 8
#define FONT_VRAM_X 640
#define FONT_VRAM_Y 0
#define FONT_TEX_W (GLYPH_COUNT * GLYPH_W)
#define FONT_TPAGE getTPage(2, 0, FONT_VRAM_X, FONT_VRAM_Y)

#define OT_LENGTH 1

#define MAX_PRIMS 512

typedef struct {
    O_TAG;
    u_long code[1];
} DR_GRID;

typedef union {
    TILE tile;
    SPRT sprt;
    DR_GRID grid;
} PRIM;

typedef struct {
    DRAWENV draw;
    DISPENV disp;
    OT_TYPE ot[OT_LENGTH];
    PRIM prims[MAX_PRIMS];
    int prim_count;
    void* prim_tail;
} DB;

static DB db[2];
static DB* cdb;

// 5x7 font 0-10
static const unsigned char font[10][7] = {
    /* 0 */
    {
        0x0E, /* .###. */
        0x11, /* #...# */
        0x13, /* #..## */
        0x15, /* #.#.# */
        0x19, /* ##..# */
        0x11, /* #...# */
        0x0E, /* .###. */
    },

    /* 1 */
    {
        0x04,
        0x0C,
        0x04,
        0x04,
        0x04,
        0x04,
        0x0E,
    },

    /* 2 */
    {
        0x0E,
        0x11,
        0x01,
        0x02,
        0x04,
        0x08,
        0x1F,
    },

    /* 3 */
    {
        0x1E,
        0x01,
        0x01,
        0x0E,
        0x01,
        0x01,
        0x1E,
    },

    /* 4 */
    {
        0x02,
        0x06,
        0x0A,
        0x12,
        0x1F,
        0x02,
        0x02,
    },

    /* 5 */
    {
        0x1F,
        0x10,
        0x10,
        0x1E,
        0x01,
        0x01,
        0x1E,
    },

    /* 6 */
    {
        0x0E,
        0x10,
        0x10,
        0x1E,
        0x11,
        0x11,
        0x0E,
    },

    /* 7 */
    {
        0x1F,
        0x01,
        0x02,
        0x04,
        0x08,
        0x08,
        0x08,
    },

    /* 8 */
    {
        0x0E,
        0x11,
        0x11,
        0x0E,
        0x11,
        0x11,
        0x0E,
    },

    /* 9 */
    {
        0x0E,
        0x11,
        0x11,
        0x0F,
        0x01,
        0x01,
        0x0E,
    }};

static u_short font_texture[FONT_TEX_W * GLYPH_H];

static const unsigned int colors[] = {
    0x4050E0u, 0x40B0E0u, 0x40D080u, 0x80D040u, 0xD0C040u,
    0xE08040u, 0xD04060u, 0xB040D0u, 0x7040D0u, 0x4070D0u,
};

static int HandleGridCommand(
    const u_long* words, int available, void* userdata) {
    unsigned int source_width;

    (void)userdata;

    if (available < 1) {
        return 0;
    }

    source_width = (unsigned int)words[0] & 0xFFFFu;

    if (Psyz_GpuSetHorizontalGrid(source_width, SCREEN_WIDTH) < 0) {
        return 0;
    }

    return 1;
}

// converts to 16-bit
static void LoadFont(void) {
    RECT rect;
    int glyph;
    int row;
    int col;

    for (glyph = 0; glyph < GLYPH_COUNT; glyph++) {
        for (row = 0; row < 7; row++) {
            unsigned int bits = font[glyph][row];

            for (col = 0; col < 5; col++) {
                if (bits & (1u << (4 - col))) {
                    font_texture[row * FONT_TEX_W + glyph * GLYPH_W + 1 + col] =
                        0x7FFF;
                }
            }
        }
    }

    rect.x = FONT_VRAM_X;
    rect.y = FONT_VRAM_Y;
    rect.w = FONT_TEX_W;
    rect.h = GLYPH_H;
    LoadImage(&rect, (u_long*)font_texture);
    DrawSync(0);
}

static void ResetPrims(void) {
    cdb->prim_count = 0;
    cdb->prim_tail = NULL;
    ClearOTag(cdb->ot, OT_LENGTH);
}

static void* AllocPrim(void) {
    void* prim;

    if (cdb->prim_count >= MAX_PRIMS) {
        return NULL;
    }

    prim = &cdb->prims[cdb->prim_count++];
    if (cdb->prim_tail) {
        setaddr(cdb->prim_tail, prim);
    } else {
        setaddr(cdb->ot, prim);
    }
    cdb->prim_tail = prim;
    termPrim(prim);
    return prim;
}

static void AddGrid(unsigned int source_width) {
    DR_GRID* grid = (DR_GRID*)AllocPrim();

    if (grid == NULL) {
        return;
    }

    setlen(grid, 1);
    grid->code[0] = GRID_COMMAND(source_width);
}

static void AddRect(int x, int y, int w, int h, unsigned int rgb) {
    TILE* tile = (TILE*)AllocPrim();

    if (tile == NULL) {
        return;
    }

    SetTile(tile);
    setXY0(tile, x, y);
    setWH(tile, w, h);
    setRGB0(tile, (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

static void AddGlyph(int x, int y, int glyph, int intensity) {
    SPRT* sprt = (SPRT*)AllocPrim();

    if (sprt == NULL) {
        return;
    }

    SetSprt(sprt);
    setXY0(sprt, x, y);
    setWH(sprt, GLYPH_W, GLYPH_H);
    setUV0(sprt, glyph * GLYPH_W, 0);
    setClut(sprt, 0, 0);
    setRGB0(sprt, intensity, intensity, intensity);
}

static void AddNumberedTile(int x, int y, int label, unsigned int rgb) {

    // colored tile
    AddRect(x, y, TILE_SIZE, TILE_SIZE, rgb);

    // shadow
    AddGlyph(x + 1, y + 1, label, 0x10);

    // number
    AddGlyph(x, y, label, 0x80);
}

static void AddSaturnPack(int target_x, int y, int pack) {

    int row;
    int col;

    AddGrid(320);

    // saturn packing: 5 * 8 = 40 pixels
    for (row = 0; row < 2; row++) {
        for (col = 0; col < 5; col++) {
            int index = row * 5 + col;
            unsigned int color = colors[(pack * 3 + index) % 10];

            AddNumberedTile(
                target_x + col * TILE_SIZE, y + row * TILE_SIZE, index, color);
        }
    }
}

static void AddPsxPack(int target_x, int y, int pack) {

    int row;
    int col;

    int source_x = target_x * 4 / 5;

    AddGrid(256);

    // psx packing, appears as 10x8 to match saturn
    for (row = 0; row < 2; row++) {
        for (col = 0; col < 4; col++) {
            int index = row * 4 + col;
            unsigned int color = colors[(pack * 3 + index) % 10];

            AddNumberedTile(
                source_x + col * TILE_SIZE, y + row * TILE_SIZE, index, color);
        }
    }
}

static void AddGuideLines(int y0, int y1) {
    int x;

    AddGrid(320);

    // grid line to separate 4x8 and 5x8 psx/saturn logical blocks
    for (x = 0; x < SCREEN_WIDTH; x += PACK_WIDTH) {
        AddRect(x, y0, 1, y1 - y0, 0xFFFFFFu);
    }
}

static void InitGraphics(void) {
    int i;

    ResetGraph(0);

    SetDefDrawEnv(&db[0].draw, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDrawEnv(&db[1].draw, 0, SCREEN_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDispEnv(&db[0].disp, 0, SCREEN_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDispEnv(&db[1].disp, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    for (i = 0; i < 2; i++) {
        db[i].draw.isbg = 1;
        setRGB0(&db[i].draw, 8, 8, 8);
        db[i].draw.tpage = FONT_TPAGE;
    }

    SetDispMask(1);
}

int main(void) {
    int pack;

    InitGraphics();

    // at 4x res saturn pixel == 4 pixels
    // psx pixel == 5 pixels
    // so we have a clean representation
    Psyz_VideoSetInternalResolution(4);

    Psyz_GpuRegisterCommandHandler(
        GRID_COMMAND_OPCODE, HandleGridCommand, NULL);

    LoadFont();

    cdb = db;
    while (1) {
        cdb = (cdb == db) ? db + 1 : db;
        ResetPrims();

        for (pack = 0; pack < PACK_COUNT; pack++) {
            AddSaturnPack(pack * PACK_WIDTH, SATURN_Y, pack);
        }

        for (pack = 0; pack < PACK_COUNT; pack++) {
            AddPsxPack(pack * PACK_WIDTH, PSX_Y, pack);
        }

        AddGuideLines(SATURN_Y - 8, PSX_Y + PACK_HEIGHT + 8);

        // back to the native grid for whatever is drawn next
        AddGrid(320);

        DrawSync(0);
        VSync(0);

        PutDispEnv(&cdb->disp);
        PutDrawEnv(&cdb->draw);
        DrawOTag(cdb->ot);
    }

    return 0;
}
