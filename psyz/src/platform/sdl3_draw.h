#ifndef SDL3_DRAW_H
#define SDL3_DRAW_H

static unsigned draw_grid_source_width = 1;
static unsigned draw_grid_target_width = 1;

static float GetDrawGridXScale(void) {
    return (float)draw_grid_target_width / (float)draw_grid_source_width;
}

int Draw_SetHorizontalGrid(
    unsigned int source_width, unsigned int target_width) {
    if (source_width == 0 || target_width == 0) {
        return -1;
    }
    if (draw_grid_source_width == source_width &&
        draw_grid_target_width == target_width) {
        return 0;
    }
    Draw_FlushBuffer();
    draw_grid_source_width = source_width;
    draw_grid_target_width = target_width;
    return 0;
}

typedef struct {
    short x, y;
    unsigned short u, v, c, t;
    unsigned char r, g, b, a;
    unsigned int twin;
} Vertex;

// ===== SDL3 reserved TPAGE flags, invalid on real hardware =====
#define TPAGE_NOTEXTURE 0x8000 // flag untextured poly
#define TPAGE_DITHER 0x4000    // flag a dithered primitive
#define TPAGE_POLYGON 0x2000   // flag a polygon to sample at PS1 integer coords

#define VRGBA(p) (*(unsigned int*)(&((p).r)))
#define SET_TC(p, tpage, clut)                                                 \
    (p)->t = (u16)(tpage), (p)->c = (u16)(clut), (p)->twin = cur_twin;
#define SET_TC_ALL(p, t, c)                                                    \
    SET_TC(p, t, c)                                                            \
    SET_TC(&(p)[1], t, c) SET_TC(&(p)[2], t, c) SET_TC(&(p)[3], t, c)

#define MAX_VERTEX_COUNT 4096
#define MAX_INDEX_COUNT (MAX_VERTEX_COUNT / 4 * 6)

#define SEMITRANSP 0x02
#define TEXTURED 0x04
#define EXTRA_VERTEX 0x08
#define GOURAUD 0x10
#define TRIANGLE 0x20

static u_short cur_tpage = 0;
static Vertex vertex_buf[MAX_VERTEX_COUNT];
static unsigned short index_buf[MAX_INDEX_COUNT];
static Vertex* vertex_cur;
static unsigned short* index_cur;
static unsigned short n_vertices;
static int n_indices;

// represents a texture window as a 32-bit integer for fast aligned copies
#define TWIN_PACK(and_x, and_y, or_x, or_y)                                    \
    ((unsigned int)(and_x) | ((unsigned int)(and_y) << 8) |                    \
     ((unsigned int)(or_x) << 16) | ((unsigned int)(or_y) << 24))
static unsigned int cur_twin = TWIN_PACK(0xFF, 0xFF, 0x00, 0x00);

static void Draw_EnsureBufferWillNotOverflow(int vertices, int indices) {
    bool bufferFull = n_vertices + vertices > MAX_VERTEX_COUNT ||
                      n_indices + indices > MAX_INDEX_COUNT;
    if (bufferFull) {
        Draw_FlushBuffer();
    }
}
static void Draw_EnqueueBuffer(int vertices, int indices) {
    assert(n_vertices + vertices <= MAX_VERTEX_COUNT);
    assert(n_indices + indices <= MAX_INDEX_COUNT);

    vertex_cur += vertices;
    index_cur += indices;
    n_vertices += vertices;
    n_indices += indices;
}

// real hardware use XY coords as signed 11-bit
static short s11(short v) { return (short)(((v & 0x7FF) ^ 1024) - 1024); }

static int writePacket(Vertex* v, int code, int n, u_long* packet, u16* pOut) {
    int w;
    if (!n) {
        return 0;
    }
    v->x = s11(((short*)packet)[0]);
    v->y = s11(((short*)packet)[1]);
    packet++;
    n--;
    if (!n) {
        return 1;
    }
    w = 1;
    if (code & TEXTURED) {
        v->u = ((u8*)packet)[0];
        v->v = ((u8*)packet)[1];
        *pOut = ((u16*)packet)[1];
        w++;
        packet++;
        n--;
        if (!n) {
            return w;
        }
    } else {
        *pOut = 0;
    }
    if (code & GOURAUD) {
        v++;
        v->r = ((u8*)packet)[0];
        v->g = ((u8*)packet)[1];
        v->b = ((u8*)packet)[2];
        v->a = code & SEMITRANSP ? 0x80 : 0xFF;
        w++;
    }
    return w;
}

static inline bool is_subtract_abr(const Vertex* v) {
    return v->a == 0x80 && (v->t & 0x60) == 0x40;
}

typedef enum {
    BLEND_ADD,
    BLEND_SUB,
    BLEND_SUB_OPAQUE,
    BLEND_ADD_UNTEXTURED,
    BLEND_SUB_UNTEXTURED,
    BLEND_COUNT,
} BlendMode;

static inline bool is_untextured(const Vertex* v) { return v->t & 0x8000; }
static int TexturedGroupEnd(int start, int run_end) {
    bool untextured = is_untextured(&vertex_buf[index_buf[start]]);
    int end = start + 3;
    while (end < run_end &&
           is_untextured(&vertex_buf[index_buf[end]]) == untextured) {
        end += 3;
    }
    return end;
}
static int SubtractGroupEnd(int start, int run_end) {
    bool untextured = is_untextured(&vertex_buf[index_buf[start]]);
    int end = start + 3;
    while (end < run_end) {
        if (is_untextured(&vertex_buf[index_buf[end]]) != untextured) {
            break;
        }
        if (!untextured) {
            bool shared = false;
            for (int i = 0; i < 9; i++) {
                shared |= index_buf[end + i / 3] == index_buf[end - 3 + i % 3];
            }
            if (!shared) {
                break;
            }
        }
        end += 3;
    }
    return end;
}

void Draw_SetTexpageMode(ParamDrawTexpageMode* p) {
    // implements SetDrawMode, SetDrawEnv
    unsigned short mode = *(u_short*)p;
    SetDither((mode & 0x200) ? 1 : 0);
    cur_tpage = mode & 0x1FF;
    if (p->tex_y_extra_vram) {
        DEBUGF("tex_y_extra_vram not implemented");
    }
    if (p->tex_flip_x) {
        DEBUGF("tex_flip_x not implemented");
    }
    if (p->tex_flip_y) {
        DEBUGF("tex_flip_y not implemented");
    }
}
void Draw_SetTextureWindow(unsigned int mask_x, unsigned int mask_y,
                           unsigned int off_x, unsigned int off_y) {
    mask_x &= 0x1F;
    mask_y &= 0x1F;
    cur_twin = TWIN_PACK(
        (unsigned char)~(mask_x * 8), (unsigned char)~(mask_y * 8),
        (unsigned char)((off_x & mask_x) * 8),
        (unsigned char)((off_y & mask_y) * 8));
}
void Draw_SetMask(int bit0, int bit1) {
    if (bit0 || bit1) {
        NOT_IMPLEMENTED;
    }
}

#endif
