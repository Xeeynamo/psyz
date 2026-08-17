#include <libetc.h>
#include <libgpu.h>
#include <psyz.h>

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

#define GRID_COMMAND_OPCODE 0x03u
#define GRID_COMMAND(source_width)                                             \
    ((GRID_COMMAND_OPCODE << 24) | ((source_width) & 0xFFFFu))

#define TILE_SIZE 8
#define PACK_WIDTH 40
#define PACK_HEIGHT 16
#define PACK_COUNT (SCREEN_WIDTH / PACK_WIDTH)

#define SATURN_Y 56
#define PSX_Y 112

static DRAWENV draw;
static DISPENV disp;

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

static const unsigned char saturn_labels[10] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
};

static const unsigned char psx_labels[8] = {
    0, 1, 2, 3, 4, 5, 6, 7,
};

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

static void WriteRect(unsigned int x, unsigned int y, unsigned int w,
                      unsigned int h, unsigned int rgb) {

    Psyz_GpuWriteGP0(0x60000000u | (rgb & 0xFFFFFFu));
    Psyz_GpuWriteGP0((y << 16) | x);
    Psyz_GpuWriteGP0((h << 16) | w);
}

static void WriteGlyph(
    unsigned int x, unsigned int y, unsigned int glyph, unsigned int rgb) {

    int row;
    int col;

    for (row = 0; row < 7; row++) {
        unsigned int bits = font[glyph][row];

        for (col = 0; col < 5; col++) {
            if (bits & (1u << (4 - col))) {
                WriteRect(x + 1 + col, y + row, 1, 1, rgb);
            }
        }
    }
}

static void WriteNumberedTile(
    unsigned int x, unsigned int y, unsigned int label, unsigned int rgb) {

    // colored tile
    WriteRect(x, y, TILE_SIZE, TILE_SIZE, rgb);

    // shadow
    WriteGlyph(x + 1, y + 1, label, 0x101010u);

    // number
    WriteGlyph(x, y, label, 0xFFFFFFu);
}

static void WriteSaturnPack(unsigned int target_x, unsigned int y, int pack) {

    int row;
    int col;

    Psyz_GpuWriteGP0(GRID_COMMAND(320));

    // saturn packing: 5 * 8 = 40 pixels
    for (row = 0; row < 2; row++) {
        for (col = 0; col < 5; col++) {
            int index = row * 5 + col;
            unsigned int color = colors[(pack * 3 + index) % 10];

            WriteNumberedTile(target_x + col * TILE_SIZE, y + row * TILE_SIZE,
                              saturn_labels[index], color);
        }
    }
}

static void WritePsxPack(unsigned int target_x, unsigned int y, int pack) {

    int row;
    int col;

    unsigned int source_x = target_x * 4 / 5;

    Psyz_GpuWriteGP0(GRID_COMMAND(256));

    // psx packing, appears as 10x8 to match saturn
    for (row = 0; row < 2; row++) {
        for (col = 0; col < 4; col++) {
            int index = row * 4 + col;
            unsigned int color = colors[(pack * 3 + index) % 10];

            WriteNumberedTile(source_x + col * TILE_SIZE, y + row * TILE_SIZE,
                              psx_labels[index], color);
        }
    }
}

static void WriteGuideLines(unsigned int y0, unsigned int y1) {
    unsigned int x;

    Psyz_GpuWriteGP0(GRID_COMMAND(320));

    // grid line to separate 4x8 and 5x8 psx/saturn logical blocks
    for (x = 0; x < SCREEN_WIDTH; x += PACK_WIDTH) {
        WriteRect(x, y0, 1, y1 - y0, 0xFFFFFFu);
    }
}

static void InitGraphics(void) {
    ResetGraph(0);

    SetDefDrawEnv(&draw, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    SetDefDispEnv(&disp, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    PutDrawEnv(&draw);
    PutDispEnv(&disp);
    SetDispMask(1);
}

int main(void) {
    RECT screen = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};

    int pack;

    InitGraphics();

    // at 4x res saturn pixel == 4 pixels
    // psx pixel == 5 pixels
    // so we have a clean representation
    Psyz_VideoSetInternalResolution(4);

    Psyz_GpuRegisterCommandHandler(
        GRID_COMMAND_OPCODE, HandleGridCommand, NULL);

    while (1) {
        ClearImage(&screen, 8, 8, 8);

        for (pack = 0; pack < PACK_COUNT; pack++) {
            WriteSaturnPack(pack * PACK_WIDTH, SATURN_Y, pack);
        }

        for (pack = 0; pack < PACK_COUNT; pack++) {
            WritePsxPack(pack * PACK_WIDTH, PSX_Y, pack);
        }

        WriteGuideLines(SATURN_Y - 8, PSX_Y + PACK_HEIGHT + 8);

        Psyz_GpuWriteGP0(GRID_COMMAND(320));

        DrawSync(0);
        VSync(0);
    }

    return 0;
}
