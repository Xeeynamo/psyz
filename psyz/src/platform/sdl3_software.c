#include <psyz.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "../internal.h"
#include "rasterizer.h"

typedef struct {
    int x, y;
} Point;

typedef struct {
    int x, y;
    int u, v;
    int r, g, b;
} SwVertex;

#define SEMITRANSP 0x02
#define TEXTURED 0x04
#define EXTRA_VERTEX 0x08
#define GOURAUD 0x10
#define TRIANGLE 0x20

static u16 vram[VRAM_W * VRAM_H];
static bool dither_enabled;

static int SoftwareDither(void) {
    return dither_enabled && Psyz_VideoGetDitheringMode() != PSYZ_DITHER_OFF;
}
static Point draw_offset;
static Point draw_area_start;
static Point draw_area_end = {VRAM_W - 1, VRAM_H - 1};
static u16 cur_tpage;
static u8 twin_and_x = 0xff;
static u8 twin_and_y = 0xff;
static u8 twin_or_x;
static u8 twin_or_y;
static bool set_mask_bit;
static bool preserve_masked;
static unsigned grid_source = 1;
static unsigned grid_target = 1;

static int Sign11(int value) {
    value &= 0x7ff;
    return (value ^ 0x400) - 0x400;
}

static int Min3(int a, int b, int c) {
    int m = a < b ? a : b;
    return m < c ? m : c;
}

static int Max3(int a, int b, int c) {
    int m = a > b ? a : b;
    return m > c ? m : c;
}

static int Clamp5(int value) { return CLAMP(value, 0, 31); }

static u16 PackColor(int r, int g, int b, bool mask) {
    return (u16)(Clamp5(r) | (Clamp5(g) << 5) | (Clamp5(b) << 10) |
                 (mask ? 0x8000 : 0));
}

static void WritePixel(int x, int y, u16 src, bool semi, int abr) {
    if (x < draw_area_start.x || x > draw_area_end.x || y < draw_area_start.y ||
        y > draw_area_end.y || x < 0 || x >= VRAM_W || y < 0 || y >= VRAM_H) {
        return;
    }
    u16* out = &vram[y * VRAM_W + x];
    if (preserve_masked && (*out & 0x8000)) {
        return;
    }
    if (semi) {
        u16 dst = *out;
        int sr = src & 31;
        int sg = (src >> 5) & 31;
        int sb = (src >> 10) & 31;
        int dr = dst & 31;
        int dg = (dst >> 5) & 31;
        int db = (dst >> 10) & 31;
        switch (abr & 3) {
        case 0:
            sr = (sr + dr) >> 1;
            sg = (sg + dg) >> 1;
            sb = (sb + db) >> 1;
            break;
        case 1:
            sr += dr;
            sg += dg;
            sb += db;
            break;
        case 2:
            sr = dr - sr;
            sg = dg - sg;
            sb = db - sb;
            break;
        case 3:
            sr = dr + sr / 4;
            sg = dg + sg / 4;
            sb = db + sb / 4;
            break;
        }
        src = PackColor(sr, sg, sb, (src & 0x8000) != 0);
    }
    *out =
        (u16)((src & 0x7fff) | ((src | (set_mask_bit ? 0x8000 : 0)) & 0x8000));
}

static u16 SampleTexture(int u, int v, u16 tpage, u16 clut) {
    u = ((u & twin_and_x) | twin_or_x) & 0xff;
    v = ((v & twin_and_y) | twin_or_y) & 0xff;
    int tx = (tpage & 0xf) * 64;
    int ty = ((tpage >> 4) & 1) * 256 + v;
    int depth = (tpage >> 7) & 3;
    if (depth == 0) {
        u16 packed = vram[ty * VRAM_W + ((tx + u / 4) & 1023)];
        int index = (packed >> ((u & 3) * 4)) & 15;
        int cx = ((clut & 0x3f) * 16 + index) & 1023;
        int cy = (clut >> 6) & 511;
        return vram[cy * VRAM_W + cx];
    }
    if (depth == 1) {
        u16 packed = vram[ty * VRAM_W + ((tx + u / 2) & 1023)];
        int index = (packed >> ((u & 1) * 8)) & 255;
        int cx = ((clut & 0x3f) * 16 + index) & 1023;
        int cy = (clut >> 6) & 511;
        return vram[cy * VRAM_W + cx];
    }
    return vram[ty * VRAM_W + ((tx + u) & 1023)];
}

static const int dither[4][4] = {
    {-4, 0, -3, 1}, {2, -2, 3, -1}, {-3, 1, -4, 0}, {3, -1, 2, -2}};

static u16 ShadeTexel(
    u16 texel, int r, int g, int b, bool raw, int x, int y, bool do_dither) {
    int tr = texel & 31;
    int tg = (texel >> 5) & 31;
    int tb = (texel >> 10) & 31;
    if (raw) {
        return texel;
    }
    if (do_dither) {
        int d = dither[y & 3][x & 3];
        tr = Clamp5((CLAMP((tr * r) >> 4, 0, 255) + d) >> 3);
        tg = Clamp5((CLAMP((tg * g) >> 4, 0, 255) + d) >> 3);
        tb = Clamp5((CLAMP((tb * b) >> 4, 0, 255) + d) >> 3);
    } else {
        tr = CLAMP((tr * r) >> 7, 0, 31);
        tg = CLAMP((tg * g) >> 7, 0, 31);
        tb = CLAMP((tb * b) >> 7, 0, 31);
    }
    return PackColor(tr, tg, tb, (texel & 0x8000) != 0);
}

static int64_t Edge(const SwVertex* a, const SwVertex* b, int px2, int py2) {
    return (int64_t)(px2 - a->x * 2) * (b->y - a->y) -
           (int64_t)(py2 - a->y * 2) * (b->x - a->x);
}

static bool IsTopLeft(const SwVertex* a, const SwVertex* b) {
    int dy = b->y - a->y;
    int dx = b->x - a->x;
    return dy < 0 || (dy == 0 && dx > 0);
}

static void RasterTriangle(
    SwVertex a, SwVertex b, SwVertex c, bool textured, bool raw_texture,
    bool semi, bool gouraud, u16 tpage, u16 clut, bool centered) {
    int64_t area =
        (int64_t)(b.x - a.x) * (c.y - a.y) - (int64_t)(b.y - a.y) * (c.x - a.x);
    if (!area) {
        return;
    }
    if (area < 0) {
        SwVertex swap = b;
        b = c;
        c = swap;
        area = -area;
    }
    int min_x = CLAMP(Min3(a.x, b.x, c.x), draw_area_start.x, draw_area_end.x);
    int max_x = CLAMP(Max3(a.x, b.x, c.x), draw_area_start.x, draw_area_end.x);
    int min_y = CLAMP(Min3(a.y, b.y, c.y), draw_area_start.y, draw_area_end.y);
    int max_y = CLAMP(Max3(a.y, b.y, c.y), draw_area_start.y, draw_area_end.y);
    min_x = CLAMP(min_x, 0, VRAM_W - 1);
    max_x = CLAMP(max_x, 0, VRAM_W - 1);
    min_y = CLAMP(min_y, 0, VRAM_H - 1);
    max_y = CLAMP(max_y, 0, VRAM_H - 1);
    int abr = (tpage >> 5) & 3;
    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            int64_t wa = Edge(&b, &c, x * 2 + centered, y * 2 + centered);
            int64_t wb = Edge(&c, &a, x * 2 + centered, y * 2 + centered);
            int64_t wc = Edge(&a, &b, x * 2 + centered, y * 2 + centered);
            if (wa > 0 || (wa == 0 && !IsTopLeft(&b, &c)) || wb > 0 ||
                (wb == 0 && !IsTopLeft(&c, &a)) || wc > 0 ||
                (wc == 0 && !IsTopLeft(&a, &b))) {
                continue;
            }
            wa = -wa;
            wb = -wb;
            wc = -wc;
            int denom = (int)(area * 2);
            int r =
                gouraud ? (int)((wa * a.r + wb * b.r + wc * c.r) / denom) : a.r;
            int g =
                gouraud ? (int)((wa * a.g + wb * b.g + wc * c.g) / denom) : a.g;
            int blue =
                gouraud ? (int)((wa * a.b + wb * b.b + wc * c.b) / denom) : a.b;
            u16 color;
            bool blend = semi;
            if (textured) {
                int64_t ta = -Edge(&b, &c, x * 2 + centered, y * 2 + centered);
                int64_t tb = -Edge(&c, &a, x * 2 + centered, y * 2 + centered);
                int64_t tc = -Edge(&a, &b, x * 2 + centered, y * 2 + centered);
                int u = (int)((ta * a.u + tb * b.u + tc * c.u) / denom);
                int v = (int)((ta * a.v + tb * b.v + tc * c.v) / denom);
                u16 texel = SampleTexture(u, v, tpage, clut);
                if (texel == 0) {
                    continue;
                }
                blend = semi && (texel & 0x8000);
                color = ShadeTexel(texel, r, g, blue, raw_texture, x, y,
                                   SoftwareDither() != 0 && !raw_texture);
            } else {
                int d = SoftwareDither() && gouraud ? dither[y & 3][x & 3] : 0;
                color = PackColor(
                    (r + d) >> 3, (g + d) >> 3, (blue + d) >> 3, false);
            }
            WritePixel(x, y, color, blend, abr);
        }
    }
}

static SwVertex VertexXY(u32 word, u32 color) {
    SwVertex v = {0};
    v.x = Sign11(word) + draw_offset.x;
    v.y = Sign11(word >> 16) + draw_offset.y;
    if (grid_source != grid_target) {
        v.x = draw_offset.x +
              (v.x - draw_offset.x) * (int)grid_target / (int)grid_source;
    }
    v.r = color & 0xff;
    v.g = (color >> 8) & 0xff;
    v.b = (color >> 16) & 0xff;
    return v;
}

static void DrawLine(SwVertex a, SwVertex b, bool semi, u16 tpage) {
    int dx = abs(b.x - a.x);
    int dy = abs(b.y - a.y);
    if (!dx && !dy) {
        WritePixel(a.x, a.y, PackColor(a.r >> 3, a.g >> 3, a.b >> 3, false),
                   semi, (tpage >> 5) & 3);
        return;
    }
    int ox, oy, ex, ey;
    if (dx >= dy) {
        ox = 0;
        oy = 1;
        ex = b.x >= a.x ? 1 : -1;
        ey = 0;
    } else {
        ox = 1;
        oy = 0;
        ex = 0;
        ey = b.y >= a.y ? 1 : -1;
    }
    SwVertex q0 = a;
    SwVertex q1 = b;
    SwVertex q2 = b;
    SwVertex q3 = a;
    q1.x += ex;
    q1.y += ey;
    q2.x += ex + ox;
    q2.y += ey + oy;
    q3.x += ox;
    q3.y += oy;
    RasterTriangle(q0, q1, q2, false, false, semi, true, tpage, 0, true);
    RasterTriangle(q0, q2, q3, false, false, semi, true, tpage, 0, true);
}

static void RasterSprite(
    const SwVertex* origin, int w, int source_w, int h, int u, int v,
    bool textured, bool raw_texture, bool semi, u16 clut) {
    int abr = (cur_tpage >> 5) & 3;
    for (int py = 0; py < h; py++) {
        int y = origin->y + py;
        if (y < draw_area_start.y || y > draw_area_end.y || y < 0 ||
            y >= VRAM_H) {
            continue;
        }
        for (int px = 0; px < w; px++) {
            int x = origin->x + px;
            if (x < draw_area_start.x || x > draw_area_end.x || x < 0 ||
                x >= VRAM_W) {
                continue;
            }
            u16 color;
            bool blend = semi;
            if (textured) {
                int sample_x = w ? px * source_w / w : 0;
                u16 texel =
                    SampleTexture(u + sample_x, v + py, cur_tpage, clut);
                if (texel == 0) {
                    continue;
                }
                blend = semi && (texel & 0x8000);
                color = ShadeTexel(texel, origin->r, origin->g, origin->b,
                                   raw_texture, x, y, false);
            } else {
                color = PackColor(
                    origin->r >> 3, origin->g >> 3, origin->b >> 3, false);
            }
            WritePixel(x, y, color, blend, abr);
        }
    }
}

static void Software_Reset(void) {
    cur_tpage = 0;
    draw_offset = (Point){0, 0};
    draw_area_start = (Point){0, 0};
    draw_area_end = (Point){VRAM_W - 1, VRAM_H - 1};
    set_mask_bit = preserve_masked = false;
}

static void Software_SetTexpageMode(ParamDrawTexpageMode* p) {
    u16 mode = *(u16*)p;
    cur_tpage = mode & 0x1ff;
    dither_enabled = (mode & 0x200) != 0;
}

static void Software_SetTextureWindow(unsigned int mask_x, unsigned int mask_y,
                                      unsigned int off_x, unsigned int off_y) {
    mask_x &= 31;
    mask_y &= 31;
    twin_and_x = (u8) ~(mask_x * 8);
    twin_and_y = (u8) ~(mask_y * 8);
    twin_or_x = (u8)((off_x & mask_x) * 8);
    twin_or_y = (u8)((off_y & mask_y) * 8);
}

static void Software_SetAreaStart(int x, int y) {
    draw_area_start = (Point){x, y};
}

static void Software_SetAreaEnd(int x, int y) { draw_area_end = (Point){x, y}; }

static void Software_SetOffset(int x, int y) {
    draw_offset = (Point){Sign11(x), Sign11(y)};
}

static int Software_SetHorizontalGrid(
    unsigned int source_width, unsigned int target_width) {
    if (!source_width || !target_width) {
        return -1;
    }
    grid_source = source_width;
    grid_target = target_width;
    return 0;
}

static void Software_SetMask(int bit0, int bit1) {
    set_mask_bit = bit0 != 0;
    preserve_masked = bit1 != 0;
}

static void Software_ClearImage(PS1_RECT* rect, u_char r, u_char g, u_char b) {
    u16 color = PackColor(r >> 3, g >> 3, b >> 3, set_mask_bit);
    int x0 = CLAMP(rect->x, 0, VRAM_W);
    int y0 = CLAMP(rect->y, 0, VRAM_H);
    int x1 = CLAMP(rect->x + rect->w, 0, VRAM_W);
    int y1 = CLAMP(rect->y + rect->h, 0, VRAM_H);
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            u16* p = &vram[y * VRAM_W + x];
            if (!preserve_masked || !(*p & 0x8000)) {
                *p = color;
            }
        }
    }
}

static void Software_LoadImage(PS1_RECT* rect, u_long* data) {
    if (rect->w <= 0 || rect->h <= 0) {
        return;
    }
    const u16* src = (const u16*)data;
    if (rect->x >= 0 && rect->y >= 0 && rect->x + rect->w <= VRAM_W &&
        rect->y + rect->h <= VRAM_H) {
        for (int y = 0; y < rect->h; y++) {
            memcpy(&vram[(rect->y + y) * VRAM_W + rect->x], &src[y * rect->w],
                   (size_t)rect->w * sizeof(u16));
        }
        return;
    }
    for (int y = 0; y < rect->h; y++) {
        for (int x = 0; x < rect->w; x++) {
            int vx = rect->x + x;
            int vy = rect->y + y;
            if ((unsigned)vx < VRAM_W && (unsigned)vy < VRAM_H) {
                vram[vy * VRAM_W + vx] = src[y * rect->w + x];
            }
        }
    }
}

static void Software_StoreImage(PS1_RECT* rect, u_long* data) {
    if (rect->w <= 0 || rect->h <= 0) {
        return;
    }
    u16* dst = (u16*)data;
    if (rect->x >= 0 && rect->y >= 0 && rect->x + rect->w <= VRAM_W &&
        rect->y + rect->h <= VRAM_H) {
        for (int y = 0; y < rect->h; y++) {
            memcpy(&dst[y * rect->w], &vram[(rect->y + y) * VRAM_W + rect->x],
                   (size_t)rect->w * sizeof(u16));
        }
        return;
    }
    for (int y = 0; y < rect->h; y++) {
        for (int x = 0; x < rect->w; x++) {
            int vx = rect->x + x;
            int vy = rect->y + y;
            dst[y * rect->w + x] =
                ((unsigned)vx < VRAM_W && (unsigned)vy < VRAM_H)
                    ? vram[vy * VRAM_W + vx]
                    : 0;
        }
    }
}

static void Software_MoveImage(PS1_RECT* rect, unsigned int x, unsigned int y) {
    int src_x = CLAMP(rect->x, 0, VRAM_W - 1);
    int src_y = CLAMP(rect->y, 0, VRAM_H - 1);
    int dst_x = CLAMP((int)x, 0, VRAM_W - 1);
    int dst_y = CLAMP((int)y, 0, VRAM_H - 1);
    int max_w =
        VRAM_W - src_x < VRAM_W - dst_x ? VRAM_W - src_x : VRAM_W - dst_x;
    int max_h =
        VRAM_H - src_y < VRAM_H - dst_y ? VRAM_H - src_y : VRAM_H - dst_y;
    int w = CLAMP(rect->w, 0, max_w);
    int h = CLAMP(rect->h, 0, max_h);
    if (!w || !h) {
        return;
    }
    for (int row = 0; row < h; row++) {
        memmove(&vram[(dst_y + row) * VRAM_W + dst_x],
                &vram[(src_y + row) * VRAM_W + src_x], (size_t)w * sizeof(u16));
    }
}

static int Software_PushPrim(u_long* words, int max_len) {
    if (max_len <= 0) {
        return 0;
    }
    int code = (int)(words[0] >> 24);
    bool textured = (code & TEXTURED) != 0;
    bool gouraud = (code & GOURAUD) != 0;
    bool semi = (code & SEMITRANSP) != 0;
    bool raw_texture = textured && (code & 1);
    bool polygon = !(code & 0x40);
    bool line = (code & 0x40) && !(code & 0x20);
    int at = 1;
    if (polygon && (code & TRIANGLE)) {
        int count = (code & EXTRA_VERTEX) ? 4 : 3;
        int required = 1 + count * (1 + textured) + (count - 1) * gouraud;
        if (max_len < required) {
            return max_len;
        }
        SwVertex v[4] = {0};
        u16 clut = 0;
        u16 tpage = cur_tpage;
        u32 color = words[0];
        for (int i = 0; i < count && at < max_len; i++) {
            v[i] = VertexXY(words[at++], color);
            if (textured && at < max_len) {
                u32 uv = words[at++];
                v[i].u = uv & 0xff;
                v[i].v = (uv >> 8) & 0xff;
                if (i == 0) {
                    clut = uv >> 16;
                } else if (i == 1) {
                    tpage = uv >> 16;
                }
            }
            if (gouraud && i + 1 < count && at < max_len) {
                color = words[at++];
            }
        }
        RasterTriangle(v[0], v[1], v[2], textured, raw_texture, semi, gouraud,
                       tpage, clut, false);
        if (count == 4) {
            RasterTriangle(v[1], v[3], v[2], textured, raw_texture, semi,
                           gouraud, tpage, clut, false);
        }
        return at;
    }
    if (line) {
        int points = ((code >> 2) & 3) + 1;
        bool padding = points != 1;
        if (points == 1) {
            points = 2;
        }
        int required = 1 + points + (points - 1) * gouraud + padding;
        if (max_len < required) {
            return max_len;
        }
        SwVertex prev = {0};
        u32 color = words[0];
        for (int i = 0; i < points && at < max_len; i++) {
            SwVertex cur = VertexXY(words[at++], color);
            if (i) {
                DrawLine(prev, cur, semi, cur_tpage);
            }
            prev = cur;
            if (gouraud && i + 1 < points && at < max_len) {
                color = words[at++];
            }
        }
        if (padding && at < max_len) {
            at++;
        }
        return at;
    }
    if ((code & 0x60) == 0x60) {
        int required = 2 + textured + ((code & 0x18) == 0);
        if (max_len < required) {
            return max_len;
        }
        SwVertex origin = VertexXY(words[at++], words[0]);
        u16 clut = 0;
        int u = 0, v = 0;
        if (textured && at < max_len) {
            u32 uv = words[at++];
            u = uv & 0xff;
            v = (uv >> 8) & 0xff;
            clut = uv >> 16;
        }
        int w, h;
        switch (code & ~3) {
        case 0x60:
        case 0x64:
            w = (short)(words[at] & 0xffff);
            h = (short)(words[at++] >> 16);
            break;
        case 0x68:
        case 0x6c:
            w = h = 1;
            break;
        case 0x70:
        case 0x74:
            w = h = 8;
            break;
        default:
            w = h = 16;
            break;
        }
        int source_w = w;
        if (grid_source != grid_target) {
            w = w * (int)grid_target / (int)grid_source;
        }
        RasterSprite(
            &origin, w, source_w, h, u, v, textured, raw_texture, semi, clut);
        return at;
    }
    WARNF("unsupported GP0 primitive %02x", code);
    return 1;
}

void SoftwareRasterizer_Reset(void) {
    memset(vram, 0, sizeof(vram));
    Software_Reset();
    grid_source = grid_target = 1;
    twin_and_x = twin_and_y = 0xff;
    twin_or_x = twin_or_y = 0;
    dither_enabled = false;
}

const u16* SoftwareRasterizer_GetVram(void) { return vram; }

static void Software_FlushBuffer(void) {}
static void Software_ResetBuffer(void) {}
static int Software_ExequeSync(void) { return 0; }

const DrawBackend software_rasterizer = {
    .reset = Software_Reset,
    .texpage_mode = Software_SetTexpageMode,
    .texture_window = Software_SetTextureWindow,
    .area_start = Software_SetAreaStart,
    .area_end = Software_SetAreaEnd,
    .offset = Software_SetOffset,
    .horizontal_grid = Software_SetHorizontalGrid,
    .mask = Software_SetMask,
    .clear_image = Software_ClearImage,
    .load_image = Software_LoadImage,
    .store_image = Software_StoreImage,
    .move_image = Software_MoveImage,
    .reset_buffer = Software_ResetBuffer,
    .flush_buffer = Software_FlushBuffer,
    .push_prim = Software_PushPrim,
    .exeque_sync = Software_ExequeSync,
};
