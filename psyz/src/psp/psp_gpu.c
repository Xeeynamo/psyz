// PSP GPU backend using the Graphics Engine.
//
// The backend has a strong emphasis on parallelizing CPU and GPU when possible.
// DMA transfers and GPU draws are preferred when moving graphics, so the CPU
// is free to continue emulating the PS1 GPU. PS1 games are not heavy, and the
// PSP itself is powerful enough to run those games natively. The challenge of
// this backend is synchronization and accuracy.
//
// When PSYZ_ASPECT_DISPLAY is used (default), it forces the output to be
// 320x240. The game gets rendered to a temporary location in the EDRAM,
// then it gets used as a texture to blit to the display at 320x240.
// Alternatively, PSYZ_ASPECT_SQUARE completely bypasses this mechanic, and
// it renders straight to the buffer meant to show to the display. This way is
// more performant than PSYZ_ASPECT_DISPLAY, and it's pixel perfect.

#include <pspdisplay.h>
#include <pspge.h>
#include <pspgu.h>
#include <psputils.h>
#include <psyz.h>
#include <psyz/log.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libgpu.h>
#include "../draw.h"

#define PSX_VRAM_W 1024
#define PSX_VRAM_H 512
#define PSP_SCREEN_W 480
#define PSP_SCREEN_H 272
#define FB_STRIDE 512

#define VRAM_TILE_W 64 // 64 for 16bpp, 128 for 8bpp, 256 for 4bpp
#define VRAM_TILE_H 256
#define VRAM_TILES_X 16
#define VRAM_TILES 32
#define VRAM_TILE_BYTES (VRAM_TILE_W * VRAM_TILE_H * 2)

// bypass D-cache, so there's no need for manual writeback
#define UNCACHED(p) ((void*)(0x40000000u | (uintptr_t)(p)))

// EDRAM layout (2MB):
// * Double-buffered 512x272 16-bit framebuffers
// * 1MB for the actual PS1 VRAM
#define EDRAM_DRAW_OFFSET 0
#define EDRAM_DISP_OFFSET (FB_STRIDE * PSP_SCREEN_H * 2)
#define EDRAM_TILES_OFFSET (EDRAM_DISP_OFFSET * 2)
// off-screen surface for the aspect-corrected present path, after the mirror
#define EDRAM_SURFACE_OFFSET (EDRAM_TILES_OFFSET + VRAM_TILES * VRAM_TILE_BYTES)

static bool is_init = false;

static unsigned int __attribute__((aligned(64))) dlist[2][0x80000 / 4];
static int dlist_idx;
// stop emitting when the list is nearly full instead of corrupting memory
#define DLIST_WATERMARK (sizeof(dlist[0]) - 0x8000)

// ===== video presentation options =====
static PsyzDitherMode dither_mode = PSYZ_DITHER_AUTO;
static PsyzAspectMode aspect_mode = PSYZ_ASPECT_DISPLAY;
static PsyzVsyncMode vsync_mode = PSYZ_VSYNC_AUTO;
static unsigned internal_res = 1;

// ===== frame state =====
static u16 cur_tpage;
static bool env_dither;
static bool can_dither;
static struct {
    int x, y;
} draw_offset, draw_area_start, draw_area_end, fb_origin;
static bool fb_origin_locked;
static bool display_enabled = true;
static bool warned_list_budget;
static int list_budget_calls;
static bool list_budget_cached = true;
static struct {
    int x, y;
} disp_origins[2];
static int disp_origin_count;

// ===== display defaults, same as a real PS1 =====
static int game_w = 256;         // GP1 PS1 register for display mode
static int game_h = 240;         // GP1 PS1 register for display mode
static int set_disp_horiz = 256; // Display range for aspect ratio
static int set_disp_vert = 240;  // Display range for aspect ratio
static bool is_pal = false; // I don't think we can ever run at 50fps on PSP

// Variables to draw pixel-perfect first, then scale aspect ratio horizontally
static int out_x, out_y, out_w = PSP_SCREEN_W, out_h = PSP_SCREEN_H;
static bool use_surface;   // false: draw straight to the framebuffer
static int surface_stride; // (game_w + 7) & ~7
static int surface_slot_bytes;
static int surface_idx;
static int target_w;       // presented width after horizontal aspect scale
static int pres_x, pres_y; // presented top-left; negative = centered crop
static int map_ofs_x, map_ofs_y; // base vertex pos, passthrough only
static PsyzRect draw_area_rect;  // {0,0,0,0} = centered (default)
static int cur_draw_fb;          // which framebuffer the GE draws to (0/1)
// framebuffer index of the frame finished but not yet displayed
static int show_fb = -1;
static int applied_draw_target = -1; // last EDRAM offset given to the GE list
static bool warned_surface_budget;

// ===== cached GE render state, to skip redundant commands =====
static int cur_blend = -1; // -1 none, 0-3 = PS1 ABR mode
static int cur_dither = -1;
static int cur_textured = -1;

static void FlushBatch(void);
static int EdramSurfaceOffset(int idx); // EDRAM offset of aspect surface slot
static void WaitPrevFrameGpu(void);
static void ShowPendingFrame(void); // display the deferred finished frame
static int FbOffset(int idx);
// set when a frame's GE list has been kicked but not yet waited on
static bool prev_frame_pending;
static bool store_readback_pending;
// GE queue id of that list, captured before sceGuStart overwrites it
static int prev_frame_list_id;
// set when a finished frame is waiting to be shown
static bool pending_show;
static unsigned int last_vsync; // vblank count at the last present, for pacing

// ===== GU_DIRECT packet emission to bypass sceGu wrappers =====
typedef struct {
    unsigned int* start;
    unsigned int* current;
    int parent_context;
} GuDisplayListRef;
extern GuDisplayListRef* gu_list;
extern int ge_list_executed[2]; // pspgu: the GE list id kicked by sceGuStart

// GE command opcodes (top byte of each command word)
#define GECMD_BASE 0x10
#define GECMD_VADR 0x01
#define GECMD_PRIM 0x04
#define GECMD_VTYPE 0x12
#define GECMD_JUMP 0x08
#define GECMD_CALL 0x0A

static inline void GeCmd(unsigned int cmd, unsigned int arg) {
    *(gu_list->current++) = (cmd << 24) | (arg & 0xffffff);
}

// sceGuDrawArray replacement: append VTYPE/BASE/VADR/PRIM directly (the caller
// issues the stall kick)
static inline void GuDrawArrayDirect(
    int prim, int vtype, int count, const void* vertices) {
    if (vtype) {
        GeCmd(GECMD_VTYPE, (unsigned int)vtype);
    }
    if (vertices) {
        GeCmd(GECMD_BASE, (((unsigned int)vertices) >> 8) & 0xf0000);
        GeCmd(GECMD_VADR, (unsigned int)vertices);
    }
    GeCmd(GECMD_PRIM, ((unsigned int)prim << 16) | (unsigned int)count);
}

// sceGuGetMemory replacement: carve scratch vertex space out of the packet
// list, with a BASE+JUMP pair in front so the GE skips the carved bytes
static inline void* GuGetMemoryDirect(int size) {
    size += 3;
    size += ((unsigned int)(size >> 31)) >> 30;
    size = (size >> 2) << 2;
    unsigned int* orig = gu_list->current;
    unsigned int* next = (unsigned int*)(((unsigned int)orig) + size + 8);
    orig[0] = (GECMD_BASE << 24) | ((((unsigned int)next) >> 8) & 0xf0000);
    orig[1] = (GECMD_JUMP << 24) | (((unsigned int)next) & 0xffffff);
    gu_list->current = next;
    return orig + 2;
}

// commands with no sceGu wrapper
#define GECMD_FBP 0x9C       // frame buffer pointer (EDRAM offset)
#define GECMD_FBW 0x9D       // frame buffer stride + address high bits
#define GECMD_CLEARMODE 0xD3 // fast-fill mode: (channel write enables<<8)|on

static void SetDrawTarget(int edram_offset, int stride) {
    if (applied_draw_target == edram_offset) {
        return;
    }
    applied_draw_target = edram_offset;

    // TODO re-introduce back sceGuDrawBufferList
    GeCmd(GECMD_FBP, (unsigned int)edram_offset);
    GeCmd(GECMD_FBW, (((unsigned int)edram_offset & 0xff000000) >> 8) |
                         (unsigned int)stride);
}

static void UpdateOutputMapping(void) {
    if (aspect_mode == PSYZ_ASPECT_SQUARE) {
        target_w = game_w;
    } else {
        // adjust aspect ratio
        float vref = is_pal ? 288.0f : 240.0f;
        float aspect = (4.0f / 3.0f) * ((float)set_disp_horiz / 256.0f) /
                       ((float)set_disp_vert / vref);
        target_w = (int)(aspect * (float)game_h + 0.5f);
    }

    // renders to a surface in the EDRAM, then re-renders it stretched.
    // bypassed if PSYZ_ASPECT_SQUARE, or if game resolution is 320x240
    use_surface = target_w != game_w && game_w > 0;
    if (use_surface) {
        surface_stride = (game_w + 7) & ~7;
        surface_slot_bytes = surface_stride * game_h * 2;
        int budget = (int)sceGeEdramGetSize() - EDRAM_SURFACE_OFFSET;
        // two slots: the surface is double-buffered for the deferred present
        if (surface_slot_bytes * 2 > budget) {
            // defensive: no realistic mode is this large. Present 1:1 instead.
            use_surface = false;
            target_w = game_w;
            if (!warned_surface_budget) {
                warned_surface_budget = true;
                WARNF("surface too large for EDRAM, presenting 1:1");
            }
        }
    }

    // placement of the presented output on the physical screen
    if (draw_area_rect.w > 0 && draw_area_rect.h > 0) {
        pres_x = draw_area_rect.x;
        pres_y = draw_area_rect.y;
    } else {
        pres_x = (PSP_SCREEN_W - target_w) / 2; // negative -> crop both sides
        pres_y = (PSP_SCREEN_H - game_h) / 2;   // 240 -> +16; 480 -> -104 crop
    }
    // presented rect clamped to the screen (present scissor and frame capture)
    int rx = pres_x, ry = pres_y, rw = target_w, rh = game_h;
    if (draw_area_rect.w > 0 && draw_area_rect.h > 0) {
        rw = draw_area_rect.w;
        rh = draw_area_rect.h;
    }
    int x0 = rx < 0 ? 0 : rx;
    int y0 = ry < 0 ? 0 : ry;
    int x1 = rx + rw > PSP_SCREEN_W ? PSP_SCREEN_W : rx + rw;
    int y1 = ry + rh > PSP_SCREEN_H ? PSP_SCREEN_H : ry + rh;
    out_x = x0;
    out_y = y0;
    out_w = x1 > x0 ? x1 - x0 : 0;
    out_h = y1 > y0 ? y1 - y0 : 0;
    map_ofs_x = use_surface ? 0 : pres_x;
    map_ofs_y = use_surface ? 0 : pres_y;

    // flush if display mode changes mid-frame
    if (is_init) {
        FlushBatch();
        if (use_surface) {
            SetDrawTarget(EdramSurfaceOffset(surface_idx), surface_stride);
        } else {
            SetDrawTarget(
                cur_draw_fb ? EDRAM_DISP_OFFSET : EDRAM_DRAW_OFFSET, FB_STRIDE);
        }
    }
}

typedef struct {
    u16 c;
    short x, y, z;
} CVert; // GU_COLOR_5551 | GU_VERTEX_16BIT
_Static_assert(sizeof(CVert) == 8, "CVert must be 8 bytes");

typedef struct {
    short u, v;
    u16 c;
    short x, y, z;
} BVert; // GU_TEXTURE_16BIT | GU_COLOR_5551 | GU_VERTEX_16BIT, 12 bytes packed
_Static_assert(sizeof(BVert) == 12, "BVert must be 12 bytes");

// Equivalent of 0xBDEF. Since GU_FRAGMENT_2X is enabled globally, the PSP GPU
// computes tex*col*2/255 and neutral is 0x7F per 8-bit channel, which is 15
// in 5 bits. Using 31 would double every texel and saturates half the range.
#define COL5551_MOD1X ((u16)(0x8000u | (15u << 10) | (15u << 5) | 15u))

// ===== primitive batching =====
#define MAX_VERTEX_COUNT 40960
static BVert __attribute__((aligned(64))) g_vring[2][MAX_VERTEX_COUNT];
static int vring_used;  // vertices produced this frame
static int batch_start; // first vertex of the pending (unflushed) batch
static bool warned_vring_budget;

static int batch_prim = GU_TRIANGLES; // GE primitive used for current batch
static unsigned int* last_kick; // list position of the last stall-address kick
static int vring_wb_mark;       // first ring vertex not yet written back

// queue commands to GPU, so both CPU and GPU can run asynchronously
static void KickGe(void) {
    if (gu_list->current == last_kick) {
        return;
    }
    if (vring_used > vring_wb_mark) {
        sceKernelDcacheWritebackRange(
            &g_vring[dlist_idx][vring_wb_mark],
            (vring_used - vring_wb_mark) * (int)sizeof(BVert));
        vring_wb_mark = vring_used;
    }
    sceKernelDcacheWritebackRange(
        last_kick, (char*)gu_list->current - (char*)last_kick);
    last_kick = gu_list->current;
    sceGeListUpdateStallAddr(ge_list_executed[0], gu_list->current);
}

static void CloseBatch(void) {
    int n = vring_used - batch_start;
    if (n == 0) {
        return;
    }
    BVert* first = &g_vring[dlist_idx][batch_start];
    batch_start = vring_used;
    GuDrawArrayDirect(
        batch_prim,
        GU_TEXTURE_16BIT | GU_COLOR_5551 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, n,
        first);
    if ((unsigned int)((char*)gu_list->current - (char*)last_kick) > 2048) {
        KickGe();
    }
}

static void FlushBatch(void) {
    CloseBatch();
    KickGe();
}

static void ScissorFull(void) {
    if (use_surface) {
        sceGuScissor(0, 0, surface_stride, game_h);
    } else {
        sceGuScissor(0, 0, PSP_SCREEN_W, PSP_SCREEN_H);
    }
}

static void UpdateScissor(void) {
    if (!is_init) {
        return;
    }
    CloseBatch(); // flush primitives using previous scissor settings
    // drawing area relative to the frame, in render-target coordinates (1:1)
    int x0 = map_ofs_x + draw_area_start.x - fb_origin.x;
    int y0 = map_ofs_y + draw_area_start.y - fb_origin.y;
    int x1 = map_ofs_x + draw_area_end.x + 1 - fb_origin.x;
    int y1 = map_ofs_y + draw_area_end.y + 1 - fb_origin.y;
    // clamp to the render target: the surface (0..game) or the presented rect
    int lo_x = use_surface ? 0 : out_x;
    int lo_y = use_surface ? 0 : out_y;
    int hi_x = use_surface ? game_w : out_x + out_w;
    int hi_y = use_surface ? game_h : out_y + out_h;
    if (x0 < lo_x) {
        x0 = lo_x;
    }
    if (y0 < lo_y) {
        y0 = lo_y;
    }
    if (x1 > hi_x) {
        x1 = hi_x;
    }
    if (y1 > hi_y) {
        y1 = hi_y;
    }
    if (x0 >= x1 || y0 >= y1) {
        sceGuScissor(0, 0, 0, 0);
    } else {
        sceGuScissor(x0, y0, x1 - x0, y1 - y0);
    }
}

// ===== VRAM access =====
static u8* edram_base; // sceGeEdramGetAddr has a fixed return value, get once.
static void* EdramTile(int tile) {
    return edram_base + EDRAM_TILES_OFFSET + tile * VRAM_TILE_BYTES;
}

static void ResetTextureBinding(void);

static void GeDrainAndSync(void) {
    store_readback_pending = false;
    WaitPrevFrameGpu();
    FlushBatch();
    int list_id = ge_list_executed[0];
    sceGuFinish();
    sceGeListSync(list_id, 0); // better perf than sceGuSync(0,0)
    sceGuStart(GU_DIRECT, dlist[dlist_idx]);
    last_kick = gu_list->current;
    applied_draw_target = EDRAM_DRAW_OFFSET;
    if (use_surface) {
        SetDrawTarget(EdramSurfaceOffset(surface_idx), surface_stride);
    } else {
        SetDrawTarget(FbOffset(cur_draw_fb), FB_STRIDE);
    }
    UpdateScissor();
    cur_blend = -1;
    cur_dither = -1;
    cur_textured = -1;
    ResetTextureBinding();
}

#define GE_FENCE_EDRAM_OFFSET (EDRAM_DISP_OFFSET + PSP_SCREEN_W * 2)
static u16 __attribute__((aligned(16))) ge_fence_src[8];
static int ge_signal_next;

// Stall the CPU until the GE consumed every command emitted so far
static void GeSyncCaughtUp(void) {
    if (!is_init) {
        GeDrainAndSync();
        return;
    }
    store_readback_pending = false;
    WaitPrevFrameGpu();
    CloseBatch();
    int id = (ge_signal_next++ & 0x7FFF) + 1; // never 0, fits a u16 token
    ge_fence_src[0] = (u16)id;
    sceKernelDcacheWritebackRange(ge_fence_src, sizeof(ge_fence_src));
    sceGuCopyImage(GU_PSM_5551, 0, 0, 8, 1, 8, ge_fence_src, 0, 0, 8,
                   edram_base + GE_FENCE_EDRAM_OFFSET);
    KickGe();
    volatile u16* fence = (u16*)UNCACHED(edram_base + GE_FENCE_EDRAM_OFFSET);
    while (*fence != (u16)id) {
        for (volatile int spin = 0; spin < 200; spin++) {
        }
    }
}

// Check if data is 16-byte aligned to use GE DMA
static bool IsAligned(const void* base, int stride_px) {
    return (((uintptr_t)base & 0xF) == 0) && ((stride_px & 7) == 0);
}

static inline u16* VramTileRow(int tile, int y) {
    return (u16*)UNCACHED(EdramTile(tile)) + ((y & (VRAM_TILE_H - 1)) << 6);
}

// CPU fallback for unaligned EDRAM access
static void VramLineWrite(int x, int y, const u16* src, int w) {
    while (w > 0) {
        int tile = (x >> 6) + (y >> 8) * VRAM_TILES_X;
        int tx = x & (VRAM_TILE_W - 1);
        int n = VRAM_TILE_W - tx;
        if (n > w) {
            n = w;
        }
        memcpy(VramTileRow(tile, y) + tx, src, n * sizeof(u16));
        src += n;
        x += n;
        w -= n;
    }
}

// CPU fallback for unaligned EDRAM access
static void VramLineRead(int x, int y, u16* dst, int w) {
    while (w > 0) {
        int tile = (x >> 6) + (y >> 8) * VRAM_TILES_X;
        int tx = x & (VRAM_TILE_W - 1);
        int n = VRAM_TILE_W - tx;
        if (n > w) {
            n = w;
        }
        memcpy(dst, VramTileRow(tile, y) + tx, n * sizeof(u16));
        dst += n;
        x += n;
        w -= n;
    }
}

static int EdramSurfaceOffset(int idx) {
    return EDRAM_SURFACE_OFFSET + idx * surface_slot_bytes;
}

static void* EdramSurface(int idx) {
    return edram_base + EdramSurfaceOffset(idx);
}

// ===== texture page and CLUT caches =====
typedef struct {
    bool valid;
    bool dirty;
    u8 page; // 0-31: VRAM cell (page & 0xF) * 64 halfwords, (page >> 4) * 256
    u8 bpp;  // 1=8bpp, 2=16bpp (4bpp pages bind the tile mirror instead)
    u32 last_used;
} PageEntry;

typedef struct {
    bool valid;
    bool dirty;
    u16 clut;
    u8 bpp; // 0=4bpp (16 colors), 1=8bpp (256 colors)
    u32 last_used;
} ClutEntry;

#define PAGECACHE_MAX 8
#define CLUTCACHE_MAX 64
static PageEntry page_cache[PAGECACHE_MAX];
static ClutEntry clut_cache[CLUTCACHE_MAX];
// one 256x256 16bpp page per slot (the worst case, 8bpp uses half); static
// so the deferred GE reads always see stable, 64-byte aligned storage
static u16 __attribute__((aligned(64))) page_bufs[PAGECACHE_MAX][256 * 256];
#define PageBuf(e) (page_bufs[(int)((e) - page_cache)])

// each CLUT is converted three ways to feed the GE alpha test the PS1 STP-bit
enum {
    CLUT_PLAIN,       // opaque primitives; only texel 0x0000 is transparent
    CLUT_OPAQUE_PASS, // semi-transparent pass 1; STP=1 texels are cut
    CLUT_BLEND_PASS,  // semi-transparent pass 2; only STP=1 texels blend
    NUM_CLUTS,
};

static u16 __attribute__((aligned(64))) g_clut[CLUTCACHE_MAX][NUM_CLUTS][256];
static u32 cache_clock;

static const void* bound_tex;
static int bound_tex_psm = -1;
static const void* bound_clut;
static bool tex_memo_valid;
static u16 tex_memo_tpage, tex_memo_clut;
static int tex_memo_variant, tex_memo_bpp;

static void ResetTextureBinding(void) {
    bound_tex = NULL;
    bound_tex_psm = -1;
    bound_clut = NULL;
    tex_memo_valid = false;
}

static void PageRect(const PageEntry* e, int* x, int* y, int* w, int* h) {
    *x = (e->page & 0xF) * 64;
    *y = (e->page >> 4) * 256;
    *w = 64 << e->bpp; // halfwords: 128 at 8bpp, 256 at 16bpp
    *h = 256;
}

static void InvalidateVram(int x, int y, int w, int h) {
    // a VRAM write may dirty the page/CLUT the memo points at; drop it
    tex_memo_valid = false;
    for (int i = 0; i < PAGECACHE_MAX; i++) {
        PageEntry* e = &page_cache[i];
        if (!e->valid || e->dirty) {
            continue;
        }
        int tx, ty, tw, th;
        PageRect(e, &tx, &ty, &tw, &th);
        if (x < tx + tw && x + w > tx && y < ty + th && y + h > ty) {
            e->dirty = true;
        }
    }
    for (int i = 0; i < CLUTCACHE_MAX; i++) {
        ClutEntry* e = &clut_cache[i];
        if (!e->valid || e->dirty) {
            continue;
        }
        int cx = (e->clut & 0x3F) * 16;
        int cy = (e->clut >> 6) & 0x1FF;
        int cw = e->bpp ? 256 : 16;
        if (x < cx + cw && x + w > cx && y <= cy && y + h > cy) {
            e->dirty = true;
        }
    }
}

// TODO is this required?! LoadImage and StoreImage are synced via DrawSync
static u16
    __attribute__((aligned(64))) page_stage[4][VRAM_TILE_W * VRAM_TILE_H];

// converts one 8bpp or 16bpp PS1 texture page from the VRAM tiles into the
// entry's main RAM buffer; 16bpp gets the transparency fixup (color 0x0000
// stays fully transparent, everything else becomes opaque)
static void AssemblePage(PageEntry* e) {
    int x, y, w, h;
    PageRect(e, &x, &y, &w, &h);
    int vw = w;
    if (x + vw > PSX_VRAM_W) {
        vw = PSX_VRAM_W - x; // pages at the right edge of VRAM
    }
    // the page x is 64-aligned, so it covers 1-4 whole tile columns that are
    // contiguous in EDRAM: fetch them all with a single GE copy
    int ntiles = (vw + VRAM_TILE_W - 1) >> 6;
    int t0 = (x >> 6) + (y >> 8) * VRAM_TILES_X;
    sceKernelDcacheWritebackInvalidateRange(
        page_stage, ntiles * VRAM_TILE_BYTES);
    sceGuCopyImage(GU_PSM_5551, 0, 0, VRAM_TILE_W, ntiles * VRAM_TILE_H,
                   VRAM_TILE_W, EdramTile(t0), 0, 0, VRAM_TILE_W, page_stage);
    // waits until the copy above lands in page_stage
    GeSyncCaughtUp();
    u16* buf = PageBuf(e);
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < vw; j += VRAM_TILE_W) {
            const u16* src = &page_stage[j >> 6][i << 6];
            int n = vw - j < VRAM_TILE_W ? vw - j : VRAM_TILE_W;
            if (e->bpp == 2) {
                u16* dst = buf + i * 256 + j;
                for (int k = 0; k < n; k++) {
                    u16 c = src[k];
                    dst[k] = c ? (u16)(c | 0x8000) : 0;
                }
            } else {
                memcpy((u8*)buf + i * 256 + j * 2, src, n * 2);
            }
        }
        if (vw < w) {
            if (e->bpp == 2) {
                memset(buf + i * 256 + vw, 0, (w - vw) * 2);
            } else {
                memset((u8*)buf + i * 256 + vw * 2, 0, (w - vw) * 2);
            }
        }
    }
    sceKernelDcacheWritebackRange(buf, 256 * (e->bpp == 2 ? 512 : 256));
}

static PageEntry* FindOrCreatePage(u8 page, u8 bpp) {
    PageEntry* lru = NULL;
    PageEntry* found = NULL;
    for (int i = 0; i < PAGECACHE_MAX; i++) {
        PageEntry* e = &page_cache[i];
        if (e->valid && e->page == page && e->bpp == bpp) {
            found = e;
            break;
        }
        if (!e->valid) {
            if (!lru || lru->valid) {
                lru = e;
            }
        } else if (!lru || (lru->valid && e->last_used < lru->last_used)) {
            lru = e;
        }
    }
    if (!found) {
        found = lru;
        found->valid = true;
        found->dirty = true;
        found->page = page;
        found->bpp = bpp;
    }
    if (found->dirty) {
        AssemblePage(found);
        found->dirty = false;
    }
    found->last_used = ++cache_clock;
    return found;
}

static ClutEntry* FindOrCreateClut(u16 clut, u8 bpp) {
    ClutEntry* lru = NULL;
    ClutEntry* found = NULL;
    for (int i = 0; i < CLUTCACHE_MAX; i++) {
        ClutEntry* e = &clut_cache[i];
        if (e->valid && e->clut == clut && e->bpp == bpp) {
            found = e;
            break;
        }
        if (!e->valid) {
            if (!lru || lru->valid) {
                lru = e;
            }
        } else if (!lru || (lru->valid && e->last_used < lru->last_used)) {
            lru = e;
        }
    }
    if (!found) {
        found = lru;
        found->valid = true;
        found->dirty = true;
        found->clut = clut;
        found->bpp = bpp;
    }
    if (found->dirty) {
        GeSyncCaughtUp();
        int idx = (int)(found - clut_cache);
        int cx = (clut & 0x3F) * 16;
        int cy = (clut >> 6) & 0x1FF;
        int colors = bpp ? 256 : 16;
        if (cx + colors > PSX_VRAM_W) {
            colors = PSX_VRAM_W - cx;
        }
        u16 row[256];
        VramLineRead(cx, cy, row, colors);
        for (int i = 0; i < colors; i++) {
            u16 c = row[i];
            u16 opaque = (u16)(c | 0x8000);
            u16 cut = (u16)(c & 0x7FFF);
            g_clut[idx][CLUT_PLAIN][i] = c ? opaque : 0;
            g_clut[idx][CLUT_OPAQUE_PASS][i] =
                c && !(c & 0x8000) ? opaque : cut;
            g_clut[idx][CLUT_BLEND_PASS][i] = c & 0x8000 ? opaque : cut;
        }
        sceKernelDcacheWritebackRange(g_clut[idx], sizeof(g_clut[0]));
        found->dirty = false;
    }
    found->last_used = ++cache_clock;
    return found;
}

// ===== GE bring-up and frame loop =====
static const ScePspIMatrix4 psx_dither_matrix = {
    {-4, 0, -3, 1},
    {2, -2, 3, -1},
    {-3, 1, -4, 0},
    {3, -1, 2, -2},
};

static void ClearTargetBlack(int w, int h) {
    ScissorFull();
    GeCmd(GECMD_CLEARMODE, (GU_COLOR_BUFFER_BIT << 8) | 1);
    CVert* out = GuGetMemoryDirect(2 * sizeof(CVert));
    out[0].c = 0x8000;
    out[0].x = 0;
    out[0].y = 0;
    out[0].z = 0;
    out[1].c = 0x8000;
    out[1].x = (short)w;
    out[1].y = (short)h;
    out[1].z = 0;
    GuDrawArrayDirect(
        GU_SPRITES, GU_COLOR_5551 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, out);
    GeCmd(GECMD_CLEARMODE, 0);
}

static void StartFrame(void) {
    fb_origin_locked = false;
    warned_list_budget = false;
    vring_used = 0;
    vring_wb_mark = 0;
    batch_start = 0;
    batch_prim = GU_TRIANGLES;
    warned_vring_budget = false;
    list_budget_calls = 0;
    list_budget_cached = true;
    cur_blend = -1;
    cur_dither = -1;
    cur_textured = -1;
    bound_tex = NULL;
    bound_tex_psm = -1;
    bound_clut = NULL;
    tex_memo_valid = false;

    cur_draw_fb ^= 1;
    sceGuStart(GU_DIRECT, dlist[dlist_idx]);
    last_kick = gu_list->current;
    applied_draw_target = EDRAM_DRAW_OFFSET;
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_DITHER);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_ALPHA_TEST);
    if (use_surface) {
        SetDrawTarget(EdramSurfaceOffset(surface_idx), surface_stride);
        ClearTargetBlack(surface_stride, game_h);
    } else {
        SetDrawTarget(FbOffset(cur_draw_fb), FB_STRIDE);
        ClearTargetBlack(PSP_SCREEN_W, PSP_SCREEN_H);
    }
    UpdateScissor();
}

static int Pow2Ceil(int v) {
    int p = 1;
    while (p < v) {
        p <<= 1;
    }
    return p;
}

static void PresentSurface(void) {
    SetDrawTarget(FbOffset(cur_draw_fb), FB_STRIDE);
    ClearTargetBlack(PSP_SCREEN_W, PSP_SCREEN_H); // letterbox/pillarbox
    if (out_w <= 0 || out_h <= 0) {
        return; // draw area fully offscreen
    }
    sceGuScissor(out_x, out_y, out_w, out_h);
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_DITHER);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_5551, 0, 0, 0);
    sceGuTexImage(0, Pow2Ceil(game_w), Pow2Ceil(game_h), surface_stride,
                  EdramSurface(surface_idx));
    BVert* out = GuGetMemoryDirect(2 * sizeof(BVert));
    out[0].u = 0;
    out[0].v = 0;
    out[0].c = COL5551_MOD1X;
    out[0].x = (short)pres_x;
    out[0].y = (short)pres_y;
    out[0].z = 0;
    out[1].u = (short)game_w;
    out[1].v = (short)game_h;
    out[1].c = COL5551_MOD1X;
    out[1].x = (short)(pres_x + target_w);
    out[1].y = (short)(pres_y + game_h);
    out[1].z = 0;
    GuDrawArrayDirect(
        GU_SPRITES,
        GU_TEXTURE_16BIT | GU_COLOR_5551 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2,
        out);
}

bool InitPlatform(void) {
    if (is_init) {
        return true;
    }
    edram_base = (u8*)sceGeEdramGetAddr();
    memset(UNCACHED(edram_base), 0, EDRAM_SURFACE_OFFSET);
    sceGuInit();
    sceGuStart(GU_DIRECT, dlist[0]);
    sceGuDrawBuffer(GU_PSM_5551, (void*)EDRAM_DRAW_OFFSET, FB_STRIDE);
    sceGuDispBuffer(
        PSP_SCREEN_W, PSP_SCREEN_H, (void*)EDRAM_DISP_OFFSET, FB_STRIDE);
    sceGuOffset(2048 - (PSP_SCREEN_W / 2), 2048 - (PSP_SCREEN_H / 2));
    sceGuViewport(2048, 2048, PSP_SCREEN_W, PSP_SCREEN_H);
    sceGuScissor(0, 0, PSP_SCREEN_W, PSP_SCREEN_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDepthMask(GU_TRUE);
    sceGuShadeModel(GU_SMOOTH);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_BLEND);
    sceGuSetDither(&psx_dither_matrix);
    sceGuDisable(GU_DITHER);
    sceGuEnable(GU_FRAGMENT_2X); // use 0x7F is 1.0 of brightness
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_REPEAT, GU_REPEAT);  // UVs wrap within a 256x256 page
    sceGuAlphaFunc(GU_GREATER, 0, 0xFF); // discard 0x0000 colors
    sceGuDisable(GU_ALPHA_TEST);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    is_init = true;
    UpdateOutputMapping();
    StartFrame();
    return true;
}

void ResetPlatform(void) {
    if (is_init) {
        sceGuTerm();
        is_init = false;
    }
    cur_tpage = 0;
    internal_res = 1;
}

static void EmitClearQuad(int x, int y, int w, int h, u8 r, u8 g, u8 b);

static struct {
    bool valid;
    short x, y, w, h;
    u8 r, g, b;
} pending_clear;

static inline void FlushPendingClear(void) {
    if (!pending_clear.valid) {
        return;
    }
    // the on-screen ClearImage is a fire-and-forget, and must be flushed.
    pending_clear.valid = false;
    EmitClearQuad(
        pending_clear.x, pending_clear.y, pending_clear.w, pending_clear.h,
        pending_clear.r, pending_clear.g, pending_clear.b);
}

// ===== vsync, pacing, video API =====

// wait for the previous frame's GE list before its buffers get reused
static void WaitPrevFrameGpu(void) {
    if (!prev_frame_pending) {
        return;
    }
    prev_frame_pending = false;
    // never sceGeDrawSync: it would also wait on the still-open list
    sceGeListSync(prev_frame_list_id, 0);
}

// EDRAM byte offset of framebuffer index
static int FbOffset(int idx) {
    return idx ? EDRAM_DISP_OFFSET : EDRAM_DRAW_OFFSET;
}

// point the display at show_fb, which has the frame ready to show on screen
static void ShowPendingFrame(void) {
    if (!pending_show) {
        return;
    }
    pending_show = false;
    if (vsync_mode != PSYZ_VSYNC_LIMITLESS) {
        sceDisplayWaitVblankStart();
    }
    sceDisplaySetFrameBuf(
        (void*)(edram_base + FbOffset(show_fb)), FB_STRIDE,
        PSP_DISPLAY_PIXEL_FORMAT_5551, PSP_DISPLAY_SETBUF_IMMEDIATE);
    sceGuDisplay(display_enabled ? GU_TRUE : GU_FALSE);
    last_vsync = sceDisplayGetVcount();
}

int Psyz_VideoVSync(int mode) {
    if (!is_init && !InitPlatform()) {
        return 0;
    }
    unsigned int cur = sceDisplayGetVcount();
    int ret = (int)((cur - last_vsync) * 1000 / 60); // TODO logic incorrect?
    last_vsync = cur;
    if (mode == 0) {
        WaitPrevFrameGpu();
        ShowPendingFrame();

        FlushPendingClear();
        FlushBatch();
        if (use_surface) {
            PresentSurface();
        }
        sceGuFinish();
        prev_frame_list_id = ge_list_executed[0];
        prev_frame_pending = true;
        show_fb = cur_draw_fb;
        pending_show = true;
        dlist_idx ^= 1;
        surface_idx ^= 1;
        StartFrame();
        if (ge_list_executed[0] == prev_frame_list_id) {
            prev_frame_pending = false;
        }
    }
    return ret;
}

int Psyz_VideoSetVsyncMode(PsyzVsyncMode mode) {
    switch (mode) {
    case PSYZ_VSYNC_AUTO:
    case PSYZ_VSYNC_ON:
    case PSYZ_VSYNC_OFF:
    case PSYZ_VSYNC_LIMITLESS:
        vsync_mode = mode;
        return 0;
    default:
        return -1;
    }
}

PsyzVsyncMode Psyz_VideoGetVsyncMode(void) { return vsync_mode; }

int Psyz_VideoSetDitheringMode(PsyzDitherMode mode) {
    switch (mode) {
    case PSYZ_DITHER_AUTO:
    case PSYZ_DITHER_OFF:
        dither_mode = mode;
        return 0;
    default:
        return -1;
    }
}

PsyzDitherMode Psyz_VideoGetDitheringMode(void) { return dither_mode; }

int Psyz_VideoSetAspectMode(PsyzAspectMode mode) {
    switch (mode) {
    case PSYZ_ASPECT_DISPLAY:
    case PSYZ_ASPECT_SQUARE:
        aspect_mode = mode;
        UpdateOutputMapping();
        UpdateScissor();
        return 0;
    default:
        return -1;
    }
}

PsyzAspectMode Psyz_VideoGetAspectMode(void) { return aspect_mode; }

PsyzSize Psyz_VideoGetDisplaySize(void) {
    PsyzSize s = {PSP_SCREEN_W, PSP_SCREEN_H};
    if (aspect_mode == PSYZ_ASPECT_DISPLAY) {
        // largest 4:3 area at 272 tall
        // keeps the output pixel-perfect while preserving the aspect ratio
        s.w = 362;
    }
    return s;
}

void Psyz_VideoSetDrawArea(PsyzRect rect) {
    draw_area_rect = rect;
    UpdateOutputMapping();
    UpdateScissor();
}

int Psyz_VideoStats(PsyzVideoStats* stats) {
    if (!stats || !is_init) {
        return -1;
    }
    memset(stats, 0, sizeof(*stats));
    stats->target_frame_time_us = 1000000.0 / 60.0;
    stats->total_frames = sceDisplayGetVcount();
    stats->using_driver_vsync = vsync_mode != PSYZ_VSYNC_LIMITLESS;
    return 0;
}

static unsigned char* AllocRgb888Region(
    int offset, int bufw, int x, int y, int w, int h) {
    const u16* fb = (const u16*)(0x40000000 | (uintptr_t)(edram_base + offset));
    unsigned char* out = malloc((size_t)w * h * 3);
    if (!out) {
        return NULL;
    }
    unsigned char* p = out;
    for (int j = 0; j < h; j++) {
        const u16* row = &fb[(y + j) * bufw + x];
        for (int i = 0; i < w; i++) {
            u16 c = row[i];
            u8 r5 = c & 0x1F;
            u8 g5 = (c >> 5) & 0x1F;
            u8 b5 = (c >> 10) & 0x1F;
            *p++ = (u8)((r5 << 3) | (r5 >> 2));
            *p++ = (u8)((g5 << 3) | (g5 >> 2));
            *p++ = (u8)((b5 << 3) | (b5 >> 2));
        }
    }
    return out;
}

unsigned char* Psyz_VideoAllocCapturedFrame(int* w, int* h) {
    if (!is_init && !InitPlatform()) {
        return NULL;
    }
    WaitPrevFrameGpu();
    int fb_idx = show_fb >= 0 ? show_fb : cur_draw_fb;
    unsigned char* out = AllocRgb888Region(
        FbOffset(fb_idx), FB_STRIDE, out_x, out_y, out_w, out_h);
    if (!out) {
        return NULL;
    }
    if (w) {
        *w = out_w;
    }
    if (h) {
        *h = out_h;
    }
    return out;
}

unsigned char* Psyz_VideoAllocVramDump(int* w, int* h) {
    // TODO feasibility of returning the edram pointer directly?
    if (!is_init && !InitPlatform()) {
        return NULL;
    }
    WaitPrevFrameGpu();
    int dump_w = FB_STRIDE;
    int dump_h = (int)sceGeEdramGetSize() / (dump_w * 2);
    unsigned char* out = AllocRgb888Region(0, dump_w, 0, 0, dump_w, dump_h);
    if (!out) {
        return NULL;
    }
    if (w) {
        *w = dump_w;
    }
    if (h) {
        *h = dump_h;
    }
    return out;
}

// ===== display state =====

void Draw_Reset(void) {
    if (!is_init && !InitPlatform()) {
        return;
    }
    cur_tpage = 0;
    env_dither = false;
    draw_offset.x = 0;
    draw_offset.y = 0;
    draw_area_start.x = 0;
    draw_area_start.y = 0;
    draw_area_end.x = game_w - 1;
    draw_area_end.y = game_h - 1;
    fb_origin.x = 0;
    fb_origin.y = 0;
    disp_origin_count = 0;
    display_enabled = true;
    UpdateOutputMapping();
    UpdateScissor();
}

void Draw_DisplayEnable(unsigned int on) { display_enabled = on != 0; }

static void TryLockFbOrigin(void);

void Draw_DisplayArea(unsigned int x, unsigned int y) {
    // remember the framebuffer locations (typically two, alternating)
    for (int i = 0; i < disp_origin_count; i++) {
        if (disp_origins[i].x == (int)x && disp_origins[i].y == (int)y) {
            return;
        }
    }
    if (disp_origin_count < 2) {
        disp_origins[disp_origin_count].x = (int)x;
        disp_origins[disp_origin_count].y = (int)y;
        disp_origin_count++;
    } else {
        // a third origin: the game moved its framebuffers, forget the oldest
        disp_origins[0] = disp_origins[1];
        disp_origins[1].x = (int)x;
        disp_origins[1].y = (int)y;
    }
    // a game sending the draw env before the disp env locks the origin now
    if (!fb_origin_locked) {
        TryLockFbOrigin();
        if (fb_origin_locked) {
            UpdateScissor();
        }
    }
}

void Draw_DisplayHorizontalRange(unsigned int start, unsigned int end) {
    set_disp_horiz = (int)(end - start) / 10;
    UpdateOutputMapping();
    UpdateScissor();
}

void Draw_DisplayVerticalRange(unsigned int start, int unsigned end) {
    set_disp_vert = (int)(end - start);
    UpdateOutputMapping();
    UpdateScissor();
}

void Draw_SetDisplayMode(DisplayMode* mode) {
    if (mode->reversed) {
        WARNF("reverse mode not supported");
    }
    if (mode->rgb24) {
        LOG_ONCE("24-bit display mode not supported");
    }
    is_pal = mode->pal;
    if (mode->horizontal_resolution_368) {
        game_w = 368;
    } else {
        switch (mode->horizontal_resolution) {
        case 0:
            game_w = 256;
            break;
        case 1:
            game_w = 320;
            break;
        case 2:
            game_w = 512;
            break;
        case 3:
            game_w = 640;
            break;
        }
    }
    game_h = mode->vertical_resolution ? 480 : 240;
    UpdateOutputMapping();
    UpdateScissor();
}

void Draw_PutDispEnv(DISPENV* disp) { (void)disp; }

void Draw_SetTexpageMode(ParamDrawTexpageMode* p) {
    cur_tpage = *(u_short*)p & 0x1FF;
    env_dither = p->dither != 0;
    if (p->tex_flip_x || p->tex_flip_y) {
        LOG_ONCE("texture page flipping not implemented");
    }
}

void Draw_SetTextureWindow(unsigned int mask_x, unsigned int mask_y,
                           unsigned int off_x, unsigned int off_y) {
    if (off_x > 0 || off_y > 0 || mask_x != 0xFFFFFFFF ||
        mask_y != 0xFFFFFFFF) {
        NOT_IMPLEMENTED;
    }
}

static void TryLockFbOriginReal(void) {
    int x = draw_area_start.x, y = draw_area_start.y;
    int origin_x = x, origin_y = y;
    bool is_framebuffer = disp_origin_count == 0;
    for (int i = 0; i < disp_origin_count; i++) {
        if (x >= disp_origins[i].x && x < disp_origins[i].x + game_w &&
            y >= disp_origins[i].y && y < disp_origins[i].y + game_h) {
            origin_x = disp_origins[i].x;
            origin_y = disp_origins[i].y;
            is_framebuffer = true;
            break;
        }
    }
    if (is_framebuffer) {
        fb_origin_locked = true;
        fb_origin.x = origin_x;
        fb_origin.y = origin_y;
    }
}

static inline void TryLockFbOrigin(void) {
    if (fb_origin_locked) {
        return;
    }
    TryLockFbOriginReal();
}

void Draw_SetAreaStart(int x, int y) {
    draw_area_start.x = x;
    draw_area_start.y = y;
    TryLockFbOrigin();
    UpdateScissor();
}

void Draw_SetAreaEnd(int x, int y) {
    draw_area_end.x = x;
    draw_area_end.y = y;
    UpdateScissor();
}

void Draw_SetOffset(int x, int y) {
    x = x % PSX_VRAM_W;
    y = y % PSX_VRAM_H;
    if (x < 0) {
        x += PSX_VRAM_W;
    }
    if (y < 0) {
        y += PSX_VRAM_H;
    }
    draw_offset.x = x;
    draw_offset.y = y;
}

void Draw_SetMask(int bit0, int bit1) {
    if (bit0 || bit1) {
        NOT_IMPLEMENTED;
    }
}

// ===== image transfers =====

static void ClampRect(const RECT* in, int* x, int* y, int* w, int* h) {
    *x = in->x & (PSX_VRAM_W - 1);
    *y = in->y & (PSX_VRAM_H - 1);
    *w = in->w;
    *h = in->h;
    if (*x + *w > PSX_VRAM_W) {
        *w = PSX_VRAM_W - *x;
    }
    if (*y + *h > PSX_VRAM_H) {
        *h = PSX_VRAM_H - *y;
    }
}

static void EmitVramFill(int x, int y, int w, int h, u8 r, u8 g, u8 b);
void Draw_ClearImage(RECT* rect, u_char r, u_char g, u_char b) {
    int x, y, w, h;
    if (rect->w == 0 || rect->h == 0) {
        return;
    }
    if (!is_init && !InitPlatform()) {
        return;
    }
    FlushPendingClear();
    ClampRect(rect, &x, &y, &w, &h);
    EmitVramFill(x, y, w, h, r, g, b);
    InvalidateVram(x, y, w, h);
    pending_clear.valid = true;
    pending_clear.x = (short)x;
    pending_clear.y = (short)y;
    pending_clear.w = (short)w;
    pending_clear.h = (short)h;
    pending_clear.r = r;
    pending_clear.g = g;
    pending_clear.b = b;
}

// Staging arena for Load/StoreImage when the caller's stride is not a
// multiple of 8: rows are repacked at a padded stride so the GE transfer can
// run instead of the uncached CPU line loop. Rects that don't fit fall back
// to the slowest CPU path.
#define XFER_STRIDE 1024
#define XFER_ROWS 128
static u16 __attribute__((aligned(64))) xfer_stage[XFER_STRIDE * XFER_ROWS];

// Tell the GPU to copy the image in chunks via DMA transfers
static void LoadImageGe(
    int x, int y, int w, int h, const u16* src, int src_stride) {
    FlushBatch();
    sceKernelDcacheWritebackRange((void*)src, (((h - 1) * src_stride) + w) * 2);
    int xe = x + w, ye = y + h;
    for (int ty = y >> 8; ty <= (ye - 1) >> 8; ty++) {
        for (int tx = x >> 6; tx <= (xe - 1) >> 6; tx++) {
            int x0 = tx << 6, y0 = ty << 8;
            int gx0 = x > x0 ? x : x0;
            int gy0 = y > y0 ? y : y0;
            int gx1 = xe < x0 + VRAM_TILE_W ? xe : x0 + VRAM_TILE_W;
            int gy1 = ye < y0 + VRAM_TILE_H ? ye : y0 + VRAM_TILE_H;
            sceGuCopyImage(GU_PSM_5551, gx0 - x, gy0 - y, gx1 - gx0, gy1 - gy0,
                           src_stride, (void*)src, gx0 - x0, gy0 - y0,
                           VRAM_TILE_W, EdramTile(tx + ty * VRAM_TILES_X));
        }
    }
    KickGe();
}

static void StoreImageGe(int x, int y, int w, int h, u16* dst, int dst_stride) {
    FlushBatch();
    int xe = x + w, ye = y + h;
    for (int ty = y >> 8; ty <= (ye - 1) >> 8; ty++) {
        for (int tx = x >> 6; tx <= (xe - 1) >> 6; tx++) {
            int x0 = tx << 6, y0 = ty << 8;
            int gx0 = x > x0 ? x : x0;
            int gy0 = y > y0 ? y : y0;
            int gx1 = xe < x0 + VRAM_TILE_W ? xe : x0 + VRAM_TILE_W;
            int gy1 = ye < y0 + VRAM_TILE_H ? ye : y0 + VRAM_TILE_H;
            sceGuCopyImage(
                GU_PSM_5551, gx0 - x0, gy0 - y0, gx1 - gx0, gy1 - gy0,
                VRAM_TILE_W, EdramTile(tx + ty * VRAM_TILES_X), gx0 - x,
                gy0 - y, dst_stride, dst);
        }
    }
}

static void EmitVramRegionQuad(int x, int y, int w, int h);
void Draw_LoadImage(RECT* rect, u_long* p) {
    int x, y, w, h;
    if (rect->w == 0 || rect->h == 0) {
        return;
    }
    if (!is_init && !InitPlatform()) {
        return;
    }
    ClampRect(rect, &x, &y, &w, &h);
    const u16* src = (const u16*)p;
    int pad_stride = (rect->w + 7) & ~7;
    if (IsAligned(src, rect->w)) {
        // data and width are aligned, best case scenario
        LoadImageGe(x, y, w, h, src, rect->w);
    } else if (((uintptr_t)src & 0xF) == 0 && h <= XFER_ROWS &&
               pad_stride <= XFER_STRIDE) {
        // width is unaligned, do a mix between CPU copy and DMA copy
        for (int i = 0; i < h; i++) {
            memcpy(xfer_stage + (size_t)i * pad_stride,
                   src + (size_t)i * rect->w, w * sizeof(u16));
        }
        LoadImageGe(x, y, w, h, xfer_stage, pad_stride);
    } else {
        // worst scenario, data is unaligned, perform CPU copy
        GeSyncCaughtUp();
        for (int i = 0; i < h; i++) {
            VramLineWrite(x, y + i, src, w);
            src += rect->w;
        }
    }
    InvalidateVram(x, y, w, h);
    EmitVramRegionQuad(x, y, w, h);
}

void Draw_StoreImage(RECT* rect, u_long* p) {
    int x, y, w, h;
    if (rect->w == 0 || rect->h == 0) {
        return;
    }
    if (!is_init && !InitPlatform()) {
        return;
    }
    ClampRect(rect, &x, &y, &w, &h);
    u16* dst = (u16*)p;
    int pad_stride = (rect->w + 7) & ~7;
    if (IsAligned(dst, rect->w)) {
        // data and width are aligned, best case scenario
        sceKernelDcacheWritebackInvalidateRange(
            dst, (((h - 1) * rect->w) + w) * 2);
        StoreImageGe(x, y, w, h, dst, rect->w);
        KickGe();
        store_readback_pending = true;
    } else if (((uintptr_t)dst & 0xF) == 0 && h <= XFER_ROWS &&
               pad_stride <= XFER_STRIDE) {
        // width is unaligned, do a mix between CPU copy and DMA copy
        sceKernelDcacheWritebackInvalidateRange(
            xfer_stage, (size_t)h * pad_stride * sizeof(u16));
        StoreImageGe(x, y, w, h, xfer_stage, pad_stride);
        GeSyncCaughtUp();
        for (int i = 0; i < h; i++) {
            memcpy(dst + (size_t)i * rect->w,
                   xfer_stage + (size_t)i * pad_stride, w * sizeof(u16));
        }
    } else {
        // worst scenario, data is unaligned, perform CPU copy
        GeSyncCaughtUp();
        for (int i = 0; i < h; i++) {
            VramLineRead(x, y + i, dst, w);
            dst += rect->w;
        }
    }
}

static void SyncFbToTiles(int x, int y, int w, int h) {
    if (!is_init || !display_enabled) {
        return;
    }
    FlushBatch();
    // clip to the region the framebuffer actually backs
    int x0 = x > fb_origin.x ? x : fb_origin.x;
    int y0 = y > fb_origin.y ? y : fb_origin.y;
    int x1 = x + w < fb_origin.x + game_w ? x + w : fb_origin.x + game_w;
    int y1 = y + h < fb_origin.y + game_h ? y + h : fb_origin.y + game_h;
    if (x0 >= x1 || y0 >= y1) {
        return;
    }
    void* src;
    int stride;
    if (use_surface) {
        src = EdramSurface(surface_idx);
        stride = surface_stride;
    } else {
        src = edram_base + FbOffset(cur_draw_fb);
        stride = FB_STRIDE;
    }
    // split along tile boundaries: each copy stays inside one destination tile
    for (int py = y0; py < y1;) {
        int tile_y = py & ~(VRAM_TILE_H - 1);
        int ny = tile_y + VRAM_TILE_H < y1 ? tile_y + VRAM_TILE_H : y1;
        for (int px = x0; px < x1;) {
            int tile_x = px & ~(VRAM_TILE_W - 1);
            int nx = tile_x + VRAM_TILE_W < x1 ? tile_x + VRAM_TILE_W : x1;
            int tile = (tile_x >> 6) + (tile_y >> 8) * VRAM_TILES_X;
            sceGuCopyImage(
                GU_PSM_5551, px - fb_origin.x + map_ofs_x,
                py - fb_origin.y + map_ofs_y, nx - px, ny - py, stride, src,
                px - tile_x, py - tile_y, VRAM_TILE_W, EdramTile(tile));
            px = nx;
        }
        py = ny;
    }
    KickGe();
}

void Draw_MoveImage(RECT* rect, unsigned int x, unsigned int y) {
    int sx, sy, w, h;
    if (rect->w == 0 || rect->h == 0) {
        return;
    }
    if (!is_init && !InitPlatform()) {
        return;
    }
    ClampRect(rect, &sx, &sy, &w, &h);
    int dx = (int)x & (PSX_VRAM_W - 1);
    int dy = (int)y & (PSX_VRAM_H - 1);
    if (dx + w > PSX_VRAM_W) {
        w = PSX_VRAM_W - dx;
    }
    if (dy + h > PSX_VRAM_H) {
        h = PSX_VRAM_H - dy;
    }
    SyncFbToTiles(sx, sy, w, h); // source may have been rendered this frame
    if (sx + w <= dx || dx + w <= sx || sy + h <= dy || dy + h <= sy) {
        // async GE copies done in chunks
        int oy = 0;
        while (oy < h) {
            int sry = (sy + oy) & (VRAM_TILE_H - 1);
            int dry = (dy + oy) & (VRAM_TILE_H - 1);
            int bh = VRAM_TILE_H - (sry > dry ? sry : dry);
            if (bh > h - oy) {
                bh = h - oy;
            }
            int ox = 0;
            while (ox < w) {
                int srx = (sx + ox) & (VRAM_TILE_W - 1);
                int drx = (dx + ox) & (VRAM_TILE_W - 1);
                int bw = VRAM_TILE_W - (srx > drx ? srx : drx);
                if (bw > w - ox) {
                    bw = w - ox;
                }
                int stile = ((sx + ox) >> 6) + ((sy + oy) >> 8) * VRAM_TILES_X;
                int dtile = ((dx + ox) >> 6) + ((dy + oy) >> 8) * VRAM_TILES_X;
                sceGuCopyImage(
                    GU_PSM_5551, srx, sry, bw, bh, VRAM_TILE_W,
                    EdramTile(stile), drx, dry, VRAM_TILE_W, EdramTile(dtile));
                ox += bw;
            }
            oy += bh;
        }
        KickGe();
    } else {
        // overlapping: the GE cannot copy in place, do it on the CPU
        GeSyncCaughtUp();
        u16 row[PSX_VRAM_W];
        if (dy > sy) {
            for (int i = h - 1; i >= 0; i--) {
                VramLineRead(sx, sy + i, row, w);
                VramLineWrite(dx, dy + i, row, w);
            }
        } else {
            for (int i = 0; i < h; i++) {
                VramLineRead(sx, sy + i, row, w);
                VramLineWrite(dx, dy + i, row, w);
            }
        }
    }
    InvalidateVram(dx, dy, w, h);
    EmitVramRegionQuad(dx, dy, w, h);
}

// ===== primitive rendering =====

typedef struct {
    short x, y;
    u16 u, v;
    u8 r, g, b, a; // a: 0x80 = semi-transparent primitive, 0xFF = opaque
} PVert;

// reserve n ring vertices for prim, close previous batch if primitive change
static BVert* BatchAlloc(int prim, int n) {
    if (batch_prim != prim) {
        CloseBatch();
        batch_prim = prim;
    }
    if (vring_used + n > MAX_VERTEX_COUNT) {
        if (!warned_vring_budget) {
            warned_vring_budget = true;
            WARNF("vertex ring exhausted, dropping primitives");
        }
        return NULL;
    }
    BVert* out = &g_vring[dlist_idx][vring_used];
    vring_used += n;
    return out;
}

static inline void BatchTri(const BVert* vs, int a, int b, int c) {
    BVert* out = BatchAlloc(GU_TRIANGLES, 3);
    if (out) {
        out[0] = vs[a];
        out[1] = vs[b];
        out[2] = vs[c];
    }
}

static inline void BatchPair(int prim, const BVert* v0, const BVert* v1) {
    BVert* out = BatchAlloc(prim, 2);
    if (out) {
        out[0] = *v0;
        out[1] = *v1;
    }
}

static bool ListBudget(void) {
    if (++list_budget_calls < 16 && list_budget_cached) {
        return true;
    }
    list_budget_calls = 0;
    // sceGuCheckList returns how much of the active list is used, in words
    if (sceGuCheckList() * 4u > DLIST_WATERMARK) {
        if (!warned_list_budget) {
            warned_list_budget = true;
            WARNF("display list budget exhausted, dropping primitives");
        }
        list_budget_cached = false;
        return false;
    }
    list_budget_cached = true;
    return true;
}

static void ApplyBlendReal(int mode) {
    CloseBatch();
    cur_blend = mode;
    if (mode < 0) {
        sceGuDisable(GU_BLEND);
        return;
    }
    // pre-computed ABR modes as fixed-factor blends to match PS1 algorithms
    static const struct {
        int op;
        unsigned int afix, bfix;
    } abr[4] = {
        {GU_ADD, 0x808080, 0x808080},              // B/2+F/2
        {GU_ADD, 0xFFFFFF, 0xFFFFFF},              // B+F
        {GU_REVERSE_SUBTRACT, 0xFFFFFF, 0xFFFFFF}, // B-F
        {GU_ADD, 0x404040, 0xFFFFFF},              // B+F/4
    };
    sceGuBlendFunc(
        abr[mode].op, GU_FIX, GU_FIX, abr[mode].afix, abr[mode].bfix);
    sceGuEnable(GU_BLEND);
}

static inline void ApplyBlend(bool semitrans, int mode) {
    if (!semitrans) {
        mode = -1;
    }
    if (mode != cur_blend) {
        ApplyBlendReal(mode);
    }
}

static void ApplyDitherReal(int v) {
    CloseBatch();
    cur_dither = v;
    if (v) {
        sceGuEnable(GU_DITHER);
    } else {
        sceGuDisable(GU_DITHER);
    }
}

static inline void ApplyDither(bool want) {
    int v = want && dither_mode == PSYZ_DITHER_AUTO ? 1 : 0;
    if (v != cur_dither) {
        ApplyDitherReal(v);
    }
}

__attribute__((noinline)) static int ApplyTextureReal(
    u16 tpage, u16 clut, int variant) {
    // bpp==0: 4bpp, GU_PSM_T4, needs 16 color palette
    // bpp==1: 8bpp, GU_PSM_T8, needs 256 color palette
    // bpp==2: 16bpp, GU_PSM_5551, bitmap
    u8 page = tpage & 0x1F;
    u8 bpp = (tpage >> 7) & 3;
    if (bpp > 2) {
        bpp = 2;
    }
    const void* tex;
    int tex_psm;
    if (bpp == 0) {
        int tile = (page & 0xF) + (page >> 4) * VRAM_TILES_X;
        tex = EdramTile(tile);
        tex_psm = GU_PSM_T4;
    } else {
        PageEntry* e = FindOrCreatePage(page, bpp);
        if (!e) {
            return -1;
        }
        tex = PageBuf(e);
        tex_psm = bpp == 1 ? GU_PSM_T8 : GU_PSM_5551;
    }
    if (tex != bound_tex || tex_psm != bound_tex_psm) {
        CloseBatch();
        sceGuTexMode(tex_psm, 0, 0, 0);
        sceGuTexImage(0, 256, 256, 256, tex); // TODO implement texture window
        bound_tex = tex;
        bound_tex_psm = tex_psm;
    }
    if (bpp != 2) {
        ClutEntry* e = FindOrCreateClut(clut, bpp);
        if (!e) {
            return -1;
        }
        const u16* colors = g_clut[(int)(e - clut_cache)][variant];
        if (colors != bound_clut) {
            CloseBatch();
            sceGuClutMode(GU_PSM_5551, 0, 0xFF, 0);
            sceGuClutLoad(bpp ? 32 : 2, colors);
            bound_clut = colors;
        }
    }
    tex_memo_valid = true;
    tex_memo_tpage = tpage;
    tex_memo_clut = clut;
    tex_memo_variant = variant;
    tex_memo_bpp = bpp;
    return bpp;
}

static inline int ApplyTexture(u16 tpage, u16 clut, int variant) {
    if (tex_memo_valid && tpage == tex_memo_tpage && clut == tex_memo_clut &&
        variant == tex_memo_variant) {
        return tex_memo_bpp;
    }
    return ApplyTextureReal(tpage, clut, variant);
}

static inline unsigned short PackColor(u8 r, u8 g, u8 b) {
    return (unsigned short)(0x8000u | ((b >> 3) << 10) | ((g >> 3) << 5) |
                            (r >> 3));
}

static inline void ApplyTextured(bool on) {
    if ((int)on == cur_textured) {
        return;
    }
    CloseBatch();
    cur_textured = on;
    if (on) {
        sceGuEnable(GU_TEXTURE_2D);
        sceGuEnable(GU_ALPHA_TEST);
    } else {
        sceGuDisable(GU_TEXTURE_2D);
        sceGuDisable(GU_ALPHA_TEST);
    }
}

static const u8 tex_color_scale[256] = {
    0,   1,   2,   3,   3,   5,   6,   6,   7,   8,   9,   10,  11,  12,  13,
    14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,
    29,  30,  31,  31,  33,  34,  35,  35,  36,  37,  39,  39,  40,  42,  42,
    43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,
    58,  59,  60,  60,  63,  63,  63,  65,  65,  67,  68,  68,  70,  70,  71,
    72,  73,  75,  75,  76,  77,  78,  79,  80,  81,  81,  83,  84,  85,  86,
    87,  88,  89,  90,  91,  92,  93,  93,  94,  96,  97,  97,  98,  99,  100,
    102, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115,
    116, 117, 118, 119, 121, 121, 121, 121, 127, 127, 127, 127, 127, 129, 130,
    130, 132, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 143, 143, 144,
    145, 146, 147, 148, 149, 150, 151, 151, 152, 153, 156, 156, 157, 158, 159,
    160, 160, 161, 163, 164, 164, 166, 166, 167, 168, 169, 170, 171, 172, 172,
    174, 176, 176, 178, 178, 180, 180, 181, 182, 183, 183, 183, 188, 188, 188,
    188, 190, 191, 192, 192, 194, 194, 195, 196, 197, 199, 199, 200, 202, 203,
    203, 205, 205, 205, 208, 208, 210, 210, 212, 212, 214, 214, 215, 215, 218,
    218, 219, 219, 221, 222, 222, 225, 225, 226, 226, 228, 228, 230, 231, 233,
    233, 233, 235, 235, 236, 236, 238, 239, 245, 245, 245, 245, 245, 245, 245,
    245,
};

static inline u8 ScaleTexChannel(u8 c) { return tex_color_scale[c]; }

static inline unsigned short PackTexColor(u8 r, u8 g, u8 b) {
#if 0
    // useless?
    return PackColor(
        ScaleTexChannel(r), ScaleTexChannel(g), ScaleTexChannel(b));
#else
    return PackColor(r, g, b);
#endif
}

// Axis-aligned TILE/SPRT fast path
static void EmitPrim(const PVert* v, int n, u16 tpage, u16 clut, bool textured,
                     bool semitrans, bool is_rect);

// noinline yields to much better performance
__attribute__((noinline)) static void EmitRectSlow(
    int x0, int y0, int x1, int y1, int u0, int v0, int u1, int v1, u8 r, u8 g,
    u8 b, u16 tpage, u16 clut, bool textured, bool semitrans) {
    PVert pv[4];
    memset(pv, 0, sizeof(pv));
    pv[0].x = (short)x0;
    pv[0].y = (short)y0;
    pv[0].u = (u16)u0;
    pv[0].v = (u16)v0;
    pv[3].x = (short)x1;
    pv[3].y = (short)y1;
    pv[3].u = (u16)u1;
    pv[3].v = (u16)v1;
    pv[3].r = pv[0].r = r;
    pv[3].g = pv[0].g = g;
    pv[3].b = pv[0].b = b;
    pv[3].a = pv[0].a = semitrans ? 0x80 : 0xFF;
    EmitPrim(pv, 4, tpage, clut, textured, semitrans, true);
}

__attribute__((noinline)) static void EmitRectTextured(
    int x0, int y0, int x1, int y1, int u0, int v0, int u1, int v1, u8 r, u8 g,
    u8 b, u16 tpage, u16 clut, bool semitrans) {
    if (!display_enabled || !ListBudget()) {
        return;
    }
    FlushPendingClear();

    int variant = semitrans ? CLUT_OPAQUE_PASS : CLUT_PLAIN;
    int bpp = ApplyTexture(tpage, clut, variant);
    if (bpp < 0 || (semitrans && bpp != 2)) {
        EmitRectSlow(x0, y0, x1, y1, u0, v0, u1, v1, r, g, b, tpage, clut,
                     bpp >= 0, semitrans);
        return;
    }

    ApplyTextured(true);
    ApplyBlend(semitrans, (tpage >> 5) & 3);
    ApplyDither(env_dither && can_dither);

    unsigned short c = PackTexColor(r, g, b);
    int kx = draw_offset.x - fb_origin.x + map_ofs_x;
    int ky = draw_offset.y - fb_origin.y + map_ofs_y;

    BVert a, d;
    a.u = (short)u0;
    a.v = (short)v0;
    a.c = c;
    a.x = (short)(x0 + kx);
    a.y = (short)(y0 + ky);
    a.z = 0;
    d.u = (short)u1;
    d.v = (short)v1;
    d.c = c;
    d.x = (short)(x1 + kx);
    d.y = (short)(y1 + ky);
    d.z = 0;
    BatchPair(GU_SPRITES, &a, &d);
}

__attribute__((noinline)) static void EmitRectUntextured(
    int x0, int y0, int x1, int y1, u8 r, u8 g, u8 b, u16 tpage,
    bool semitrans) {
    if (!display_enabled || !ListBudget()) {
        return;
    }
    FlushPendingClear();

    ApplyTextured(false);
    ApplyBlend(semitrans, (tpage >> 5) & 3);
    ApplyDither(env_dither && can_dither);

    unsigned short c = PackColor(r, g, b);
    int kx = draw_offset.x - fb_origin.x + map_ofs_x;
    int ky = draw_offset.y - fb_origin.y + map_ofs_y;

    BVert a, d;
    a.u = 0;
    a.v = 0;
    a.c = c;
    a.x = (short)(x0 + kx);
    a.y = (short)(y0 + ky);
    a.z = 0;
    d.u = 0;
    d.v = 0;
    d.c = c;
    d.x = (short)(x1 + kx);
    d.y = (short)(y1 + ky);
    d.z = 0;
    BatchPair(GU_SPRITES, &a, &d);
}

static inline void EmitRect(
    int x0, int y0, int x1, int y1, int u0, int v0, int u1, int v1, u8 r, u8 g,
    u8 b, u16 tpage, u16 clut, bool textured, bool semitrans) {
    if (textured) {
        EmitRectTextured(
            x0, y0, x1, y1, u0, v0, u1, v1, r, g, b, tpage, clut, semitrans);
    } else {
        EmitRectUntextured(x0, y0, x1, y1, r, g, b, tpage, semitrans);
    }
}

typedef struct {
    int kx, ky;
} LineCtx;

// Returns false when the packet must be dropped (display off / list full).
static bool BeginLineBatch(bool semitrans, LineCtx* ctx) {
    if (!display_enabled || !ListBudget()) {
        return false;
    }
    FlushPendingClear();
    ApplyTextured(false);
    ApplyBlend(semitrans, (cur_tpage >> 5) & 3);
    ApplyDither(env_dither); // PS1 dithers lines, flat or gouraud
    ctx->kx = draw_offset.x - fb_origin.x + map_ofs_x;
    ctx->ky = draw_offset.y - fb_origin.y + map_ofs_y;
    return true;
}

// Per-segment emit with the invariant state already applied by BeginLineBatch.
static inline void EmitLineSeg(
    const LineCtx* ctx, int x0, int y0, unsigned int c0, int x1, int y1,
    unsigned int c1, bool extend_end) {
    int kx = ctx->kx;
    int ky = ctx->ky;
    int ex = x1, ey = y1;
    if (extend_end) {
        ex += (x1 > x0) - (x1 < x0);
        ey += (y1 > y0) - (y1 < y0);
    }
    BVert a, b;
    a.u = 0;
    a.v = 0;
    a.c = c0;
    a.x = (short)(x0 + kx);
    a.y = (short)(y0 + ky);
    a.z = 0;
    b.u = 0;
    b.v = 0;
    b.c = c1;
    b.x = (short)(ex + kx);
    b.y = (short)(ey + ky);
    b.z = 0;
    BatchPair(GU_LINES, &a, &b);
}

static void EmitPrim(const PVert* v, int n, u16 tpage, u16 clut, bool textured,
                     bool semitrans, bool is_rect) {
    // TODO ditch textured+semitrans, they're already packed in tpage
    if (!display_enabled || !ListBudget()) {
        return;
    }
    FlushPendingClear();
    BVert tv[4];
    int build[4], build_n;
    if (is_rect) {
        build[0] = 0;
        build[1] = 3;
        build_n = 2;
    } else {
        build[0] = 0;
        build[1] = 1;
        build[2] = 2;
        build[3] = 3;
        build_n = n;
    }
    int kx = draw_offset.x - fb_origin.x + map_ofs_x;
    int ky = draw_offset.y - fb_origin.y + map_ofs_y;
    for (int k = 0; k < build_n; k++) {
        int i = build[k];
        tv[i].u = textured ? (short)v[i].u : 0;
        tv[i].v = textured ? (short)v[i].v : 0;
        tv[i].c = textured ? PackTexColor(v[i].r, v[i].g, v[i].b)
                           : PackColor(v[i].r, v[i].g, v[i].b);
        tv[i].x = (short)(v[i].x + kx);
        tv[i].y = (short)(v[i].y + ky);
        tv[i].z = 0;
    }

    if (!textured) {
        ApplyTextured(false);
        ApplyBlend(semitrans, (tpage >> 5) & 3);
        ApplyDither(env_dither && can_dither);
        if (is_rect) {
            BatchPair(GU_SPRITES, &tv[0], &tv[3]);
        } else {
            BatchTri(tv, 0, 1, 2);
            if (n == 4) {
                BatchTri(tv, 2, 1, 3);
            }
        }
        return;
    }

    int variant = semitrans ? CLUT_OPAQUE_PASS : CLUT_PLAIN;
    int bpp = ApplyTexture(tpage, clut, variant);
    if (bpp < 0) {
        // no memory left: draw untextured instead of dropping
        EmitPrim(v, n, tpage, clut, false, semitrans, is_rect);
        return;
    }
    ApplyTextured(true);
    ApplyDither(env_dither && can_dither);

    int passes = semitrans && bpp != 2 ? 2 : 1;
    if (passes == 1) {
        ApplyBlend(semitrans, (tpage >> 5) & 3);
        if (is_rect) {
            BatchPair(GU_SPRITES, &tv[0], &tv[3]);
        } else {
            BatchTri(tv, 0, 1, 2);
            if (n == 4) {
                BatchTri(tv, 2, 1, 3);
            }
        }
        return;
    }

    for (int pass = 0; pass < passes; pass++) {
        ApplyTexture(tpage, clut, pass ? CLUT_BLEND_PASS : variant);
        ApplyBlend(pass != 0, (tpage >> 5) & 3);
        FlushBatch();
        BVert* out = GuGetMemoryDirect(n * sizeof(BVert));
        memcpy(out, tv, n * sizeof(BVert));
        GuDrawArrayDirect(n == 3 ? GU_TRIANGLES : GU_TRIANGLE_STRIP,
                          GU_TEXTURE_16BIT | GU_COLOR_5551 | GU_VERTEX_16BIT |
                              GU_TRANSFORM_2D,
                          n, out);
    }
}

// Image transfer that landed in the visible framebuffer
static void EmitVramRegionQuad(int x, int y, int w, int h) {
    if (!is_init || !display_enabled) {
        return;
    }
    FlushPendingClear();
    int x0 = x > fb_origin.x ? x : fb_origin.x;
    int y0 = y > fb_origin.y ? y : fb_origin.y;
    int x1 = x + w < fb_origin.x + game_w ? x + w : fb_origin.x + game_w;
    int y1 = y + h < fb_origin.y + game_h ? y + h : fb_origin.y + game_h;
    if (x0 >= x1 || y0 >= y1) {
        return;
    }
    FlushBatch();
    ApplyBlend(false, 0);
    ApplyDither(false);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuDisable(GU_ALPHA_TEST); // disabled, so all pixels will land clean
    cur_textured = -1; // force the next primitive to re-apply its state
    tex_memo_valid = false;
    ScissorFull();
    for (int py = y0; py < y1;) {
        // tile rows start at y 0/256 and are 256 tall
        int tile_y = py & ~0xFF;
        int span_y = (tile_y + 256 < y1 ? tile_y + 256 : y1) - py;
        for (int px = x0; px < x1;) {
            if (!ListBudget()) {
                UpdateScissor();
                return;
            }
            int tile_x = px & ~0x3F;
            int span_x = (tile_x + 64 < x1 ? tile_x + 64 : x1) - px;
            int tile = (tile_x >> 6) + (tile_y >> 8) * VRAM_TILES_X;
            if (EdramTile(tile) != bound_tex || bound_tex_psm != GU_PSM_5551) {
                sceGuTexMode(GU_PSM_5551, 0, 0, 0);
                sceGuTexImage(0, 64, 256, 64, EdramTile(tile));
                bound_tex = EdramTile(tile);
                bound_tex_psm = GU_PSM_5551;
            }
            BVert* out = GuGetMemoryDirect(2 * sizeof(BVert));
            out[0].u = (short)(px - tile_x);
            out[0].v = (short)(py - tile_y);
            out[0].c = COL5551_MOD1X;
            out[0].x = (short)(px - fb_origin.x + map_ofs_x);
            out[0].y = (short)(py - fb_origin.y + map_ofs_y);
            out[0].z = 0;
            out[1].u = (short)(px - tile_x + span_x);
            out[1].v = (short)(py - tile_y + span_y);
            out[1].c = COL5551_MOD1X;
            out[1].x = (short)(px + span_x - fb_origin.x + map_ofs_x);
            out[1].y = (short)(py + span_y - fb_origin.y + map_ofs_y);
            out[1].z = 0;
            GuDrawArrayDirect(GU_SPRITES,
                              GU_TEXTURE_16BIT | GU_COLOR_5551 |
                                  GU_VERTEX_16BIT | GU_TRANSFORM_2D,
                              2, out);
            px += span_x;
        }
        py += span_y;
    }
    UpdateScissor();
}

static void EmitVramFill(int x, int y, int w, int h, u8 r, u8 g, u8 b) {
    FlushBatch();
    GeCmd(GECMD_CLEARMODE,
          ((GU_COLOR_BUFFER_BIT | GU_STENCIL_BUFFER_BIT) << 8) | 1);
    unsigned short c = PackColor(r, g, b);
    int xe = x + w, ye = y + h;
    for (int ty = y >> 8; ty <= (ye - 1) >> 8; ty++) {
        for (int tx = x >> 6; tx <= (xe - 1) >> 6; tx++) {
            int x0 = tx << 6, y0 = ty << 8;
            int lx0 = x > x0 ? x - x0 : 0;
            int ly0 = y > y0 ? y - y0 : 0;
            int lx1 = xe < x0 + VRAM_TILE_W ? xe - x0 : VRAM_TILE_W;
            int ly1 = ye < y0 + VRAM_TILE_H ? ye - y0 : VRAM_TILE_H;
            int tile = tx + ty * VRAM_TILES_X;
            SetDrawTarget(
                EDRAM_TILES_OFFSET + tile * VRAM_TILE_BYTES, VRAM_TILE_W);
            sceGuScissor(0, 0, VRAM_TILE_W, VRAM_TILE_H);
            CVert* out = GuGetMemoryDirect(2 * sizeof(CVert));
            out[0].c = c;
            out[0].x = (short)lx0;
            out[0].y = (short)ly0;
            out[0].z = 0;
            out[1].c = c;
            out[1].x = (short)lx1;
            out[1].y = (short)ly1;
            out[1].z = 0;
            GuDrawArrayDirect(
                GU_SPRITES, GU_COLOR_5551 | GU_VERTEX_16BIT | GU_TRANSFORM_2D,
                2, out);
        }
    }
    GeCmd(GECMD_CLEARMODE, 0);
    if (use_surface) {
        SetDrawTarget(EdramSurfaceOffset(surface_idx), surface_stride);
    } else {
        SetDrawTarget(FbOffset(cur_draw_fb), FB_STRIDE);
    }
    UpdateScissor();
    KickGe();
}

static void EmitClearQuad(int x, int y, int w, int h, u8 r, u8 g, u8 b) {
    if (!display_enabled || !ListBudget()) {
        return;
    }
    int x0 = x > fb_origin.x ? x : fb_origin.x;
    int y0 = y > fb_origin.y ? y : fb_origin.y;
    int x1 = x + w < fb_origin.x + game_w ? x + w : fb_origin.x + game_w;
    int y1 = y + h < fb_origin.y + game_h ? y + h : fb_origin.y + game_h;
    if (x0 >= x1 || y0 >= y1) {
        return;
    }
    FlushBatch();
    GeCmd(GECMD_CLEARMODE, (GU_COLOR_BUFFER_BIT << 8) | 1);
    CVert* out = GuGetMemoryDirect(2 * sizeof(CVert));
    out[0].c = PackColor(r, g, b);
    out[0].x = (float)(x0 - fb_origin.x + map_ofs_x);
    out[0].y = (float)(y0 - fb_origin.y + map_ofs_y);
    out[0].z = 0.0f;
    out[1].c = PackColor(r, g, b);
    out[1].x = (float)(x1 - fb_origin.x + map_ofs_x);
    out[1].y = (float)(y1 - fb_origin.y + map_ofs_y);
    out[1].z = 0.0f;
    ScissorFull();
    GuDrawArrayDirect(
        GU_SPRITES, GU_COLOR_5551 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, out);
    GeCmd(GECMD_CLEARMODE, 0);
    UpdateScissor();
}

// ===== PS1 GPU packet parsing, identical to sdl3_gpu.c =====

#define SEMITRANSP 0x02
#define TEXTURED 0x04
#define EXTRA_VERTEX 0x08
#define GOURAUD 0x10
#define TRIANGLE 0x20

// real hardware uses XY coords as signed 11-bit
static short s11(short v) { return (short)(((v & 0x7FF) ^ 1024) - 1024); }

static int writePacket(PVert* v, int code, int n, u_long* packet, u16* pOut) {
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

static void FixupFlipUV(PVert* v, int hasFourVertices) {
    bool fix_u = (v[0].x > v[1].x) ^ (v[0].u > v[1].u);
    fix_u |= (v[0].x > v[2].x) ^ (v[0].u > v[2].u);
    if (fix_u) {
        v[0].u++;
        v[1].u++;
        v[2].u++;
        if (hasFourVertices) {
            v[3].u++;
        }
    }

    bool fix_v = (v[0].y > v[1].y) ^ (v[0].v > v[1].v);
    fix_v |= (v[0].y > v[2].y) ^ (v[0].v > v[2].v);
    if (fix_v) {
        v[0].v++;
        v[1].v++;
        v[2].v++;
        if (hasFourVertices) {
            v[3].v++;
        }
    }
}

static inline int PushTileWithFixedSize(
    u_long* packets, int max_len, int code, int size, bool textured) {
    int len = max_len;
    bool isSemiTrans = (code & SEMITRANSP) != 0;
    bool isShadeTex = !((code & 1) && textured);
    u8 r, g, b;

    if (isShadeTex) {
        r = (u8)(*packets >> 0);
        g = (u8)(*packets >> 8);
        b = (u8)(*packets >> 16);
    } else {
        r = g = b = 0x80;
    }
    packets++;
    len--;

    int x = s11(((short*)packets)[0]);
    int y = s11(((short*)packets)[1]);
    packets++;
    len--;

    int tu = 0, tv = 0;
    u16 clut = (u16)-1;
    if (textured) {
        tu = ((u8*)packets)[0];
        tv = ((u8*)packets)[1];
        clut = (u16)((s16*)packets)[1];
        packets++;
        len--;
    }

    EmitRect(x, y, x + size, y + size, tu, tv, tu + size, tv + size, r, g, b,
             cur_tpage, clut, textured, isSemiTrans);
    return max_len - len;
}

// creates six variants of PushTileWithFixedSize without using ugly macros
__attribute__((noinline)) static int PushTile1(u_long* p, int len, int code) {
    return PushTileWithFixedSize(p, len, code, 1, false);
}
__attribute__((noinline)) static int PushSprt1(u_long* p, int len, int code) {
    return PushTileWithFixedSize(p, len, code, 1, true);
}
__attribute__((noinline)) static int PushTile8(u_long* p, int len, int code) {
    return PushTileWithFixedSize(p, len, code, 8, false);
}
__attribute__((noinline)) static int PushSprt8(u_long* p, int len, int code) {
    return PushTileWithFixedSize(p, len, code, 8, true);
}
__attribute__((noinline)) static int PushTile16(u_long* p, int len, int code) {
    return PushTileWithFixedSize(p, len, code, 16, false);
}
__attribute__((noinline)) static int PushSprt16(u_long* p, int len, int code) {
    return PushTileWithFixedSize(p, len, code, 16, true);
}

__attribute__((noinline)) static int PushTile(
    u_long* packets, int max_len, int code) {
    int len = max_len;
    bool isTextured = (code & TEXTURED) != 0;
    bool isSemiTrans = (code & SEMITRANSP) != 0;
    bool isShadeTex = !((code & 1) && isTextured);
    u8 r, g, b;

    if (isShadeTex) {
        r = (u8)(*packets >> 0);
        g = (u8)(*packets >> 8);
        b = (u8)(*packets >> 16);
    } else {
        r = g = b = 0x80;
    }
    packets++;
    len--;

    int x = s11(((short*)packets)[0]);
    int y = s11(((short*)packets)[1]);
    packets++;
    len--;

    int tu = 0, tv = 0;
    u16 clut = (u16)-1;
    if (isTextured) {
        tu = ((u8*)packets)[0];
        tv = ((u8*)packets)[1];
        clut = (u16)((s16*)packets)[1];
        packets++;
        len--;
    }

    int w = ((s16*)packets)[0];
    int h = ((s16*)packets)[1];
    packets++;
    len--;

    EmitRect(x, y, x + w, y + h, tu, tv, tu + w, tv + h, r, g, b, cur_tpage,
             clut, isTextured, isSemiTrans);
    return max_len - len;
}

// LINE_* emits one native GE line per segment
__attribute__((noinline)) static int PushLine(
    u_long* packets, int max_len, int code) {
    int len = max_len;
    bool isGouraud = (code & GOURAUD) != 0;
    bool isSemiTrans = (code & SEMITRANSP) != 0;
    bool padding = true;
    int nPoints = ((code >> 2) & 3) + 1;
    if (nPoints == 1) {
        padding = false;
        nPoints++; // don't ask, have faith
    }

    unsigned short c1 = PackColor(
        (u8)(*packets >> 0), (u8)(*packets >> 8), (u8)(*packets >> 16));
    packets++;
    len--;

    LineCtx ctx;
    bool ok = BeginLineBatch(isSemiTrans, &ctx);
    if (len > 0) {
        short px0 = s11(((short*)packets)[0]);
        short py0 = s11(((short*)packets)[1]);
        packets++;
        len--;
        for (int i = 1; i < nPoints && len > 0; i++) {
            unsigned short c0 = c1;
            if (isGouraud) {
                c1 = PackColor(
                    ((u8*)packets)[0], ((u8*)packets)[1], ((u8*)packets)[2]);
                packets++;
                len--;
                if (len <= 0) {
                    break;
                }
            }
            short px1 = s11(((short*)packets)[0]);
            short py1 = s11(((short*)packets)[1]);
            packets++;
            len--;
            if (ok) {
                EmitLineSeg(&ctx, px0, py0, c0, px1, py1, c1,
                            i + 1 < nPoints || !padding);
            }
            px0 = px1;
            py0 = py1;
        }
    }
    if (padding) {
        len--;
    }
    return max_len - len;
}

__attribute__((noinline)) static int PushPolyGeneric(
    u_long* packets, int max_len, int code) {
    int len = max_len;
    bool isTextured = (code & TEXTURED) != 0;
    bool isGouraud = (code & GOURAUD) != 0;
    bool isShadeTex = !((code & 1) && isTextured);
    bool isSemiTrans = (code & SEMITRANSP) != 0;
    u16 tpage = -1, clut = -1, pad2, pad3;
    PVert verts[5] = {0};
    PVert* v = verts;

    if (isShadeTex) {
        v->r = (unsigned char)(*packets >> 0);
        v->g = (unsigned char)(*packets >> 8);
        v->b = (unsigned char)(*packets >> 16);
    } else {
        v->r = v->g = v->b = 0x80;
    }
    v->a = isSemiTrans ? 0x80 : 0xFF;
    packets++;
    len--;

    int wr, nVertices;
    wr = writePacket(v++, code, len, packets, &clut);
    packets += wr;
    len -= wr;
    wr = writePacket(v++, code, len, packets, &tpage);
    packets += wr;
    len -= wr;
    wr = writePacket(v++, code, len, packets, &pad2);
    packets += wr;
    len -= wr;

    if (code & EXTRA_VERTEX) {
        nVertices = 4;
        wr = writePacket(v, code, len, packets, &pad3);
        packets += wr;
        len -= wr;
    } else {
        nVertices = 3;
    }
    // HACK last rgb are not read by writePacket, so we patch the amount
    if (isGouraud) {
        packets--;
        len++;
    }

    if (isTextured) {
        FixupFlipUV(verts, code & EXTRA_VERTEX);
    } else {
        clut = -1;
        tpage = cur_tpage;
    }
    if (!isGouraud || !isShadeTex) {
        for (int i = 1; i < nVertices; i++) {
            verts[i].r = verts[0].r;
            verts[i].g = verts[0].g;
            verts[i].b = verts[0].b;
            verts[i].a = verts[0].a;
        }
    }
    can_dither = isGouraud || (isTextured && isShadeTex);
    EmitPrim(verts, nVertices, isTextured ? tpage : cur_tpage, clut, isTextured,
             isSemiTrans, false);
    can_dither = false;
    return max_len - len;
}

typedef struct {
    short x[4], y[4];
    u16 u[4], v[4];
    unsigned int c[4];
} PolyVerts;

// parse one vertex of a well-formed POLY_* packet; returns the next word
static inline u_long* ReadPolyVert(
    PolyVerts* p, int i, u_long* w, bool textured, bool gouraud) {
    if (gouraud && i > 0) {
        p->c[i] = (unsigned int)*w++;
    }
    unsigned int xy = (unsigned int)*w++;
    p->x[i] = s11((short)xy);
    p->y[i] = s11((short)(xy >> 16));
    if (textured) {
        unsigned int uv = (unsigned int)*w++;
        p->u[i] = (u8)uv;
        p->v[i] = (u8)(uv >> 8);
    }
    return w;
}

static inline void FixupPolyFlipUV(PolyVerts* p, bool quad) {
    bool fix_u = (p->x[0] > p->x[1]) ^ (p->u[0] > p->u[1]);
    fix_u |= (p->x[0] > p->x[2]) ^ (p->u[0] > p->u[2]);
    if (fix_u) {
        p->u[0]++;
        p->u[1]++;
        p->u[2]++;
        if (quad) {
            p->u[3]++;
        }
    }
    bool fix_v = (p->y[0] > p->y[1]) ^ (p->v[0] > p->v[1]);
    fix_v |= (p->y[0] > p->y[2]) ^ (p->v[0] > p->v[2]);
    if (fix_v) {
        p->v[0]++;
        p->v[1]++;
        p->v[2]++;
        if (quad) {
            p->v[3]++;
        }
    }
}

static inline void WritePolyVert(
    BVert* out, const PolyVerts* p, int i, bool textured, int kx, int ky) {
    out->u = textured ? (short)p->u[i] : 0;
    out->v = textured ? (short)p->v[i] : 0;
    out->c = p->c[i];
    out->x = (short)(p->x[i] + kx);
    out->y = (short)(p->y[i] + ky);
    out->z = 0;
}

static inline int PushPolyFast(
    u_long* packets, int max_len, int code, bool textured, bool gouraud) {
    bool quad = (code & EXTRA_VERTEX) != 0;
    int need = (quad ? 4 : 3) * (1 + textured + gouraud) + 1 - gouraud;
    if (max_len < need) {
        return PushPolyGeneric(packets, max_len, code);
    }
    if (!display_enabled || !ListBudget()) {
        return need;
    }
    FlushPendingClear();

    bool semitrans = (code & SEMITRANSP) != 0;
    bool shadetex = !(textured && (code & 1));

    PolyVerts pv;
    pv.c[0] = (unsigned int)packets[0];
    u_long* w = packets + 1;
    w = ReadPolyVert(&pv, 0, w, textured, gouraud);
    w = ReadPolyVert(&pv, 1, w, textured, gouraud);
    w = ReadPolyVert(&pv, 2, w, textured, gouraud);
    if (quad) {
        ReadPolyVert(&pv, 3, w, textured, gouraud);
    }

    u16 clut = (u16)-1;
    u16 tpage = cur_tpage;
    if (textured) {
        clut = (u16)(packets[2] >> 16);
        tpage = (u16)(packets[gouraud ? 5 : 4] >> 16);
    }

    if (!shadetex) {
        pv.c[0] = COL5551_MOD1X; // raw texture: pass-through modulation
    } else {
        int ncol = gouraud ? (quad ? 4 : 3) : 1;
        for (int i = 0; i < ncol; i++) {
            unsigned int c = pv.c[i];
            pv.c[i] = textured
                          ? PackTexColor((u8)c, (u8)(c >> 8), (u8)(c >> 16))
                          : PackColor((u8)c, (u8)(c >> 8), (u8)(c >> 16));
        }
    }
    if (!gouraud || !shadetex) {
        pv.c[1] = pv.c[0];
        pv.c[2] = pv.c[0];
        if (quad) {
            pv.c[3] = pv.c[0];
        }
    }

    if (textured) {
        FixupPolyFlipUV(&pv, quad);
        int bpp = ApplyTexture(
            tpage, clut, semitrans ? CLUT_OPAQUE_PASS : CLUT_PLAIN);
        if (bpp < 0 || (semitrans && bpp != 2)) {
            return PushPolyGeneric(packets, max_len, code);
        }
    }
    ApplyTextured(textured);
    ApplyBlend(semitrans, (tpage >> 5) & 3);
    ApplyDither(env_dither && (gouraud || (textured && shadetex)));

    BVert* out = BatchAlloc(GU_TRIANGLES, quad ? 6 : 3);
    if (!out) {
        return need;
    }
    int kx = draw_offset.x - fb_origin.x + map_ofs_x;
    int ky = draw_offset.y - fb_origin.y + map_ofs_y;
    WritePolyVert(&out[0], &pv, 0, textured, kx, ky);
    WritePolyVert(&out[1], &pv, 1, textured, kx, ky);
    WritePolyVert(&out[2], &pv, 2, textured, kx, ky);
    if (quad) {
        out[3] = out[2];
        out[4] = out[1];
        WritePolyVert(&out[5], &pv, 3, textured, kx, ky);
    }
    return need;
}

// don't inline these four functions, but hint to inline PushPolyFast in them
__attribute__((noinline)) static int PushPolyF(u_long* p, int len, int code) {
    return PushPolyFast(p, len, code, false, false);
}
__attribute__((noinline)) static int PushPolyFT(u_long* p, int len, int code) {
    return PushPolyFast(p, len, code, true, false);
}
__attribute__((noinline)) static int PushPolyG(u_long* p, int len, int code) {
    return PushPolyFast(p, len, code, false, true);
}
__attribute__((noinline)) static int PushPolyGT(u_long* p, int len, int code) {
    return PushPolyFast(p, len, code, true, true);
}

int Draw_PushPrim(u_long* packets, int max_len) {
    int code = (int)(*packets >> 24) & 0xFF;
    if (!is_init && !InitPlatform()) {
        return max_len;
    }
    if (code & 0x40) {
        if (code & 0x20) {
            // fixed-size tiles/sprites carry no w/h word: a specialized parser
            // per shape avoids PushTile's union-of-all-shapes frame
            switch (code & ~3) {
            case 0x68:
                return PushTile1(packets, max_len, code);
            case 0x6C:
                return PushSprt1(packets, max_len, code);
            case 0x70:
                return PushTile8(packets, max_len, code);
            case 0x74:
                return PushSprt8(packets, max_len, code);
            case 0x78:
                return PushTile16(packets, max_len, code);
            case 0x7C:
                return PushSprt16(packets, max_len, code);
            default:
                return PushTile(packets, max_len, code);
            }
        }
        return PushLine(packets, max_len, code);
    }
    if (!(code & TRIANGLE)) {
        // shouldn't happen on a normal PSX application
        WARNF("code %02X not supported", code);
        return 1;
    }
    switch (code & (TEXTURED | GOURAUD)) {
    case 0:
        return PushPolyF(packets, max_len, code);
    case TEXTURED:
        return PushPolyFT(packets, max_len, code);
    case GOURAUD:
        return PushPolyG(packets, max_len, code);
    default:
        return PushPolyGT(packets, max_len, code);
    }
}

void Draw_ResetBuffer(void) {
    if (!is_init && !InitPlatform()) {
        return;
    }
}

void Draw_FlushBuffer(void) {
    // geometry commands are emitted to the engine immediately
}

int Draw_ExequeSync(void) {
    // wait for StoreImage/LoadImage DMA to finish
    if (store_readback_pending) {
        store_readback_pending = false;
        GeSyncCaughtUp();
    }
    return 0;
}
