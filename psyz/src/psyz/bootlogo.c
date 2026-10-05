#include <libetc.h>
#include <libgpu.h>
#include <libgte.h>
#include <libspu.h>
#include <psyz/bootlogo.h>
#ifdef __psyz
#include <psyz/audio.h>
#include <psyz/gte.h>
#include <psyz/system.h>
#endif

#ifndef PSYZ_TITLE
#define PSYZ_TITLE "Hello PsyZ!"
#endif

// Fixed point: ONE is 1.0, angles use 4096 per turn, time is in milliseconds.
// World space is y-down with WU GTE units per world unit, camera at origin.
#define ONE 4096
#define WU 64
#define RAD(milli) ((milli) * 652 / 1000)
#define MS_RATE(ms, rate_milli) ((ms) * (rate_milli) / 1000 * ONE / 1000)

#define LAYOUT_W 320
#define LAYOUT_H 240
#define FOCAL 280

#define T_SHATTER 1200
#define T_LOCK 2220
#define T_FADE 380
#define T_LENGTH 4000 // the scene has settled
#define T_MAX_DEFAULT 4500
#define T_SKIP_FADE 250
#define WAVE_MS 640

#define OT_LEN 256
#define OT_SHIFT 2 // otz spans 4 times the table
#define OT_BG (OT_LEN - 1)
#define OT_STARS (OT_LEN - 2)
#define OT_HALO (OT_LEN - 3)
#define OT_MODELS (OT_LEN - 4)
#define OT_FAR (OT_LEN - 5)
#define OT_NEAR 16
#define OT_EFFECTS 14
#define OT_GLINT 12
#define OT_SPARKLE 11
#define OT_FLARE 10
#define OT_SHOCK 9
#define OT_TEXT 8
#define OT_CRT 3

#define FAN_SEG 16
#define STAR_COUNT 150
#define STAR_SPAN (90 * WU)
#define SPHERE_VERTS 42
#define SPHERE_FACES 80
#define SHARD_MAX 128
#define FRONT_MAX 32
#define GLINT_MAX 128
#define RING_COUNT 34
#define SHOCK_SEG 32
#define SPARK_SLOTS 2
#define TITLE_MAX 40

#define FONT_X 960
#define FONT_Y 0
#define CLUT_X (FONT_X + 48)
#define CLUT_Y (FONT_Y + 13)

enum { MODE_BG, MODE_MODELS, MODE_EFFECTS, MODE_TEXT, MODE_COUNT };

#define GLOW_RINGS 5

typedef struct {
    POLY_G3 core[FAN_SEG];
    POLY_G4 ring[GLOW_RINGS - 1][FAN_SEG];
} GlowFan;

// Drawn only once the letters settle, so they share memory with the shatter
// data and get set up again on entering STEP_IMPACT.
typedef struct {
    POLY_G3 glint[GLINT_MAX];
    POLY_G3 sparkle[SPARK_SLOTS][8];
    TILE bar[4];
    POLY_FT4 glyph[TITLE_MAX][2];
} Finale;

typedef struct {
    DRAWENV draw;
    DISPENV disp;
    OT_TYPE ot[OT_LEN];
    DR_MODE mode[MODE_COUNT];
    GlowFan glow;
    POLY_G3 halo[FAN_SEG];
    LINE_G2 star[STAR_COUNT];
    POLY_G3 model[SHARD_MAX];
    POLY_G3 ring[RING_COUNT];
    POLY_G4 flare[4];
    POLY_G4 shock[SHOCK_SEG][2];
    TILE crt[5];
    Finale* fin;
} DB;

typedef struct {
    void* head;
    void* tail;
} Chain;

typedef struct {
    MATRIX rot;
    int scale;
    int cx, cy, cz;
    int charge;
} Xf;

// The GTE loads SVECTORs with lwc2: the int keeps sizeof a multiple of 4, so
// loc stays word aligned in every array element.
typedef struct {
    SVECTOR loc[3];
    SVECTOR nrm;
    u_char col[3][3];
    u_char face, letter;
    int t0;
} Shard;

// Burst direction and swirl off the sphere, then the tumble back to the letters
typedef struct {
    signed char dir[3], axis[3];
    u_char src;
    short mag, spin, swirl;
} ShardMotion;

typedef struct {
    short x, y, z;
    short b;
} Star;

typedef struct {
    SVECTOR pos[SHARD_MAX][3];
    SVECTOR nrm[SHARD_MAX];
    short e[SHARD_MAX];
    short otz[SHARD_MAX];
    u_char shard[SHARD_MAX];
    u_char facing[SHARD_MAX];
    SVECTOR back; // from a letter's front face to its back face, in view space
    int n;
} Fly;

static const u_char brand[4][3] = {
    {255, 60, 125}, {255, 176, 32}, {46, 230, 200}, {106, 123, 255}};
static const u_char letter_pal[4][3][3] = {
    {{255, 150, 192}, {214, 26, 104}, {110, 12, 64}},
    {{255, 230, 128}, {236, 124, 14}, {124, 56, 6}},
    {{150, 255, 236}, {14, 164, 168}, {6, 78, 92}},
    {{172, 184, 255}, {80, 68, 232}, {36, 30, 124}}};
static const short light_dir[3] = {-1848, -2874, -2258};
static const short exp_tbl[33] = {
    4096, 3190, 2484, 1935, 1507, 1174, 914, 712, 554, 432, 336,
    262,  204,  159,  124,  96,   75,   58,  46,  35,  28,  21,
    17,   13,   10,   8,    6,    5,    4,   3,   2,   2,   1};

// letter, then four corners in tenths of a letter unit, one convex piece each
static const signed char logo_pieces[15][9] = {
    {0, 0, 0, 12, 0, 12, 48, 0, 48},     {0, 0, 48, 12, 60, 38, 60, 50, 48},
    {0, 38, 36, 50, 36, 50, 48, 38, 48}, {0, 12, 24, 38, 24, 50, 36, 12, 36},
    {1, 0, 48, 12, 60, 50, 60, 50, 48},  {1, 0, 36, 12, 36, 12, 48, 0, 48},
    {1, 0, 36, 38, 36, 50, 24, 12, 24},  {1, 38, 12, 50, 12, 50, 24, 38, 24},
    {1, 0, 0, 38, 0, 50, 12, 0, 12},     {2, 19, 0, 31, 0, 31, 30, 19, 30},
    {2, 19, 30, 31, 30, 12, 60, 0, 60},  {2, 19, 30, 31, 30, 50, 60, 38, 60},
    {3, 0, 48, 50, 48, 50, 60, 0, 60},   {3, 0, 12, 17, 12, 50, 48, 33, 48},
    {3, 0, 0, 50, 0, 50, 12, 0, 12}};
#define LOGO_ADVANCE 62
#define LOGO_HALF_W (118 * 8)
#define LOGO_HALF_H (30 * 8)
#define LOGO_HALF_D (12 * 8)

// sparkle anchors on letter corners (tenths) and when they flash
static const short spark_at[5][3] = {
    {12, 60, 2500},
    {50 + LOGO_ADVANCE, 60, 2780},
    {50 + LOGO_ADVANCE * 3, 0, 3000},
    {50 + LOGO_ADVANCE * 2, 60, 3220},
    {LOGO_ADVANCE * 2, 60, 3400}};

static const char font_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,:;-!?'&/()+_#*=";
static const u_char font_width[] = {
    5, 5, 5, 5, 5, 5, 5, 5, 3, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 3, 5, 5, 5, 5, 5, 5, 5, 5,
    1, 2, 1, 2, 3, 1, 5, 1, 5, 5, 2, 2, 5, 5, 5, 5, 5};
// The 5x7 glyphs as a 4bpp texture: glyph i sits in cell i & 31 of texel rows
// (i >> 5) * 7 to +6, lowest nibble leftmost. Cells 24-31 of the last row hold
// the CLUT, so one LoadImage uploads both.
static const unsigned int font_tex[14][32] = {
    {0x01110, 0x01111, 0x01110, 0x01111, 0x11111, 0x11111, 0x01110, 0x10001,
     0x00111, 0x11100, 0x10001, 0x00001, 0x10001, 0x10001, 0x01110, 0x01111,
     0x01110, 0x01111, 0x11110, 0x11111, 0x10001, 0x10001, 0x10001, 0x10001,
     0x10001, 0x11111, 0x01110, 0x00010, 0x01110, 0x01111, 0x01000, 0x11111},
    {0x10001, 0x10001, 0x10001, 0x10001, 0x00001, 0x00001, 0x10001, 0x10001,
     0x00010, 0x01000, 0x01001, 0x00001, 0x11011, 0x10001, 0x10001, 0x10001,
     0x10001, 0x10001, 0x00001, 0x00100, 0x10001, 0x10001, 0x10001, 0x10001,
     0x10001, 0x10000, 0x10001, 0x00011, 0x10001, 0x10000, 0x01100, 0x00001},
    {0x10001, 0x10001, 0x00001, 0x10001, 0x00001, 0x00001, 0x00001, 0x10001,
     0x00010, 0x01000, 0x00101, 0x00001, 0x10101, 0x10011, 0x10001, 0x10001,
     0x10001, 0x10001, 0x00001, 0x00100, 0x10001, 0x10001, 0x10001, 0x01010,
     0x01010, 0x01000, 0x11001, 0x00010, 0x10000, 0x10000, 0x01010, 0x01111},
    {0x11111, 0x01111, 0x00001, 0x10001, 0x01111, 0x01111, 0x11101, 0x11111,
     0x00010, 0x01000, 0x00011, 0x00001, 0x10101, 0x10101, 0x10001, 0x01111,
     0x10001, 0x01111, 0x01110, 0x00100, 0x10001, 0x10001, 0x10101, 0x00100,
     0x00100, 0x00100, 0x10101, 0x00010, 0x01000, 0x01110, 0x01001, 0x10000},
    {0x10001, 0x10001, 0x00001, 0x10001, 0x00001, 0x00001, 0x10001, 0x10001,
     0x00010, 0x01000, 0x00101, 0x00001, 0x10001, 0x11001, 0x10001, 0x00001,
     0x10101, 0x00101, 0x10000, 0x00100, 0x10001, 0x10001, 0x10101, 0x01010,
     0x00100, 0x00010, 0x10011, 0x00010, 0x00100, 0x10000, 0x11111, 0x10000},
    {0x10001, 0x10001, 0x10001, 0x10001, 0x00001, 0x00001, 0x10001, 0x10001,
     0x00010, 0x01001, 0x01001, 0x00001, 0x10001, 0x10001, 0x10001, 0x00001,
     0x01001, 0x01001, 0x10000, 0x00100, 0x10001, 0x01010, 0x10101, 0x10001,
     0x00100, 0x00001, 0x10001, 0x00010, 0x00010, 0x10000, 0x01000, 0x10001},
    {0x10001, 0x01111, 0x01110, 0x01111, 0x11111, 0x00001, 0x11110, 0x10001,
     0x00111, 0x00110, 0x10001, 0x11111, 0x10001, 0x10001, 0x01110, 0x00001,
     0x10110, 0x10001, 0x01111, 0x00100, 0x01110, 0x00100, 0x01010, 0x10001,
     0x00100, 0x11111, 0x01110, 0x00111, 0x11111, 0x01111, 0x01000, 0x01110},
    {0x01100, 0x11111, 0x01110, 0x01110, 0x00000, 0x00000, 0x00000, 0x00000,
     0x00000, 0x00001, 0x01110, 0x00001, 0x00110, 0x10000, 0x00010, 0x00001,
     0x00000, 0x00000, 0x01010, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000,
     0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000},
    {0x00010, 0x10000, 0x10001, 0x10001, 0x00000, 0x00000, 0x00000, 0x00000,
     0x00000, 0x00001, 0x10001, 0x00001, 0x01001, 0x10000, 0x00001, 0x00010,
     0x00100, 0x00000, 0x01010, 0x10101, 0x00000, 0x00000, 0x00000, 0x00000,
     0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000},
    {0x00001, 0x01000, 0x10001, 0x10001, 0x00000, 0x00000, 0x00001, 0x00010,
     0x00000, 0x00001, 0x10000, 0x00000, 0x00101, 0x01000, 0x00001, 0x00010,
     0x00100, 0x00000, 0x11111, 0x01110, 0x11111, 0x00000, 0x00000, 0x00000,
     0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000},
    {0x01111, 0x00100, 0x01110, 0x11110, 0x00000, 0x00000, 0x00000, 0x00000,
     0x00111, 0x00001, 0x01000, 0x00000, 0x00010, 0x00100, 0x00001, 0x00010,
     0x11111, 0x00000, 0x01010, 0x11111, 0x00000, 0x00000, 0x00000, 0x00000,
     0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000},
    {0x10001, 0x00010, 0x10001, 0x10000, 0x00000, 0x00000, 0x00000, 0x00000,
     0x00000, 0x00001, 0x00100, 0x00000, 0x10101, 0x00010, 0x00001, 0x00010,
     0x00100, 0x00000, 0x11111, 0x01110, 0x11111, 0x00000, 0x00000, 0x00000,
     0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000},
    {0x10001, 0x00010, 0x10001, 0x01000, 0x00000, 0x00010, 0x00001, 0x00010,
     0x00000, 0x00000, 0x00000, 0x00000, 0x01001, 0x00001, 0x00001, 0x00010,
     0x00100, 0x00000, 0x01010, 0x10101, 0x00000, 0x00000, 0x00000, 0x00000,
     0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000},
    {0x01110,    0x00010,    0x01110,    0x00110,    0x00001,    0x00001,
     0x00000,    0x00001,    0x00000,    0x00001,    0x00100,    0x00000,
     0x10110,    0x00001,    0x00010,    0x00001,    0x00000,    0x11111,
     0x01010,    0x00000,    0x00000,    0x00000,    0x00000,    0x00000,
     0x7FFF0000, 0x7FFF7FFF, 0x7FFF7FFF, 0x7FFF7FFF, 0x7FFF7FFF, 0x7FFF7FFF,
     0x7FFF7FFF, 0x7FFF7FFF},
};

// Easter egg offsets; all zero when the pad is left alone.
typedef struct {
    int ring_yaw, ring_pitch;
    int star_yaw, star_pitch;
    int logo_yaw, logo_pitch;
    int warp;
    // velocities, in 1/16 of a unit per 60 Hz frame
    int ring_yaw_v, ring_pitch_v, yaw_v, pitch_v, warp_v;
} EggState;

static struct {
    int w, h;
    int sx, sy;
    int cam_x;
    int dfe;
    int zbias;
    int logo_y;
    int shatter_y;
    int settle;
    int shards;
    int glint_n;
    short glint_face[FRONT_MAX][6];
    MATRIX cam;
    MATRIX ring;
    int sc[3];
    int title_n;
    int title_w;
    int title_x;
    int text_k;
    int type_step;
    int wave_at;
    int wave_rate;
    int inv_sx, inv_sy;
    int fade_mul;
    int clock; // unclamped time, for the motions that never settle
    int flash;
    int bg_inv_h;
    int bg_base[3];
    int bg_slope[3];
    int step;
    EggState egg;
} g;

static unsigned rng;

typedef struct {
    short x;
    u_char u, v, w, accent;
} TitleChar;

// Lives on Psyz_Bootlogo's stack
typedef struct {
    SVECTOR sphere_pos[SPHERE_VERTS];
    SVECTOR sphere_nrm[SPHERE_FACES];
    u_char sphere_face[SPHERE_FACES][3];
    u_char sphere_col[SPHERE_VERTS][3];
    int sphere_sxy[SPHERE_VERTS];
    int sphere_otz[SPHERE_VERTS];
    Shard shards[SHARD_MAX];
    union {
        struct {
            SVECTOR shatter_pos[SPHERE_FACES][3];
            u_long shard_rgb[SHARD_MAX][2][3];
            ShardMotion motion[SHARD_MAX];
            Fly fly;
        };
        Finale fin[2];
    } stage;
    Star stars[STAR_COUNT];
    short star_recip[32];
    TitleChar title[TITLE_MAX];
    // Quarter-wave copy of rsin: calling into libgte from the hot loops lets
    // its code evict theirs from the direct-mapped instruction cache.
    short sin_q[1025];
    // last, so every other field stays in reach of a 16-bit load offset
    DB dbuf[2];
} Scene;

static Scene* sc;

static int Sin(int a) {
    a &= ONE - 1;
    if (a < 1024)
        return sc->sin_q[a];
    if (a < 2048)
        return sc->sin_q[2048 - a];
    if (a < 3072)
        return -sc->sin_q[a - 2048];
    return -sc->sin_q[4096 - a];
}

static int Cos(int a) { return Sin(a + 1024); }

static int Sat(int x) { return x < 0 ? 0 : x > ONE ? ONE : x; }
static int Ramp(int time, int t0, int len) {
    return Sat((time - t0) * ONE / len);
}
static int Lerp(int a, int b, int f) { return a + ((b - a) * f >> 12); }
static int Clamp8(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }
static int Abs(int v) { return v < 0 ? -v : v; }
static int Max(int a, int b) { return a > b ? a : b; }
static int Min(int a, int b) { return a < b ? a : b; }

// Saturated inputs return early: pcsx-redux's dynarec miscomputes the
// constant-operand mult that GCC emits for the clamped path.
static int Smooth(int x) {
    if (x <= 0)
        return 0;
    if (x >= ONE)
        return ONE;
    return (x * x >> 12) * (3 * ONE - 2 * x) >> 12;
}

static int EaseOut(int x) {
    if (x <= 0)
        return 0;
    if (x >= ONE)
        return ONE;
    x = ONE - x;
    return ONE - ((x * x >> 12) * x >> 12);
}

static int ExpNeg(int x) {
    int i;
    if (x <= 0)
        return ONE;
    if (x >= 8 * ONE)
        return 0;
    i = x >> 10;
    return exp_tbl[i] + ((exp_tbl[i + 1] - exp_tbl[i]) * (x & 1023) >> 10);
}

// Every 2500 ms is a whole number of angle units for rates in steps of 100,
// which keeps long runs from overflowing without changing a single angle.
static int Spin(int ms, int milli_rad_per_s) {
    return (ms / 2500 & (ONE - 1)) * (milli_rad_per_s / 100 * 163) +
           ms % 2500 * milli_rad_per_s / 1000 * 652 / 1000;
}

static int Rand(void) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (rng >> 8) & (ONE - 1);
}

static int Hash(int n) {
    unsigned x = (unsigned)n;
    x = (x ^ (x >> 16)) * 0x45D9F3B;
    x = (x ^ (x >> 16)) * 0x45D9F3B;
    x ^= x >> 16;
    return (int)(x >> 20);
}

static int Sx(int v) { return v * g.sx >> 12; }
static int Sy(int v) { return v * g.sy >> 12; }

static int Normalize(int* v) {
    int len;
    while (Abs(v[0]) > 16384 || Abs(v[1]) > 16384 || Abs(v[2]) > 16384) {
        v[0] >>= 1;
        v[1] >>= 1;
        v[2] >>= 1;
    }
    len = SquareRoot0(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (len) {
        v[0] = v[0] * ONE / len;
        v[1] = v[1] * ONE / len;
        v[2] = v[2] * ONE / len;
    }
    return len;
}

static void Link(Chain* c, void* p) {
    if (c->tail)
        catPrim(c->tail, p);
    else
        c->head = p;
    c->tail = p;
}

static void Commit(OT_TYPE* ot, Chain* c) {
    if (c->head)
        addPrims(ot, c->head, c->tail);
    c->head = c->tail = 0;
}

static int OtIndex(int otz) {
    otz = (otz - g.zbias) >> OT_SHIFT;
    return otz < OT_NEAR ? OT_NEAR : otz > OT_FAR ? OT_FAR : otz;
}

// Fades are folded into vertex colours, scaled once per effect by fade_mul: a
// full-screen semi-transparent tile costs a whole extra pass on the GPU.
#define SET_RGB(p, i, r, gg, b)                                                \
    do {                                                                       \
        u_char* c_ = &(p)->r0 + (i) * 8;                                       \
        c_[0] = Clamp8(r);                                                     \
        c_[1] = Clamp8(gg);                                                    \
        c_[2] = Clamp8(b);                                                     \
    } while (0)

static int Fade(int v) { return v * g.fade_mul >> 12; }

static void FadeRGB0(u_char* rgb, int r, int gg, int b, int add) {
    rgb[0] = Clamp8((r * g.fade_mul >> 12) + add);
    rgb[1] = Clamp8((gg * g.fade_mul >> 12) + add);
    rgb[2] = Clamp8((b * g.fade_mul >> 12) + add);
}

// Lights one horizontal edge of a gradient quad, the other edge stays black
static void Edge(POLY_G4* p, int bottom, const int* a, const int* b) {
    setRGB0(p, 0, 0, 0);
    setRGB1(p, 0, 0, 0);
    setRGB2(p, 0, 0, 0);
    setRGB3(p, 0, 0, 0);
    if (bottom) {
        setRGB2(p, a[0], a[1], a[2]);
        setRGB3(p, b[0], b[1], b[2]);
    } else {
        setRGB0(p, a[0], a[1], a[2]);
        setRGB1(p, b[0], b[1], b[2]);
    }
}

static void Box(TILE* p, Chain* c, int x, int y, int w, int h, int grey) {
    setXY0(p, x, y);
    setWH(p, w, h);
    setRGB0(p, grey, grey, grey);
    Link(c, p);
}

static void SxyToXY(int sxy, short* x, short* y) {
    *x = (short)(sxy & 0xFFFF);
    *y = (short)(sxy >> 16);
}

// Converts the y-up rotation Ry * Rx * Rz into the y-down GTE space.
static void Rot(MATRIX* m, int rx, int ry, int rz) {
    int cx = Cos(rx), sx = Sin(rx), cy = Cos(ry), sy = Sin(ry);
    int cz = Cos(rz), sz = Sin(rz);
    int sysx = sy * sx >> 12, cysx = cy * sx >> 12;
    m->m[0][0] = (cy * cz + sysx * sz) >> 12;
    m->m[0][1] = -((-cy * sz + sysx * cz) >> 12);
    m->m[0][2] = sy * cx >> 12;
    m->m[1][0] = -(cx * sz >> 12);
    m->m[1][1] = cx * cz >> 12;
    m->m[1][2] = sx;
    m->m[2][0] = (-sy * cz + cysx * sz) >> 12;
    m->m[2][1] = -((sy * sz + cysx * cz) >> 12);
    m->m[2][2] = cy * cx >> 12;
}

static void Place(MATRIX* out, const MATRIX* rot, int scale, int cx, int cy,
                  int cz, int camera) {
    int i, j, row;
    for (i = 0; i < 3; i++) {
        row = camera && i == 0 ? g.cam_x : ONE;
        for (j = 0; j < 3; j++)
            out->m[i][j] = (rot->m[i][j] * scale >> 12) * row >> 12;
    }
    out->t[0] = camera ? cx * g.cam_x >> 12 : cx;
    out->t[1] = cy;
    out->t[2] = cz;
}

static void UseCamera(void) {
    SetRotMatrix(&g.cam);
    SetTransMatrix(&g.cam);
}

static int ProjectWorld(int x, int y, int z, int* sxy) {
    SVECTOR v;
    int p, flag;
    v.vx = x;
    v.vy = y;
    v.vz = z;
    return RotTransPers(&v, sxy, &p, &flag);
}

static void Mode(DR_MODE* p, int dither, int abr) {
    RECT tw = {0, 0, 0, 0};
    SetDrawMode(p, g.dfe, dither, getTPage(0, abr, 0, 0), &tw);
}

static void SphereXf(int time, Xf* x) {
    int k = EaseOut((time - 100) * ONE / 980);
    int charge = Smooth((time - 920) * ONE / 280);
    int sw = ONE - k;
    int rx = RAD(420) + (Sin(Spin(time, 1900)) * RAD(250) >> 12);
    int ry = RAD(600) + Spin(time, 2400) +
             ((charge * charge >> 12) * RAD(3000) >> 12);
    // the shards are cut from the pose at T_SHATTER, so the egg lets go by then
    rx += g.egg.star_pitch * (ONE - charge) >> 12;
    ry += g.egg.star_yaw * (ONE - charge) >> 12;
    Rot(&x->rot, rx, ry, RAD(180));
    x->scale = 6963 + (charge * 1114 >> 12);
    x->cx = (Cos(Spin(time, 5000)) * 224 >> 12) * sw >> 12;
    x->cy = -(29 + ((Sin(Spin(time, 5000)) * 141 >> 12) * sw >> 12));
    x->cz = Lerp(72 * WU, 736, k);
    x->charge = charge;
}

static void LogoXf(int time, Xf* x) {
    int a = Smooth((time - T_SHATTER) * ONE / (T_LOCK - T_SHATTER));
    int h = Smooth((time - T_LOCK) * ONE / 1780);
    int yaw = time < T_LOCK ? Lerp(RAD(-700), RAD(260), a)
                            : Lerp(RAD(260), RAD(100), h);
    int pitch = Lerp(RAD(400), RAD(-300), a);
    int punch = 0, u, k;
    if (time > T_LOCK) {
        u = Min(time - T_LOCK, 2000);
        punch =
            (Sin(Spin(u, 26000)) * 246 >> 12) * ExpNeg(MS_RATE(u, 7000)) >> 12;
    }
    // once settled, a slow sway eases in from rest; the two periods never align
    u = Max(0, g.clock - T_LENGTH);
    k = Smooth(Min(u, 1500) * ONE / 1500);
    yaw += (Sin(Spin(u, 1000)) * RAD(90) >> 12) * k >> 12;
    pitch += (Sin(Spin(u, 700)) * RAD(45) >> 12) * k >> 12;
    Rot(&x->rot, pitch + g.egg.logo_pitch, yaw + g.egg.logo_yaw, 0);
    x->scale = 1016 + (1016 * punch >> 12);
    x->cx = 0;
    x->cy = -29;
    x->cz = Lerp(15 * WU, 11 * WU,
                 EaseOut((time - T_SHATTER) * ONE / (T_LOCK - T_SHATTER)));
}

// Rotates the world light into model space: the transpose of the rotation.
static void ModelLight(const MATRIX* rot, int* lm) {
    int i;
    for (i = 0; i < 3; i++)
        lm[i] = (rot->m[0][i] * light_dir[0] + rot->m[1][i] * light_dir[1] +
                 rot->m[2][i] * light_dir[2]) >>
                12;
}

static void BuildStar(void) {
    static const signed char ico[12][3] = {
        {-1, 2, 0}, {1, 2, 0}, {-1, -2, 0}, {1, -2, 0},
        {0, -1, 2}, {0, 1, 2}, {0, -1, -2}, {0, 1, -2},
        {2, 0, -1}, {2, 0, 1}, {-2, 0, -1}, {-2, 0, 1}};
    static const u_char ico_face[20][3] = {
        {0, 11, 5}, {0, 5, 1},  {0, 1, 7},   {0, 7, 10}, {0, 10, 11},
        {1, 5, 9},  {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
        {3, 9, 4},  {3, 4, 2},  {3, 2, 6},   {3, 6, 8},  {3, 8, 9},
        {4, 9, 5},  {2, 4, 11}, {6, 2, 10},  {8, 6, 7},  {9, 8, 1}};
    int v[SPHERE_VERTS][3], pos[SPHERE_VERTS][3];
    u_char edge[30][3];
    int n_vert = 12, n_edge = 0, n_face = 0;
    int i, j, k, f, mid[3], e0[3], e1[3], n[3], c[3], hue, seg, frac, white;

    for (i = 0; i < 12; i++)
        for (j = 0; j < 3; j++)
            v[i][j] = ico[i][j] == 0        ? 0
                      : Abs(ico[i][j]) == 1 ? (ico[i][j] > 0 ? 2153 : -2153)
                                            : (ico[i][j] > 0 ? 3484 : -3484);
    for (f = 0; f < 20; f++) {
        for (k = 0; k < 3; k++) {
            int a = ico_face[f][k], b = ico_face[f][(k + 1) % 3];
            int lo = Min(a, b), hi = Max(a, b);
            for (i = 0; i < n_edge; i++)
                if (edge[i][0] == lo && edge[i][1] == hi)
                    break;
            if (i == n_edge) {
                for (j = 0; j < 3; j++)
                    v[n_vert][j] = v[a][j] + v[b][j];
                Normalize(v[n_vert]);
                edge[n_edge][0] = lo;
                edge[n_edge][1] = hi;
                edge[n_edge++][2] = n_vert++;
            }
            mid[k] = edge[i][2];
        }
        for (k = 0; k < 3; k++) {
            sc->sphere_face[n_face][0] = ico_face[f][k];
            sc->sphere_face[n_face][1] = mid[k];
            sc->sphere_face[n_face++][2] = mid[(k + 2) % 3];
        }
        sc->sphere_face[n_face][0] = mid[0];
        sc->sphere_face[n_face][1] = mid[1];
        sc->sphere_face[n_face++][2] = mid[2];
    }

    for (i = 0; i < SPHERE_VERTS; i++)
        for (j = 0; j < 3; j++)
            pos[i][j] = i < 12 ? v[i][j] * 5489 >> 12 : v[i][j];

    for (f = 0; f < SPHERE_FACES; f++) {
        u_char* fv = sc->sphere_face[f];
        for (j = 0; j < 3; j++) {
            e0[j] = (pos[fv[1]][j] - pos[fv[0]][j]) >> 4;
            e1[j] = (pos[fv[2]][j] - pos[fv[0]][j]) >> 4;
            c[j] = (pos[fv[0]][j] + pos[fv[1]][j] + pos[fv[2]][j]) >> 4;
        }
        n[0] = e0[1] * e1[2] - e0[2] * e1[1];
        n[1] = e0[2] * e1[0] - e0[0] * e1[2];
        n[2] = e0[0] * e1[1] - e0[1] * e1[0];
        if ((n[0] >> 4) * c[0] + (n[1] >> 4) * c[1] + (n[2] >> 4) * c[2] < 0) {
            k = fv[1];
            fv[1] = fv[2];
            fv[2] = k;
            n[0] = -n[0];
            n[1] = -n[1];
            n[2] = -n[2];
        }
        Normalize(n);
        // store y-down: flip y and swap winding so outward stays outward
        k = fv[1];
        fv[1] = fv[2];
        fv[2] = k;
        sc->sphere_nrm[f].vx = n[0];
        sc->sphere_nrm[f].vy = -n[1];
        sc->sphere_nrm[f].vz = n[2];
    }

    for (i = 0; i < SPHERE_VERTS; i++) {
        sc->sphere_pos[i].vx = pos[i][0] >> 4;
        sc->sphere_pos[i].vy = -pos[i][1] >> 4;
        sc->sphere_pos[i].vz = pos[i][2] >> 4;
        hue = (ratan2(v[i][2], v[i][0]) + 2048 + (v[i][1] * 901 >> 12)) &
              (ONE - 1);
        seg = hue * 4 >> 12;
        frac = Smooth((hue * 4) & (ONE - 1));
        white = Max(0, v[i][1]) >> 2;
        for (j = 0; j < 3; j++)
            sc->sphere_col[i][j] = Lerp(
                Lerp(brand[seg][j], brand[(seg + 1) & 3][j], frac), 255, white);
    }
}

static int Interval(
    const int* a, const int* b, const int* poly, int* t0, int* t1) {
    int lo = 0, hi = ONE, i, ex, ey, f0, df, tc;
    for (i = 0; i < 4; i++) {
        const int* v = poly + i * 2;
        const int* w = poly + ((i + 1) & 3) * 2;
        ex = w[0] - v[0];
        ey = w[1] - v[1];
        f0 = ex * (a[1] - v[1]) - ey * (a[0] - v[0]);
        df = ex * (b[1] - a[1]) - ey * (b[0] - a[0]);
        if (df == 0) {
            if (f0 < 0)
                return 0;
            continue;
        }
        tc = -f0 * ONE / df;
        if (df > 0)
            lo = Max(lo, tc);
        else
            hi = Min(hi, tc);
        if (lo > hi)
            return 0;
    }
    *t0 = lo;
    *t1 = hi;
    return 1;
}

static void PushLetterTri(
    int letter, int face, int* a, int* b, int* c, int* out) {
    int e0[3], e1[3], n[3], i, *tmp, *v[3];
    Shard* s;
    for (i = 0; i < 3; i++) {
        e0[i] = b[i] - a[i];
        e1[i] = c[i] - a[i];
    }
    n[0] = e0[1] * e1[2] - e0[2] * e1[1];
    n[1] = e0[2] * e1[0] - e0[0] * e1[2];
    n[2] = e0[0] * e1[1] - e0[1] * e1[0];
    if (n[0] * out[0] + n[1] * out[1] + n[2] * out[2] < 0) {
        tmp = b;
        b = c;
        c = tmp;
    }
    if (g.shards >= SHARD_MAX)
        return;
    s = &sc->shards[g.shards++];
    s->letter = letter;
    s->face = face;
    // y-down: flip y, swap winding
    v[0] = a;
    v[1] = c;
    v[2] = b;
    for (i = 0; i < 3; i++) {
        s->loc[i].vx = v[i][0];
        s->loc[i].vy = -v[i][1];
        s->loc[i].vz = v[i][2];
    }
    n[0] = out[0];
    n[1] = -out[1];
    n[2] = out[2];
    Normalize(n);
    s->nrm.vx = n[0];
    s->nrm.vy = n[1];
    s->nrm.vz = n[2];
}

static void LetterVertex(const int* p, int back, int* out) {
    out[0] = p[0] - LOGO_HALF_W;
    out[1] = p[1] - LOGO_HALF_H;
    out[2] = back ? LOGO_HALF_D : -LOGO_HALF_D;
}

static void BuildLetters(void) {
    int pts[15][8], i, j, k, q, a2, first, last, n_cut, cur;
    int cut[4][2], s0, s1, p0[2], p1[2], v0[3], v1[3], v2[3], v3[3], out[3];
    int front[3] = {0, 0, -1};

    for (i = 0; i < 15; i++) {
        for (j = 0; j < 4; j++) {
            pts[i][j * 2] =
                logo_pieces[i][1 + j * 2] + logo_pieces[i][0] * LOGO_ADVANCE;
            pts[i][j * 2 + 1] = logo_pieces[i][2 + j * 2];
        }
        for (a2 = 0, j = 0; j < 4; j++)
            a2 += pts[i][j * 2] * pts[i][((j + 1) & 3) * 2 + 1] -
                  pts[i][((j + 1) & 3) * 2] * pts[i][j * 2 + 1];
        if (a2 < 0)
            for (j = 0; j < 2; j++) {
                k = pts[i][j * 2];
                pts[i][j * 2] = pts[i][(3 - j) * 2];
                pts[i][(3 - j) * 2] = k;
                k = pts[i][j * 2 + 1];
                pts[i][j * 2 + 1] = pts[i][(3 - j) * 2 + 1];
                pts[i][(3 - j) * 2 + 1] = k;
            }
    }

    for (first = 0; first < 15; first = last) {
        for (last = first;
             last < 15 && logo_pieces[last][0] == logo_pieces[first][0];)
            last++;
        for (i = first; i < last; i++) {
            for (j = 1; j < 3; j++) {
                int a[2] = {pts[i][0] * 8, pts[i][1] * 8};
                int b[2] = {pts[i][j * 2] * 8, pts[i][j * 2 + 1] * 8};
                int c[2] = {pts[i][j * 2 + 2] * 8, pts[i][j * 2 + 3] * 8};
                LetterVertex(a, 0, v0);
                LetterVertex(b, 0, v1);
                LetterVertex(c, 0, v2);
                PushLetterTri(logo_pieces[i][0], 0, v0, v1, v2, front);
            }
            for (j = 0; j < 4; j++) {
                int* a = &pts[i][j * 2];
                int* b = &pts[i][((j + 1) & 3) * 2];
                for (n_cut = 0, q = first; q < last; q++)
                    if (q != i && Interval(a, b, pts[q], &s0, &s1) && s1 > s0) {
                        for (k = n_cut++; k > 0 && cut[k - 1][0] > s0; k--) {
                            cut[k][0] = cut[k - 1][0];
                            cut[k][1] = cut[k - 1][1];
                        }
                        cut[k][0] = s0;
                        cut[k][1] = s1;
                    }
                out[0] = b[1] - a[1];
                out[1] = -(b[0] - a[0]);
                out[2] = 0;
                for (cur = 0, k = 0; k <= n_cut; k++) {
                    s0 = cur;
                    s1 = k < n_cut ? cut[k][0] : ONE;
                    if (k < n_cut)
                        cur = Max(cur, cut[k][1]);
                    if (s1 - s0 < 82)
                        continue;
                    p0[0] = a[0] * 8 + ((b[0] - a[0]) * 8 * s0 >> 12);
                    p0[1] = a[1] * 8 + ((b[1] - a[1]) * 8 * s0 >> 12);
                    p1[0] = a[0] * 8 + ((b[0] - a[0]) * 8 * s1 >> 12);
                    p1[1] = a[1] * 8 + ((b[1] - a[1]) * 8 * s1 >> 12);
                    LetterVertex(p0, 0, v0);
                    LetterVertex(p1, 0, v1);
                    LetterVertex(p1, 1, v2);
                    LetterVertex(p0, 1, v3);
                    PushLetterTri(logo_pieces[i][0], 1, v0, v1, v2, out);
                    PushLetterTri(logo_pieces[i][0], 1, v0, v2, v3, out);
                }
            }
        }
    }
}

static void BuildShards(void) {
    Xf x;
    MATRIX m;
    VECTOR w;
    int i, j, k, flag, v[3], ly;
    short face_x[SPHERE_FACES];
    u_char face_order[SPHERE_FACES];
    Shard tmp;
    Shard* s;
    ShardMotion* mo;
    const SVECTOR* src;

    SphereXf(T_SHATTER, &x);
    g.sc[0] = x.cx;
    g.sc[1] = x.cy;
    g.sc[2] = x.cz;
    Place(&m, &x.rot, x.scale >> 2, x.cx, x.cy, x.cz, 0);
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    for (i = 0; i < SPHERE_FACES; i++) {
        face_x[i] = 0;
        for (j = 0; j < 3; j++) {
            RotTrans(&sc->sphere_pos[sc->sphere_face[i][j]], &w, &flag);
            sc->stage.shatter_pos[i][j].vx = w.vx;
            sc->stage.shatter_pos[i][j].vy = w.vy;
            sc->stage.shatter_pos[i][j].vz = w.vz;
            face_x[i] += w.vx;
        }
        for (k = i; k > 0 && face_x[face_order[k - 1]] > face_x[i]; k--)
            face_order[k] = face_order[k - 1];
        face_order[k] = i;
    }

#define SHARD_X(s) ((s)->loc[0].vx + (s)->loc[1].vx + (s)->loc[2].vx)
    for (i = 1; i < g.shards; i++) {
        tmp = sc->shards[i];
        for (k = i; k > 0 && SHARD_X(&sc->shards[k - 1]) > SHARD_X(&tmp); k--)
            sc->shards[k] = sc->shards[k - 1];
        sc->shards[k] = tmp;
    }
#undef SHARD_X

    for (i = 0; i < g.shards; i++) {
        s = &sc->shards[i];
        mo = &sc->stage.motion[i];
        mo->src = face_order[i * SPHERE_FACES / g.shards];
        for (j = 0; j < 3; j++) {
            ly = (LOGO_HALF_H - s->loc[j].vy) * ONE / (LOGO_HALF_H * 2);
            for (k = 0; k < 3; k++)
                s->col[j][k] =
                    s->face == 0
                        ? Lerp(letter_pal[s->letter][1][k],
                               letter_pal[s->letter][0][k], ly)
                        : Lerp(letter_pal[s->letter][2][k],
                               letter_pal[s->letter][1][k], ly * 2253 >> 12);
        }
        src = sc->stage.shatter_pos[mo->src];
        for (j = 0; j < 3; j++)
            v[j] = ((&src[0].vx)[j] + (&src[1].vx)[j] + (&src[2].vx)[j]) / 3 -
                   g.sc[j];
        Normalize(v);
        for (j = 0; j < 3; j++)
            v[j] += (Rand() - 2048) * 2867 >> 12;
        Normalize(v);
        for (j = 0; j < 3; j++)
            mo->dir[j] = v[j] * 127 >> 12;
        mo->mag = 141 + (Rand() * 205 >> 12);
        for (j = 0; j < 3; j++)
            v[j] = Rand() - 2048;
        Normalize(v);
        for (j = 0; j < 3; j++)
            mo->axis[j] = v[j] * 127 >> 12;
        mo->spin = (Rand() < 2048 ? -1 : 1) * RAD(3000 + (Rand() * 6000 >> 12));
        mo->swirl =
            (Rand() < 1229 ? -1 : 1) * RAD(1200 + (Rand() * 1800 >> 12));
        s->t0 = 1280 + s->letter * 60 + (Rand() * 140 >> 12);
        g.settle = Max(g.settle, s->t0 + 600 + 70);
        for (j = 0; j < 3; j++) {
            const u_char* c0 = sc->sphere_col[sc->sphere_face[mo->src][j]];
            sc->stage.shard_rgb[i][0][j] = c0[0] | c0[1] << 8 | c0[2] << 16;
            sc->stage.shard_rgb[i][1][j] =
                s->col[j][0] | s->col[j][1] << 8 | s->col[j][2] << 16;
        }
    }
}

static void BuildStars(void) {
    int i;
    for (i = 0; i < 32; i++)
        sc->star_recip[i] = ONE / (i + 1);
    for (i = 0; i < STAR_COUNT; i++) {
        sc->stars[i].x = (Rand() * 2 - ONE) * 26 * WU >> 12;
        sc->stars[i].y = (Rand() * 2 - ONE) * 19 * WU >> 12;
        sc->stars[i].z = Rand() * STAR_SPAN >> 12;
        sc->stars[i].b = 1434 + (Rand() * 2662 >> 12);
    }
}

static int GlyphIndex(char ch) {
    int i;
    if (ch >= 'a' && ch <= 'z')
        ch -= 'a' - 'A';
    for (i = 0; font_chars[i]; i++)
        if (font_chars[i] == ch)
            return i;
    return -1;
}

static void BuildTitle(void) {
    static const char text[] = PSYZ_TITLE;
    int k = g.text_k, i, n = 0, x = 0, track = 2 * k, word = 1, accent = 0, gi;
    int max_w = g.w - Sx(16);
    RECT rc;

    setRECT(&rc, FONT_X, FONT_Y, 64, 14);
    LoadImage(&rc, (u_long*)font_tex);

    for (i = 0; text[i] && n < TITLE_MAX; i++) {
        gi = GlyphIndex(text[i]);
        if (gi < 0) {
            x += 3 * k + track;
            word = 1;
            continue;
        }
        sc->title[n].x = x;
        sc->title[n].u = (gi & 31) * 8;
        sc->title[n].v = (gi >> 5) * 7;
        sc->title[n].w = font_width[gi];
        sc->title[n].accent = word && gi < 26 ? 1 + (accent++ & 3) : 0;
        word = 0;
        x += font_width[gi] * k + track;
        n++;
    }
    g.title_n = n;
    g.title_w = n ? x - track : 0;
    if (g.title_w > max_w)
        for (i = 0; i < n; i++)
            sc->title[i].x = sc->title[i].x * max_w / g.title_w;
    g.title_w = Min(g.title_w, max_w);
    g.title_x = (g.w - g.title_w) / 2;
    // long titles type faster, so the wave still ends as the fade begins
    g.type_step = n > 1 ? Min(20, 320 / (n - 1)) : 20;
    g.wave_at = 2480 + (n - 1) * g.type_step + 180;
    g.wave_rate = ((10 + n - 1) << 12) / (2 * WAVE_MS);
}

static void InitPrims(DB* db) {
    int i, j;
    Mode(&db->mode[MODE_BG], 1, 1);
    Mode(&db->mode[MODE_MODELS], 0, 1);
    Mode(&db->mode[MODE_EFFECTS], 1, 1);
    Mode(&db->mode[MODE_TEXT], 0, 1);
    for (i = 0; i < FAN_SEG; i++) {
        setPolyG3(&db->glow.core[i]);
        setPolyG3(&db->halo[i]);
        setSemiTrans(&db->halo[i], 1);
        for (j = 0; j < GLOW_RINGS - 1; j++) {
            setPolyG4(&db->glow.ring[j][i]);
        }
    }
    for (i = 0; i < STAR_COUNT; i++) {
        setLineG2(&db->star[i]);
        setSemiTrans(&db->star[i], 1);
    }
    for (i = 0; i < SHARD_MAX; i++)
        setPolyG3(&db->model[i]);
    for (i = 0; i < RING_COUNT; i++) {
        setPolyG3(&db->ring[i]);
        setSemiTrans(&db->ring[i], 1);
    }
    for (i = 0; i < 4; i++) {
        setPolyG4(&db->flare[i]);
        setSemiTrans(&db->flare[i], 1);
    }
    for (i = 0; i < SHOCK_SEG * 2; i++) {
        setPolyG4(&db->shock[i >> 1][i & 1]);
        setSemiTrans(&db->shock[i >> 1][i & 1], 1);
    }
}

static void InitFinale(Finale* f) {
    int i;
    for (i = 0; i < GLINT_MAX; i++) {
        setPolyG3(&f->glint[i]);
        setSemiTrans(&f->glint[i], 1);
    }
    for (i = 0; i < SPARK_SLOTS * 8; i++) {
        setPolyG3(&f->sparkle[i >> 3][i & 7]);
        setSemiTrans(&f->sparkle[i >> 3][i & 7], 1);
    }
    for (i = 0; i < 4; i++)
        setTile(&f->bar[i]);
    for (i = 0; i < TITLE_MAX * 2; i++) {
        POLY_FT4* p = &f->glyph[i >> 1][i & 1];
        const TitleChar* t = &sc->title[i >> 1];
        int k = g.text_k, shadow = i & 1 ? 0 : k;
        int x = g.title_x + t->x + shadow, y = Sy(154) + shadow;
        setPolyFT4(p);
        // a textured prim's texpage also sets the blend mode for later prims
        p->tpage = getTPage(0, 1, FONT_X, FONT_Y);
        p->clut = getClut(CLUT_X, CLUT_Y);
        setUV4(p, t->u, t->v, t->u + t->w, t->v, t->u, t->v + 7, t->u + t->w,
               t->v + 7);
        setXYWH(p, x, y, t->w * k, 7 * k);
    }
}

static const u_char bg_top[3] = {5, 3, 14};
static const u_char bg_bottom[3] = {17, 6, 33};

// Background gradient at screen row y, extrapolated past the edges.
static void BgAt(int y, int* rgb) {
    int t = y * g.bg_inv_h >> 8;
    rgb[0] = g.bg_base[0] + (g.bg_slope[0] * t >> 12);
    rgb[1] = g.bg_base[1] + (g.bg_slope[1] * t >> 12);
    rgb[2] = g.bg_base[2] + (g.bg_slope[2] * t >> 12);
}

// With rings this is the background: an opaque glow with the gradient added
// per vertex, then gradient-only rings past the corners, so each pixel is
// filled once.
static void GlowFanDraw(POLY_G3* core, POLY_G4 (*ring)[FAN_SEG], Chain* c,
                        int cx, int cy, int rx, int ry, int r, int gg, int b) {
    static const short ring_mul[GLOW_RINGS + 1] = {4096, 1820, 455, 0, 0, 0};
    int i, k, c0, s0, c1, s1, dx, dy, far, rings = ring ? GLOW_RINGS : 1;
    int x[GLOW_RINGS + 1][2], y[GLOW_RINGS + 1][2];
    int krx[GLOW_RINGS + 1], kry[GLOW_RINGS + 1];
    int bg[GLOW_RINGS + 1][2][3], col[GLOW_RINGS + 1][3];
    r = Fade(r);
    gg = Fade(gg);
    b = Fade(b);
    if (!ring && r <= 0 && gg <= 0 && b <= 0)
        return;
    for (k = 1; k <= 3 && k <= rings; k++) {
        krx[k] = rx * k / (ring ? 3 : 1);
        kry[k] = ry * k / (ring ? 3 : 1);
    }
    if (ring) {
        // 16 segments cover a circle only up to cos(pi/16) of its radius
        dx = Max(cx, g.w - cx);
        dy = Max(cy, g.h - cy);
        far = SquareRoot0(dx * dx + dy * dy) * 4177 / ONE + 2;
        far = Max(far, Max(rx, ry));
        krx[4] = (rx + far) / 2;
        kry[4] = (ry + far) / 2;
        krx[5] = kry[5] = far;
    }
    for (k = 0; k <= GLOW_RINGS; k++) {
        col[k][0] = r * ring_mul[k] >> 12;
        col[k][1] = gg * ring_mul[k] >> 12;
        col[k][2] = b * ring_mul[k] >> 12;
    }
    for (k = 0; k < (GLOW_RINGS + 1) * 2 * 3; k++)
        (&bg[0][0][0])[k] = 0;
    if (ring)
        BgAt(cy, bg[0][0]);
    c1 = ONE;
    s1 = 0;
    for (i = 0; i < FAN_SEG; i++) {
        c0 = c1;
        s0 = s1;
        c1 = Cos((i + 1) * (ONE / FAN_SEG));
        s1 = Sin((i + 1) * (ONE / FAN_SEG));
        for (k = 1; k <= rings; k++) {
            x[k][0] = cx + (c0 * krx[k] >> 12);
            y[k][0] = cy + (s0 * kry[k] >> 12);
            x[k][1] = cx + (c1 * krx[k] >> 12);
            y[k][1] = cy + (s1 * kry[k] >> 12);
            if (ring) {
                BgAt(y[k][0], bg[k][0]);
                BgAt(y[k][1], bg[k][1]);
            }
        }
        k = ring ? 1 : 3;
        setXY3(&core[i], cx, cy, x[1][0], y[1][0], x[1][1], y[1][1]);
        SET_RGB(
            &core[i], 0, r + bg[0][0][0], gg + bg[0][0][1], b + bg[0][0][2]);
        SET_RGB(&core[i], 1, col[k][0] + bg[1][0][0], col[k][1] + bg[1][0][1],
                col[k][2] + bg[1][0][2]);
        SET_RGB(&core[i], 2, col[k][0] + bg[1][1][0], col[k][1] + bg[1][1][1],
                col[k][2] + bg[1][1][2]);
        Link(c, &core[i]);
        for (k = 0; ring && k < GLOW_RINGS - 1; k++) {
            POLY_G4* p = &ring[k][i];
            int* c0 = col[k + 1];
            int* c1 = col[k + 2];
            setXY4(p, x[k + 1][0], y[k + 1][0], x[k + 1][1], y[k + 1][1],
                   x[k + 2][0], y[k + 2][0], x[k + 2][1], y[k + 2][1]);
            SET_RGB(p, 0, c0[0] + bg[k + 1][0][0], c0[1] + bg[k + 1][0][1],
                    c0[2] + bg[k + 1][0][2]);
            SET_RGB(p, 1, c0[0] + bg[k + 1][1][0], c0[1] + bg[k + 1][1][1],
                    c0[2] + bg[k + 1][1][2]);
            SET_RGB(p, 2, c1[0] + bg[k + 2][0][0], c1[1] + bg[k + 2][0][1],
                    c1[2] + bg[k + 2][0][2]);
            SET_RGB(p, 3, c1[0] + bg[k + 2][1][0], c1[1] + bg[k + 2][1][1],
                    c1[2] + bg[k + 2][1][2]);
            Link(c, p);
        }
    }
}

static void Background(DB* db, int gx, int gy, int glow) {
    Chain c = {0, 0};
    Link(&c, &db->mode[MODE_BG]);
    GlowFanDraw(db->glow.core, db->glow.ring, &c, gx, gy, Sx(165), Sy(122),
                70 * glow >> 12, 34 * glow >> 12, 130 * glow >> 12);
    Commit(db->ot + OT_BG, &c);
}

static void Stars(DB* db, int dist, int tail, int alpha) {
    int i, z, b, head, back, n, dx, dy, r, gg, bl, tail_mul;
    int span = dist % STAR_SPAN, max_n = Sx(360);
    short hx, hy, tx, ty;
    SVECTOR v;
    LINE_G2* p;
    UseCamera();
    for (i = 0; i < STAR_COUNT; i++) {
        const Star* s = &sc->stars[i];
        z = s->z - span;
        if (z < 0)
            z += STAR_SPAN;
        z += 77;
        b = (s->b * alpha >> 12) * Min((STAR_SPAN - z) * 41 >> 4, ONE) >> 12;
        if (b < 28)
            continue;
        // keep both ends inside the GTE's +-1023 screen range
        if ((Abs(s->x) * g.sx >> 12) * FOCAL > z * 1000 ||
            (Abs(s->y) * g.sy >> 12) * FOCAL > z * 1000)
            continue;
        v.vx = s->x;
        v.vy = s->y;
        v.vz = z;
        gte_ldv0(&v);
        gte_rtps();
        gte_stsxy(&head);
        back = head;
        if (tail > 16) {
            v.vz = z + tail;
            gte_ldv0(&v);
            gte_rtps();
            gte_stsxy(&back);
        }
        SxyToXY(head, &hx, &hy);
        SxyToXY(back, &tx, &ty);
        dx = hx - tx;
        dy = hy - ty;
        n = Max(Max(Abs(dx), Abs(dy)), 1);
        if (n > max_n)
            continue;
        r = Clamp8(150 * b >> 12);
        gg = Clamp8(165 * b >> 12);
        bl = Clamp8(255 * b >> 12);
        if (!dx && !dy) {
            // the GPU paints a one-pixel line with the start colour only; the
            // prototype adds both ends there, 1.5x the head
            r = Clamp8(r * 3 >> 1);
            gg = Clamp8(gg * 3 >> 1);
            bl = Clamp8(bl * 3 >> 1);
            tail_mul = ONE;
        } else
            tail_mul = n < 32 ? sc->star_recip[n] : 0;
        p = &db->star[i];
        setXY2(p, tx, ty, hx, hy);
        setRGB0(
            p, r * tail_mul >> 12, gg * tail_mul >> 12, bl * tail_mul >> 12);
        setRGB1(p, r, gg, bl);
        addPrim(db->ot + OT_STARS, p);
    }
}

static void StarField(DB* db, int time) {
    int u = Sat(time * ONE / 1250), iu = ONE - u;
    int dist = (95 * WU * (ONE - (iu * iu >> 12)) >> 12) +
               3 * WU * (g.clock % 30000) / 1000;
    int speed = (u < ONE ? 152 * iu : 0) + 3 * ONE;
    int tail =
        Min((speed * 224 / 100 >> 12) + (g.egg.warp_v * 34 >> 8), 7 * WU);
    int alpha = Ramp(time, 100, 300);
    if (time >= T_SHATTER)
        alpha = alpha * Lerp(ONE, 2253, Ramp(time, T_SHATTER, 1000)) >> 12;
    Stars(db, dist + g.egg.warp, tail, alpha * g.fade_mul >> 12);
}

static void PsyzStar(DB* db, int time) {
    Xf x;
    MATRIX m;
    SVECTOR v;
    POLY_G3* p;
    int* sxy = sc->sphere_sxy;
    int* otz = sc->sphere_otz;
    int lm[3];
    int i, j, jit, fr, pz, flag, dot, k, hl, n = 0;

    SphereXf(time, &x);
    Place(&m, &x.rot, x.scale >> 2, x.cx, x.cy, x.cz, 1);
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    jit = x.charge * 23 / x.scale;
    fr = time * 60 / 1000;
    for (j = 0; j < SPHERE_VERTS; j++) {
        v = sc->sphere_pos[j];
        if (jit) {
            v.vx += (Hash(j * 131 + fr * 7) - 2048) * 2 * jit >> 12;
            v.vy += (Hash(j * 17 + fr * 3 + 99) - 2048) * 2 * jit >> 12;
        }
        otz[j] = RotTransPers(&v, &sxy[j], &pz, &flag);
    }
    ModelLight(&x.rot, lm);
    for (i = 0; i < SPHERE_FACES; i++) {
        const u_char* f = sc->sphere_face[i];
        if (NormalClip(sxy[f[0]], sxy[f[1]], sxy[f[2]]) >= 0)
            continue;
        dot = Max(
            0, (sc->sphere_nrm[i].vx * lm[0] + sc->sphere_nrm[i].vy * lm[1] +
                sc->sphere_nrm[i].vz * lm[2]) >>
                   12);
        k = 1311 + (dot * 3891 >> 12) + (x.charge >> 1);
        hl = (dot > 3686 ? (dot - 3686) * 900 >> 12 : 0) +
             (x.charge * 60 >> 12) + g.flash;
        if (g.fade_mul != ONE) {
            k = Fade(k);
            hl = Fade(hl);
        }
        p = &db->model[n++];
        SxyToXY(sxy[f[0]], &p->x0, &p->y0);
        SxyToXY(sxy[f[1]], &p->x1, &p->y1);
        SxyToXY(sxy[f[2]], &p->x2, &p->y2);
        for (j = 0; j < 3; j++) {
            const u_char* c = sc->sphere_col[f[j]];
            SET_RGB(p, j, (c[0] * k >> 12) + hl, (c[1] * k >> 12) + hl,
                    (c[2] * k >> 12) + hl);
        }
        addPrim(db->ot + OtIndex((otz[f[0]] + otz[f[1]] + otz[f[2]]) / 3), p);
    }
}

static int Blast(int time) {
    return ONE - ExpNeg(MS_RATE(time - T_SHATTER, 4200));
}

static void Shatter(const ShardMotion* mo, int blast, int e, SVECTOR* out) {
    const SVECTOR* src = sc->stage.shatter_pos[mo->src];
    int push = mo->mag * blast >> 12;
    int dx = mo->dir[0] * push >> 7, dy = mo->dir[1] * push >> 7,
        dz = mo->dir[2] * push >> 7;
    int ang = (mo->swirl * blast >> 12) * (ONE - e) >> 12;
    int ca = Cos(ang), sa = Sin(ang), j, rx, ry;
    for (j = 0; j < 3; j++) {
        rx = src[j].vx + dx - g.sc[0];
        ry = src[j].vy + dy - g.sc[1];
        out[j].vx = g.sc[0] + ((rx * ca - ry * sa) >> 12);
        out[j].vy = g.sc[1] + ((rx * sa + ry * ca) >> 12);
        out[j].vz = src[j].vz + dz;
    }
}

// Rotation of a shard spinning around its own axis (Rodrigues' formula).
static void TumbleMatrix(const ShardMotion* mo, int ang, MATRIX* m) {
    int k0 = mo->axis[0], k1 = mo->axis[1], k2 = mo->axis[2];
    int c = Cos(ang), sn = Sin(ang), C = ONE - c;
    m->m[0][0] = c + ((k0 * k0 >> 2) * C >> 12);
    m->m[1][1] = c + ((k1 * k1 >> 2) * C >> 12);
    m->m[2][2] = c + ((k2 * k2 >> 2) * C >> 12);
    m->m[0][1] = ((k0 * k1 >> 2) * C >> 12) - (k2 * sn >> 7);
    m->m[1][0] = ((k0 * k1 >> 2) * C >> 12) + (k2 * sn >> 7);
    m->m[0][2] = ((k0 * k2 >> 2) * C >> 12) + (k1 * sn >> 7);
    m->m[2][0] = ((k0 * k2 >> 2) * C >> 12) - (k1 * sn >> 7);
    m->m[1][2] = ((k1 * k2 >> 2) * C >> 12) - (k0 * sn >> 7);
    m->m[2][1] = ((k1 * k2 >> 2) * C >> 12) + (k0 * sn >> 7);
}

static void AddGlintFace(const POLY_G3* p) {
    short* f;
    if (g.glint_n >= FRONT_MAX)
        return;
    f = g.glint_face[g.glint_n++];
    f[0] = p->x0;
    f[1] = p->y0;
    f[2] = p->x1;
    f[3] = p->y1;
    f[4] = p->x2;
    f[5] = p->y2;
}

// Shards fly back into the letters in passes small enough for the 4KB I-cache.
// Where each shard lands in the wordmark this frame, in view space.
static void FlyLanding(int time) {
    Xf x;
    MATRIX m;
    VECTOR v;
    int i, j, flag;
    LogoXf(time, &x);
    Place(&m, &x.rot, x.scale, x.cx, x.cy, x.cz, 0);
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    sc->stage.fly.back.vx = m.m[0][2] * (LOGO_HALF_D * 2) >> 12;
    sc->stage.fly.back.vy = m.m[1][2] * (LOGO_HALF_D * 2) >> 12;
    sc->stage.fly.back.vz = m.m[2][2] * (LOGO_HALF_D * 2) >> 12;
    for (i = 0; i < g.shards; i++)
        for (j = 0; j < 3; j++) {
            RotTrans(&sc->shards[i].loc[j], &v, &flag);
            sc->stage.fly.pos[i][j].vx = v.vx;
            sc->stage.fly.pos[i][j].vy = v.vy;
            sc->stage.fly.pos[i][j].vz = v.vz;
        }
}

// Blends the burst toward the landing and takes the untumbled face normal.
static void FlyMove(int time) {
    SVECTOR sv[3];
    int i, j, e, sh, e0[3], e1[3], n[3], blast = Blast(time);
    for (i = 0; i < g.shards; i++) {
        const ShardMotion* mo = &sc->stage.motion[i];
        SVECTOR* pos = sc->stage.fly.pos[i];
        e = Smooth((time - sc->shards[i].t0) * 6991 >> 10);
        sc->stage.fly.e[i] = e;
        if (e < ONE) {
            Shatter(mo, blast, e, sv);
            for (j = 0; j < 3; j++) {
                if (e > 0) {
                    sv[j].vx += (pos[j].vx - sv[j].vx) * e >> 12;
                    sv[j].vy += (pos[j].vy - sv[j].vy) * e >> 12;
                    sv[j].vz += (pos[j].vz - sv[j].vz) * e >> 12;
                }
                pos[j].vx = sv[j].vx;
                pos[j].vy = sv[j].vy;
                pos[j].vz = sv[j].vz;
            }
        }
        for (j = 0; j < 3; j++) {
            e0[j] = (&pos[1].vx)[j] - (&pos[0].vx)[j];
            e1[j] = (&pos[2].vx)[j] - (&pos[0].vx)[j];
        }
        n[0] = e0[1] * e1[2] - e0[2] * e1[1];
        n[1] = e0[2] * e1[0] - e0[0] * e1[2];
        n[2] = e0[0] * e1[1] - e0[1] * e1[0];
        for (sh = Max(Max(Abs(n[0]), Abs(n[1])), Abs(n[2])); sh > 16384;
             sh >>= 1) {
            n[0] >>= 1;
            n[1] >>= 1;
            n[2] >>= 1;
        }
        sc->stage.fly.nrm[i].vx = n[0];
        sc->stage.fly.nrm[i].vy = n[1];
        sc->stage.fly.nrm[i].vz = n[2];
    }
}

// Tumbles, projects and culls; the survivors fill db->model in shard order.
static void FlyProject(DB* db, int time) {
    MATRIX r;
    POLY_G3* p;
    SVECTOR *sv, *vp, rel[3];
    int i, j, e, tumble, otz, opz, n[3], w[3], cen[3];
    int spin_age = 1024 + (time - T_SHATTER) * 5734 / 1000;
    int scale_x = g.cam_x != ONE, camera = 0;
    sc->stage.fly.n = 0;
    for (i = 0; i < g.shards; i++) {
        const ShardMotion* mo = &sc->stage.motion[i];
        sv = sc->stage.fly.pos[i];
        e = sc->stage.fly.e[i];
        n[0] = sc->stage.fly.nrm[i].vx;
        n[1] = sc->stage.fly.nrm[i].vy;
        n[2] = sc->stage.fly.nrm[i].vz;
        tumble = e < ONE ? (mo->spin * (ONE - e) >> 12) * spin_age >> 12 : 0;
        vp = sv;
        if (tumble) {
            TumbleMatrix(mo, tumble, &r);
            for (j = 0; j < 3; j++)
                cen[j] =
                    ((&sv[0].vx)[j] + (&sv[1].vx)[j] + (&sv[2].vx)[j]) * 1365 >>
                    12;
            // project around the centroid, so the translation is the
            // centroid itself instead of cen - R * cen
            for (j = 0; j < 3; j++) {
                w[j] =
                    (r.m[j][0] * n[0] + r.m[j][1] * n[1] + r.m[j][2] * n[2]) >>
                    12;
                r.t[j] = cen[j];
                rel[j].vx = sv[j].vx - cen[0];
                rel[j].vy = sv[j].vy - cen[1];
                rel[j].vz = sv[j].vz - cen[2];
            }
            vp = rel;
            if (scale_x) {
                r.m[0][0] = r.m[0][0] * g.cam_x >> 12;
                r.m[0][1] = r.m[0][1] * g.cam_x >> 12;
                r.m[0][2] = r.m[0][2] * g.cam_x >> 12;
                r.t[0] = r.t[0] * g.cam_x >> 12;
            }
            n[0] = w[0];
            n[1] = w[1];
            n[2] = w[2];
            gte_SetRotMatrix(&r);
            gte_SetTransMatrix(&r);
            camera = 0;
        } else if (!camera) {
            gte_SetRotMatrix(&g.cam);
            gte_SetTransMatrix(&g.cam);
            camera = 1;
        }

        if (sv[0].vz < 32 || sv[1].vz < 32 || sv[2].vz < 32)
            continue;
        gte_ldv3(&vp[0], &vp[1], &vp[2]);
        gte_rtpt();
        gte_nclip();
        gte_stopz(&opz);
        if (opz >= 0) {
            if (e >= ONE) {
                // a landed front face seen from behind shows on the back of
                // the letter; the vertex order stays, so it winds the other way
                if (sc->shards[i].face != 0)
                    continue;
                for (j = 0; j < 3; j++) {
                    rel[j].vx = sv[j].vx + sc->stage.fly.back.vx;
                    rel[j].vy = sv[j].vy + sc->stage.fly.back.vy;
                    rel[j].vz = sv[j].vz + sc->stage.fly.back.vz;
                }
                gte_ldv3(&rel[0], &rel[1], &rel[2]);
                gte_rtpt();
                gte_nclip();
                gte_stopz(&opz);
                if (opz <= 0)
                    continue;
            }
            n[0] = -n[0];
            n[1] = -n[1];
            n[2] = -n[2];
        }
        p = &db->model[sc->stage.fly.n];
        gte_stsxy3(&p->x0, &p->x1, &p->x2);
        gte_avsz3();
        gte_stotz(&otz);
        sc->stage.fly.nrm[i].vx = n[0];
        sc->stage.fly.nrm[i].vy = n[1];
        sc->stage.fly.nrm[i].vz = n[2];
        sc->stage.fly.otz[sc->stage.fly.n] = otz;
        sc->stage.fly.shard[sc->stage.fly.n] = i;
        sc->stage.fly.facing[sc->stage.fly.n] = opz < 0;
        sc->stage.fly.n++;
    }
}

static void FlyShade(DB* db, int time) {
    POLY_G3* p;
    int o, i, j, e, n[3], len, dot, amb, dif, k, arrive, wh;
    int heat = Max(0, ONE - MS_RATE(time - T_SHATTER, 2400));
    for (o = 0; o < sc->stage.fly.n; o++) {
        i = sc->stage.fly.shard[o];
        const Shard* s = &sc->shards[i];
        e = sc->stage.fly.e[i];
        n[0] = sc->stage.fly.nrm[i].vx;
        n[1] = sc->stage.fly.nrm[i].vy;
        n[2] = sc->stage.fly.nrm[i].vz;
        p = &db->model[o];
        len = SquareRoot0(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        dot = n[0] * light_dir[0] + n[1] * light_dir[1] + n[2] * light_dir[2];
        dot = len && dot > 0 ? dot / len : 0;
        amb = Lerp(1311, s->face == 0 ? 2540 : 1024, e);
        dif = Lerp(3891, s->face == 0 ? 2048 : 5325, e);
        k = amb + (dif * dot >> 12);
        arrive = Max(0, ONE - Abs(time - (s->t0 + 600)) * 59);
        wh = (heat * 200 >> 12) + (arrive * 150 >> 12) + g.flash;
        if (e < ONE && dot > 3686)
            wh += ((dot - 3686) * 700 >> 12) * (ONE - e) >> 12;
        if (g.fade_mul != ONE) {
            k = Fade(k);
            wh = Fade(wh);
        }
        if (e <= 0 || e >= ONE) {
            const u_long* cw = sc->stage.shard_rgb[i][e > 0];
            for (j = 0; j < 3; j++) {
                u_long c = cw[j];
                SET_RGB(p, j, ((c & 255) * k >> 12) + wh,
                        ((c >> 8 & 255) * k >> 12) + wh,
                        ((c >> 16) * k >> 12) + wh);
            }
        } else {
            int k1 = k * e >> 12, k0 = k - k1;
            for (j = 0; j < 3; j++) {
                u_long c0 = sc->stage.shard_rgb[i][0][j],
                       c1 = sc->stage.shard_rgb[i][1][j];
                int r0 = c0 & 255, g0 = c0 >> 8 & 255, b0 = c0 >> 16;
                int r1 = c1 & 255, g1 = c1 >> 8 & 255, b1 = c1 >> 16;
                SET_RGB(p, j, ((r0 * k0 + r1 * k1) >> 12) + wh,
                        ((g0 * k0 + g1 * k1) >> 12) + wh,
                        ((b0 * k0 + b1 * k1) >> 12) + wh);
            }
        }
        addPrim(db->ot + OtIndex(sc->stage.fly.otz[o]), p);
        if (e >= ONE && s->face == 0 && sc->stage.fly.facing[o])
            AddGlintFace(p);
    }
}

static void Reassemble(DB* db, int time) {
    FlyLanding(time);
    FlyMove(time);
    FlyProject(db, time);
    FlyShade(db, time);
}

static void Letters(DB* db, int time) {
    Xf x;
    MATRIX m;
    POLY_G3* p;
    SVECTOR back[3];
    int lm[3], i, j, opz, otz, dot, k, n = 0, wh = Fade(g.flash);

    LogoXf(time, &x);
    Place(&m, &x.rot, x.scale, x.cx, x.cy, x.cz, 1);
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    ModelLight(&x.rot, lm);
    for (i = 0; i < g.shards; i++) {
        Shard* s = &sc->shards[i];
        p = &db->model[n];
        gte_ldv3(&s->loc[0], &s->loc[1], &s->loc[2]);
        gte_rtpt();
        gte_nclip();
        gte_stopz(&opz);
        dot = (s->nrm.vx * lm[0] + s->nrm.vy * lm[1] + s->nrm.vz * lm[2]) >> 12;
        if (opz >= 0) {
            // a front face seen from behind shows on the back of the letter;
            // the vertex order stays, so it winds the other way
            if (s->face != 0)
                continue;
            for (j = 0; j < 3; j++) {
                back[j] = s->loc[j];
                back[j].vz = LOGO_HALF_D;
            }
            gte_ldv3(&back[0], &back[1], &back[2]);
            gte_rtpt();
            gte_nclip();
            gte_stopz(&opz);
            if (opz <= 0)
                continue;
            dot = -dot;
        }
        n++;
        gte_stsxy3(&p->x0, &p->x1, &p->x2);
        // sort by the average depth: a wall's third vertex can sit on the
        // front plane and draw it over the letter's face
        gte_avsz3();
        gte_stotz(&otz);
        dot = Max(0, dot);
        k = s->face == 0 ? 2540 + (2048 * dot >> 12)
                         : 1024 + (5325 * dot >> 12);
        k = Fade(k);
        for (j = 0; j < 3; j++)
            SET_RGB(p, j, (s->col[j][0] * k >> 12) + wh,
                    (s->col[j][1] * k >> 12) + wh,
                    (s->col[j][2] * k >> 12) + wh);
        addPrim(db->ot + OtIndex(otz), p);
        if (s->face == 0)
            AddGlintFace(p);
    }
}

// The arrowheads orbiting the star, and later the wordmark.
static void Ring(DB* db, int time) {
    Xf x;
    MATRIX m;
    SVECTOR v;
    POLY_G3* p;
    int cx, cy, cz, r, a, size16, i, ang, spin, sxy0, sxy1, otz0, otz1;
    int sx, sy, dx, dy, len, wz, depth, depth_k, in, size, tip, back, side;
    short x0, y0, x1, y1;
    const u_char* bc;

    if (time < T_SHATTER) {
        SphereXf(time, &x);
        cx = x.cx;
        cy = x.cy;
        cz = x.cz;
        r = 198 * (2458 + (1638 * Smooth((time - 300) * ONE / 500) >> 12)) >>
            12;
        a = Smooth((time - 350) * ONE / 400);
        size16 = 164;
    } else if (time < 2050) {
        int u = Smooth((time - T_SHATTER) * ONE / 800);
        cx = Lerp(g.sc[0], 0, u);
        cy = Lerp(g.sc[1], -29, u);
        cz = Lerp(g.sc[2], 11 * WU, u);
        r = 198 + (704 * EaseOut((time - T_SHATTER) * ONE / 600) >> 12);
        a = ONE - Smooth((time - T_SHATTER) * ONE / 450);
        size16 = 164;
    } else {
        cx = 0;
        cy = -29;
        cz = 11 * WU;
        r = Lerp(12 * WU, 333, EaseOut((time - 2050) * ONE / 500));
        a = Smooth((time - 2050) * ONE / 300);
        size16 = 118;
    }
    a = Fade(a);
    if (a <= 41)
        return;
    Place(&m, &g.ring, ONE, cx, cy, cz, 1);
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    spin = Spin(g.clock, 1200);
    depth_k = ONE * 10 * ONE / (r * 19);
    for (i = 0; i < RING_COUNT; i++) {
        ang = i * ONE / RING_COUNT + spin;
        v.vy = 0;
        v.vx = Cos(ang - 81) * r >> 12;
        v.vz = Sin(ang - 81) * r >> 12;
        gte_ldv0(&v);
        gte_rtps();
        gte_stsxy(&sxy0);
        gte_stszotz(&otz0);
        v.vx = Cos(ang + 81) * r >> 12;
        v.vz = Sin(ang + 81) * r >> 12;
        gte_ldv0(&v);
        gte_rtps();
        gte_stsxy(&sxy1);
        gte_stszotz(&otz1);
        wz = cz + ((g.ring.m[2][0] * (Cos(ang) * r >> 12) +
                    g.ring.m[2][2] * (Sin(ang) * r >> 12)) >>
                   12);
        if (wz < WU)
            continue;
        SxyToXY(sxy0, &x0, &y0);
        SxyToXY(sxy1, &x1, &y1);
        sx = (x0 + x1) << 3;
        sy = (y0 + y1) << 3;
        dx = x1 - x0;
        dy = y1 - y0;
        len = SquareRoot0(dx * dx + dy * dy);
        if (!len)
            continue;
        len = (ONE << 12) / len;
        dx = dx * len >> 12;
        dy = dy * len >> 12;
        size = size16 * FOCAL / wz * (i % 4 == 0 ? 3 : 2) / 2;
        depth = Sat(2048 - ((wz - cz) * depth_k >> 12));
        in = a * (1229 + (2867 * depth >> 12)) >> 12;
        bc = brand[i & 3];
        tip = size * 16 / 10;
        back = size * 6 / 10;
        side = size * 7 / 10;
        p = &db->ring[i];
        setXY3(p, (sx + (dx * tip >> 12) * g.sx / ONE) >> 4,
               (sy + (dy * tip >> 12) * g.sy / ONE) >> 4,
               (sx + ((-dx * back - dy * side) >> 12) * g.sx / ONE) >> 4,
               (sy + ((-dy * back + dx * side) >> 12) * g.sy / ONE) >> 4,
               (sx + ((-dx * back + dy * side) >> 12) * g.sx / ONE) >> 4,
               (sy + ((-dy * back - dx * side) >> 12) * g.sy / ONE) >> 4);
        SET_RGB(p, 0, ((bc[0] >> 1) + 128) * in >> 12,
                ((bc[1] >> 1) + 128) * in >> 12,
                ((bc[2] >> 1) + 128) * in >> 12);
        in = in * 217 >> 8;
        SET_RGB(p, 1, bc[0] * in >> 12, bc[1] * in >> 12, bc[2] * in >> 12);
        SET_RGB(p, 2, bc[0] * in >> 12, bc[1] * in >> 12, bc[2] * in >> 12);
        addPrim(db->ot + OtIndex((otz0 + otz1) >> 1), p);
    }
}

typedef struct {
    int x, y, s;
} ClipVertex;

// Sutherland-Hodgman against one side of the band edge at `lim`
static int ClipBand(
    const ClipVertex* in, int n, ClipVertex* out, int lim, int above) {
    int i, m = 0, f, ina, inb;
    for (i = 0; i < n; i++) {
        const ClipVertex* a = &in[i];
        const ClipVertex* b = &in[i + 1 < n ? i + 1 : 0];
        ina = above ? a->s >= lim : a->s <= lim;
        inb = above ? b->s >= lim : b->s <= lim;
        if (ina)
            out[m++] = *a;
        if (ina != inb) {
            f = (lim - a->s) * ONE / (b->s - a->s);
            out[m].x = a->x + ((b->x - a->x) * f >> 12);
            out[m].y = a->y + ((b->y - a->y) * f >> 12);
            out[m++].s = lim;
        }
    }
    return m;
}

// Specular band clipped to each front face on the CPU, so any GPU backend draws
// it. Runs after the models fill g.glint_face.
static void Glint(DB* db, int time) {
    static const short knot[8] = {-96, -48, 0, 48, 96, -248, -208, -168};
    static const short level[8] = {0, 64, 255, 64, 0, 0, 153, 0};
    Chain c = {0, 0};
    ClipVertex tri[3], half[4], poly[5];
    int gu = (time - (T_LOCK + 150)) * ONE / 700, glx16, gly16, i, j, k, n, v,
        lo, hi;
    int col[5], used = 0, inv_x = g.inv_sx, inv_y = g.inv_sy, step;
    if (gu <= 0 || gu >= ONE)
        return;
    glx16 = Lerp(40, 300, gu) << 4;
    gly16 = g.logo_y * inv_y >> 8;
    for (i = 0; i < g.glint_n; i++) {
        const short* f = g.glint_face[i];
        lo = 0x7FFFFFFF;
        hi = -lo;
        for (j = 0; j < 3; j++) {
            tri[j].x = f[j * 2] << 4;
            tri[j].y = f[j * 2 + 1] << 4;
            tri[j].s = (f[j * 2] * inv_x >> 8) - glx16 +
                       (((f[j * 2 + 1] * inv_y >> 8) - gly16) * 154 >> 8);
            lo = Min(lo, tri[j].s);
            hi = Max(hi, tri[j].s);
        }
        if (lo > knot[4] || hi < knot[5])
            continue;
        for (k = 0; k < 7; k++) {
            if (k == 4 || lo > knot[k + 1] || hi < knot[k])
                continue;
            n = ClipBand(tri, 3, half, knot[k], 1);
            n = ClipBand(half, n, poly, knot[k + 1], 0);
            step = (level[k + 1] - level[k]) * ONE / (knot[k + 1] - knot[k]);
            for (v = 0; v < n; v++)
                col[v] = Fade(level[k] + ((poly[v].s - knot[k]) * step >> 12));
            for (v = 1; v + 1 < n && used < GLINT_MAX; v++) {
                POLY_G3* p = &db->fin->glint[used++];
                setXY3(p, poly[0].x >> 4, poly[0].y >> 4, poly[v].x >> 4,
                       poly[v].y >> 4, poly[v + 1].x >> 4, poly[v + 1].y >> 4);
                SET_RGB(p, 0, col[0], col[0], col[0]);
                SET_RGB(p, 1, col[v], col[v], col[v]);
                SET_RGB(p, 2, col[v + 1], col[v + 1], col[v + 1]);
                Link(&c, p);
            }
        }
    }
    Commit(db->ot + OT_GLINT, &c);
}

static void Sparkle(POLY_G3* p, Chain* c, int x, int y, int size16, int in) {
    int k, j, w, h, px[4], py[4];
    int col = Fade(255 * in >> 12), colg = Fade(245 * in >> 12);
    for (k = 0; k < 2; k++) {
        w = k ? 2 * 16 : size16;
        h = k ? size16 * 8 / 10 : 2 * 16;
        px[0] = x - (Sx(w) >> 4);
        py[0] = y;
        px[1] = x;
        py[1] = y - (Sy(h) >> 4);
        px[2] = x + (Sx(w) >> 4);
        py[2] = y;
        px[3] = x;
        py[3] = y + (Sy(h) >> 4);
        for (j = 0; j < 4; j++, p++) {
            setXY3(p, x, y, px[j], py[j], px[(j + 1) & 3], py[(j + 1) & 3]);
            SET_RGB(p, 0, col, colg, col);
            SET_RGB(p, 1, 0, 0, 0);
            SET_RGB(p, 2, 0, 0, 0);
            Link(c, p);
        }
    }
}

static void Sparkles(DB* db, int time) {
    Xf x;
    MATRIX m;
    SVECTOR v;
    Chain c = {0, 0};
    int i, u, in, sxy, pz, flag, slot = 0;
    short sx, sy;
    LogoXf(time, &x);
    Place(&m, &x.rot, x.scale, x.cx, x.cy, x.cz, 1);
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    for (i = 0; i < 5 && slot < SPARK_SLOTS; i++) {
        u = (time - spark_at[i][2]) * ONE / 400;
        if (u <= 0 || u >= ONE)
            continue;
        v.vx = spark_at[i][0] * 8 - LOGO_HALF_W;
        v.vy = -(spark_at[i][1] * 8 - LOGO_HALF_H);
        v.vz = -LOGO_HALF_D;
        RotTransPers(&v, &sxy, &pz, &flag);
        SxyToXY(sxy, &sx, &sy);
        in = Sin(u >> 1);
        Sparkle(
            db->fin->sparkle[slot++], &c, sx, sy, (3 * ONE + 7 * in) >> 8, in);
    }
    Commit(db->ot + OT_SPARKLE, &c);
}

static void Flare(DB* db, int cy, int in) {
    static const u_char core[3] = {200, 180, 255}, rim[3] = {60, 40, 110};
    Chain c = {0, 0};
    POLY_G4* p = db->flare;
    int h, i, j, y, x0, x1, xm = g.w / 2, cc[3], ce[3];
    if (in <= 41)
        return;
    h = Sy(3 * ONE + 7 * in) >> 12;
    in = Fade(in);
    for (j = 0; j < 3; j++) {
        cc[j] = Clamp8(core[j] * in >> 12);
        ce[j] = Clamp8(rim[j] * in >> 12);
    }
    for (i = 0; i < 4; i++, p++) {
        y = i < 2 ? cy - h : cy;
        x0 = i & 1 ? xm : 0;
        x1 = i & 1 ? g.w : xm;
        setXY4(p, x0, y, x1, y, x0, y + h, x1, y + h);
        Edge(p, i < 2, i & 1 ? cc : ce, i & 1 ? ce : cc);
        Link(&c, p);
    }
    Commit(db->ot + OT_FLARE, &c);
}

static void Shockwave(DB* db, int cx, int cy, int u) {
    Chain c = {0, 0};
    int k, r, th, in, col[3], i, j, a, rr, px[3][2], py[3][2];
    if (u < 0 || u > 650)
        return;
    k = u * ONE / 650;
    r = 14 * 16 + (220 * 16 * EaseOut(k) >> 12);
    th = (9 * 16 * (ONE - k) >> 12) + 2 * 16;
    in = Fade((ONE - k) * SquareRoot12(ONE - k) >> 12);
    col[0] = Clamp8(110 * in >> 12);
    col[1] = Clamp8(100 * in >> 12);
    col[2] = Clamp8(255 * in >> 12);
    for (i = 0; i < SHOCK_SEG; i++) {
        for (j = 0; j < 2; j++) {
            a = (i + j) * ONE / SHOCK_SEG;
            for (rr = 0; rr < 3; rr++) {
                int rad = r + (rr - 1) * th;
                px[rr][j] = cx + (Sx(Cos(a) * rad >> 12) >> 4);
                py[rr][j] = cy + (Sy((Sin(a) * rad >> 12) * 42 / 100) >> 4);
            }
        }
        for (j = 0; j < 2; j++) {
            POLY_G4* p = &db->shock[i][j];
            setXY4(p, px[j][0], py[j][0], px[j][1], py[j][1], px[j + 1][0],
                   py[j + 1][0], px[j + 1][1], py[j + 1][1]);
            Edge(p, !j, col, col);
            Link(&c, p);
        }
    }
    Commit(db->ot + OT_SHOCK, &c);
}

// The brand colours then white step across the title once; phase in 1/4096 of a
// colour, neighbours half a colour apart.
static void TitleWave(int i, int time, int* rgb) {
    int p = (time - g.wave_at) * g.wave_rate - (i << 11), s, j;
    if (p < 0 || p >= 5 << 12)
        return;
    s = p >> 12;
    for (j = 0; j < 3; j++)
        rgb[j] = s < 4 ? brand[s][j] >> 1 : 128;
}

static void Separator(DB* db, int time) {
    Chain c = {0, 0};
    int half, k, x0, x1;
    if (time <= 2400 || !g.title_n)
        return;
    half = (g.title_w / 2) * EaseOut((time - 2400) * ONE / 250) >> 12;
    for (k = 0; k < 4; k++) {
        TILE* p = &db->fin->bar[k];
        x0 = k == 0 ? g.w / 2 - half
                    : Max(g.w / 2 - half, g.title_x + k * g.title_w / 4);
        x1 = k == 3 ? g.w / 2 + half
                    : Min(g.w / 2 + half, g.title_x + (k + 1) * g.title_w / 4);
        if (x1 <= x0)
            continue;
        setXY0(p, x0, Sy(147));
        setWH(p, x1 - x0, g.text_k);
        FadeRGB0(&p->r0, brand[k][0], brand[k][1], brand[k][2], g.flash);
        Link(&c, p);
    }
    Commit(db->ot + OT_TEXT, &c);
}

static void Tagline(DB* db, int time) {
    Chain c = {0, 0};
    int k, i, j, age, rgb[3];
    if (time <= 2400 || !g.title_n)
        return;
    Link(&c, &db->mode[MODE_TEXT]);
    for (i = 0; i < g.title_n; i++) {
        age = time - (2480 + i * g.type_step);
        if (age < 0)
            break;
        if (age < 60) {
            rgb[0] = rgb[1] = rgb[2] = 128;
        } else if (sc->title[i].accent) {
            for (j = 0; j < 3; j++)
                rgb[j] = brand[sc->title[i].accent - 1][j] >> 1;
        } else {
            rgb[0] = 98;
            rgb[1] = 95;
            rgb[2] = 113;
        }
        if (age >= 60)
            TitleWave(i, time, rgb);
        // textures modulate by col/128, so the flash goes in at half strength
        k = g.flash >> 1;
        setRGB0(&db->fin->glyph[i][0], k, k, k);
        FadeRGB0(&db->fin->glyph[i][1].r0, rgb[0], rgb[1], rgb[2], k);
        Link(&c, &db->fin->glyph[i][0]);
        Link(&c, &db->fin->glyph[i][1]);
    }
    Commit(db->ot + OT_TEXT, &c);
}

// Tube warm-up: a dot of light stretches into a line, then opens vertically.
static void CrtOn(DB* db, int time) {
    Chain c = {0, 0};
    TILE* p = db->crt;
    int hw, hh, top, bot, edge;
    if (time < 90) {
        Box(&p[0], &c, 0, 0, g.w, g.h, 0);
        if (time >= 30) {
            hw = 160 * EaseOut((time - 30) * ONE / 60) >> 12;
            Box(&p[1], &c, Sx(160 - hw), Sy(119), Max(1, Sx(hw * 2)),
                Max(1, Sy(2)), 255);
        }
    } else {
        hh = Max(16, 120 * 16 * EaseOut((time - 90) * ONE / 150) >> 12);
        top = Sy(120 * 16 - hh) >> 4;
        bot = Sy(120 * 16 + hh) >> 4;
        edge = Max(1, Sy(40) >> 4);
        if (top > 0)
            Box(&p[0], &c, 0, 0, g.w, top, 0);
        if (bot < g.h)
            Box(&p[2], &c, 0, bot, g.w, g.h - bot, 0);
        Box(&p[3], &c, 0, top, g.w, edge, 130);
        Box(&p[4], &c, 0, bot - edge, g.w, edge, 130);
    }
    Commit(db->ot + OT_CRT, &c);
}

enum {
    STEP_WARM_UP, // the CRT effect opens over the approaching star
    STEP_STAR,
    STEP_CHARGE, // the star charges up and the first flare builds
    STEP_BURST,  // shards fly out while the flare fades
    STEP_GATHER,
    STEP_LOCK,   // the wordmark locks in while the last shards land
    STEP_IMPACT, // glint, sparkles and title join the flare and shockwave
    STEP_TITLE,
};

static int StepEnd(int step) {
    switch (step) {
    case STEP_WARM_UP:
        return 240;
    case STEP_STAR:
        return 1050;
    case STEP_CHARGE:
        return T_SHATTER;
    case STEP_BURST:
        return 1900;
    case STEP_GATHER:
        return T_LOCK;
    case STEP_LOCK:
        return g.settle;
    case STEP_IMPACT:
        return T_LOCK + 800;
    default:
        return T_LENGTH;
    }
}

// The full-screen tile this replaces was flat shaded, so its colour was
// truncated to 5 bits before blending.
static void Lighting(int flash) {
    int i;
    g.flash = Clamp8(flash) & ~7;
    for (i = 0; i < 3; i++) {
        g.bg_base[i] = Fade(bg_top[i] + g.flash);
        g.bg_slope[i] = Fade(bg_bottom[i] - bg_top[i]);
    }
}

static void StarBackdrop(DB* db, int time) {
    Xf x;
    Chain c = {0, 0};
    int warm = Sat(ONE - time * ONE / 450), glow, in, rs, sxy;
    short gx, gy;

    warm = 110 * (warm * warm >> 12) >> 12;
    Lighting(warm + ((Smooth((time - 1120) * ONE / 80) >> 1) * 255 >> 12));
    SphereXf(time, &x);
    ProjectWorld(x.cx, x.cy, x.cz, &sxy);
    SxyToXY(sxy, &gx, &gy);
    glow = 1024 + (1229 * EaseOut(time * ONE / 1000) >> 12) +
           (x.charge * 2867 >> 12);
    g.zbias = (x.cz >> 2) - (OT_LEN << OT_SHIFT) / 2;
    Background(db, gx, gy, glow);
    StarField(db, time);
    in = 1434 + (x.charge * 4506 >> 12);
    rs = (x.scale * 5489 >> 12) * FOCAL * WU / x.cz * 24 / 10 >> 12;
    GlowFanDraw(db->halo, 0, &c, gx, gy, Sx(rs), Sy(rs), 120 * in >> 12,
                70 * in >> 12, 230 * in >> 12);
    Commit(db->ot + OT_HALO, &c);
}

static void LogoBackdrop(DB* db, int time, int locked) {
    Chain c = {0, 0};
    int flash = ExpNeg(MS_RATE(time - T_SHATTER, 7000));
    int glow = 2253 + (3686 * ExpNeg(MS_RATE(time - T_SHATTER, 5000)) >> 12) +
               (246 * Sin(Spin(g.clock, 5000)) >> 12);
    int in = 1229 * Ramp(time, 1600, 600) >> 12;

    if (locked) {
        flash += 1434 * ExpNeg(MS_RATE(time - T_LOCK, 9000)) >> 12;
        glow += 1434 * ExpNeg(MS_RATE(time - T_LOCK, 4000)) >> 12;
        in = 1229 + (2048 * ExpNeg(MS_RATE(time - T_LOCK, 3000)) >> 12);
    }
    Lighting(flash * 255 >> 12);
    g.zbias = 0;
    Background(db, g.w / 2, g.logo_y, glow);
    StarField(db, time);
    if (in > 41) {
        GlowFanDraw(db->halo, 0, &c, g.w / 2, g.logo_y, Sx(150), Sy(48),
                    90 * in >> 12, 60 * in >> 12, 200 * in >> 12);
        Commit(db->ot + OT_HALO, &c);
    }
}

static void Impact(DB* db, int time) {
    Flare(db, g.logo_y, 2458 * ExpNeg(MS_RATE(time - T_LOCK, 6000)) >> 12);
    Shockwave(db, g.w / 2, g.logo_y, time - T_LOCK);
}

// Everything but the ring, stars and glow has settled by T_LENGTH, so the rest
// of the scene sees a clamped time and can hold forever without overflowing.
static void Frame(DB* db, int time, int dark) {
    g.clock = time;
    time = Min(time, T_LENGTH);
    ClearOTagR(db->ot, OT_LEN);
    g.glint_n = 0;
    g.fade_mul = ONE - Sat(dark);
    while (g.step < STEP_TITLE && time >= StepEnd(g.step)) {
        if (++g.step == STEP_IMPACT) {
            InitFinale(&sc->stage.fin[0]);
            InitFinale(&sc->stage.fin[1]);
        }
    }
    UseCamera();
    addPrim(db->ot + OT_MODELS, &db->mode[MODE_MODELS]);
    addPrim(db->ot + OT_EFFECTS, &db->mode[MODE_EFFECTS]);

    // Models before Ring: both sort into the same OT slots.
    switch (g.step) {
    case STEP_WARM_UP:
        StarBackdrop(db, time);
        PsyzStar(db, time);
        Ring(db, time);
        CrtOn(db, time);
        break;
    case STEP_STAR:
        StarBackdrop(db, time);
        PsyzStar(db, time);
        Ring(db, time);
        break;
    case STEP_CHARGE:
        StarBackdrop(db, time);
        PsyzStar(db, time);
        Ring(db, time);
        Flare(db, g.shatter_y, Smooth((time - 1050) * ONE / 150) * 6 / 10);
        break;
    case STEP_BURST:
        LogoBackdrop(db, time, 0);
        Reassemble(db, time);
        Ring(db, time);
        Flare(db, g.shatter_y,
              4506 * ExpNeg(MS_RATE(time - T_SHATTER, 5000)) >> 12);
        break;
    case STEP_GATHER:
        LogoBackdrop(db, time, 0);
        Reassemble(db, time);
        Ring(db, time);
        break;
    case STEP_LOCK:
        LogoBackdrop(db, time, 1);
        Reassemble(db, time);
        Ring(db, time);
        Impact(db, time);
        break;
    case STEP_IMPACT:
        LogoBackdrop(db, time, 1);
        Letters(db, time);
        Ring(db, time);
        Impact(db, time);
        Glint(db, time);
        Sparkles(db, time);
        Separator(db, time);
        Tagline(db, time);
        break;
    case STEP_TITLE:
        LogoBackdrop(db, time, 1);
        Letters(db, time);
        Ring(db, time);
        Glint(db, time);
        Sparkles(db, time);
        Separator(db, time);
        Tagline(db, time);
        break;
    }
}

/*
 * Jingle: every timbre is synthesised into SPU RAM at start up and the score
 * plays them through ADSR, pitch, pan and reverb. Chords climb C, D, E (bVI,
 * bVII, I of E) at the approach, shatter and lock.
 */

enum { WAVE_HARMONIC, WAVE_BELL, WAVE_KICK };
enum { SMP_SINE, SMP_SAW, SMP_SOFT, SMP_SQUARE, SMP_BELL, SMP_KICK, SMP_COUNT };
enum { CUE_NOTE, CUE_SWELL };
enum {
    ENV_KICK,
    ENV_THUNK,
    ENV_THUD,
    ENV_SUB,
    ENV_PAD_IN,
    ENV_STAB,
    ENV_PAD,
    ENV_PLUCK,
    ENV_BASS,
    ENV_BASS_SWELL,
    ENV_BELL,
    ENV_GLASS,
    ENV_HOLD,
    ENV_COUNT
};

#define SPU_BASE 0x1010
#define PERIOD_MAX 224
#define BELL_ATTACK 2800 // samples, 50 loops of the FM cycle
#define KICK_ATTACK 2240
#define KICK_PERIOD 1064
#define VOICE_COUNT 24
#define REVERB_DEPTH 0x4800
#define SWELL_RANGE 6000 // cents of amplitude a swell rises through

#define F_REV 1  // send to the reverb
#define F_PAIR 2 // two voices, detuned and spread by pan

#define N(note) ((note) * 100)

// Saw partials (odd only for a square) through a resonant low-pass at
// harmonic `cut`, Q = q / 16. Bell and kick have their own synths.
typedef struct {
    u_char kind;
    u_char harm, odd;
    u_char cut, q;
    short period; // samples per looped cycle
    short cents;  // pitch of that cycle above MIDI note 0
    short blocks, loop;
} Wave;

static const Wave waves[SMP_COUNT] = {
    {WAVE_HARMONIC, 1, 0, 0, 0, 56, 7908, 2, 0},     // sine
    {WAVE_HARMONIC, 40, 0, 26, 11, 224, 5508, 8, 0}, // saw
    {WAVE_HARMONIC, 16, 0, 5, 12, 224, 5508, 8, 0},  // soft saw
    {WAVE_HARMONIC, 13, 1, 0, 0, 56, 7908, 2, 0},    // square
    {WAVE_BELL, 0, 0, 0, 0, 56, 9108, (BELL_ATTACK + 56) / 28,
     BELL_ATTACK / 28},
    {WAVE_KICK, 0, 0, 0, 0, KICK_PERIOD, 2811, (KICK_ATTACK + KICK_PERIOD) / 28,
     KICK_ATTACK / 28},
};

// in ms: linear attack, exponential decay to sustain level sl (of 15), then
// exponential sustain decrease (0 holds) and release time constants
typedef struct {
    u_short att, dec, tau, rel;
    u_char sl;
} Env;

static const Env envs[ENV_COUNT] = {
    {0, 0, 300, 120, 15},     // kick
    {0, 0, 220, 80, 15},      // thunk
    {0, 0, 140, 60, 15},      // thud
    {400, 0, 1800, 500, 15},  // sub
    {950, 0, 0, 25, 15},      // pad_in
    {40, 220, 2200, 30, 8},   // stab
    {90, 450, 3200, 900, 12}, // pad
    {1, 0, 110, 50, 15},      // pluck
    {2, 0, 85, 30, 15},       // bass
    {300, 0, 1400, 400, 15},  // bass_swell
    {0, 0, 650, 400, 15},     // bell
    {0, 0, 140, 80, 15},      // glass
    {6, 0, 0, 25, 15},        // hold
};

// c0 glides to c1 (cents) over `glide` ms, the voice keys off after `dur`
typedef struct {
    short at;
    u_char kind, smp, env, flags;
    signed char pan; // -100 to 100
    short c0, c1;
    short glide, dur;
    u_short vol; // ONE puts a hard panned voice at full volume
} Cue;

// Hits land 20 to 40 ms after their pictures: a frame reaches the screen two
// vsyncs after Jingle sees its time.
static const Cue score[] = {
    // power on: relay thunk
    {0, CUE_NOTE, SMP_KICK, ENV_THUNK, 0, 0, N(36), N(24), 350, 600, 1809},
    // approach: C add9 swells in under a square arpeggio
    {120, CUE_NOTE, SMP_SOFT, ENV_PAD_IN, F_REV | F_PAIR, 50, N(48), 0, 0, 1070,
     304},
    {120, CUE_NOTE, SMP_SOFT, ENV_PAD_IN, F_REV | F_PAIR, 50, N(55), 0, 0, 1070,
     304},
    {120, CUE_NOTE, SMP_SOFT, ENV_PAD_IN, F_REV | F_PAIR, 50, N(62), 0, 0, 1070,
     304},
    {120, CUE_NOTE, SMP_SOFT, ENV_PAD_IN, F_REV | F_PAIR, 50, N(64), 0, 0, 1070,
     304},
    {120, CUE_NOTE, SMP_SAW, ENV_PAD_IN, 0, 0, N(36), 0, 0, 1070, 1420},
    {120, CUE_NOTE, SMP_SINE, ENV_PAD_IN, 0, 0, N(24), 0, 0, 1070, 763},
    {440, CUE_NOTE, SMP_KICK, ENV_THUD, 0, 0, N(36), 0, 0, 200, 1325},
    {440, CUE_NOTE, SMP_SQUARE, ENV_PLUCK, F_REV, -40, N(67), 0, 0, 100, 728},
    {540, CUE_NOTE, SMP_SQUARE, ENV_PLUCK, F_REV, 40, N(72), 0, 0, 100, 758},
    {640, CUE_NOTE, SMP_KICK, ENV_THUD, 0, 0, N(36), 0, 0, 200, 1522},
    {640, CUE_NOTE, SMP_SQUARE, ENV_PLUCK, F_REV, -40, N(74), 0, 0, 100, 790},
    {740, CUE_NOTE, SMP_SQUARE, ENV_PLUCK, F_REV, 40, N(76), 0, 0, 100, 822},
    {840, CUE_NOTE, SMP_KICK, ENV_THUD, 0, 0, N(36), 0, 0, 200, 1747},
    {840, CUE_NOTE, SMP_SQUARE, ENV_PLUCK, F_REV, -40, N(79), 0, 0, 100, 856},
    {940, CUE_NOTE, SMP_SQUARE, ENV_PLUCK, F_REV, 40, N(84), 0, 0, 100, 891},
    {1040, CUE_NOTE, SMP_SQUARE, ENV_PLUCK, F_REV, -40, N(86), 0, 0, 100, 928},
    {1140, CUE_NOTE, SMP_SQUARE, ENV_PLUCK, F_REV, 40, N(88), 0, 0, 100, 966},
    // charge: a rising whine
    {940, CUE_SWELL, SMP_SQUARE, ENV_HOLD, F_REV, 0, N(72), N(96), 250, 250,
     1295},
    // shatter on D add9: kick, stab and flying glass
    {1240, CUE_NOTE, SMP_KICK, ENV_KICK, 0, 0, N(38), N(26), 700, 700, 2225},
    {1240, CUE_NOTE, SMP_SAW, ENV_STAB, F_REV | F_PAIR, 60, N(50), 0, 0, 950,
     510},
    {1260, CUE_NOTE, SMP_SAW, ENV_STAB, F_REV | F_PAIR, 60, N(57), 0, 0, 930,
     510},
    {1240, CUE_NOTE, SMP_SAW, ENV_STAB, F_REV | F_PAIR, 60, N(64), 0, 0, 950,
     510},
    {1260, CUE_NOTE, SMP_SAW, ENV_STAB, F_REV | F_PAIR, 60, N(66), 0, 0, 930,
     510},
    {1240, CUE_NOTE, SMP_BELL, ENV_GLASS, F_REV, -70, N(98), 0, 0, 200, 592},
    {1257, CUE_NOTE, SMP_BELL, ENV_GLASS, F_REV, 60, N(93), 0, 0, 200, 528},
    {1280, CUE_NOTE, SMP_BELL, ENV_GLASS, F_REV, -30, N(104), 0, 0, 200, 460},
    {1310, CUE_NOTE, SMP_BELL, ENV_GLASS, F_REV, 85, N(95), 0, 0, 200, 528},
    {1350, CUE_NOTE, SMP_BELL, ENV_GLASS, F_REV, -85, N(100), 0, 0, 200, 460},
    {1400, CUE_NOTE, SMP_BELL, ENV_GLASS, F_REV, 30, N(105), 0, 0, 200, 396},
    {1460, CUE_NOTE, SMP_BELL, ENV_GLASS, F_REV, -50, N(102), 0, 0, 200, 396},
    {1530, CUE_NOTE, SMP_BELL, ENV_GLASS, F_REV, 70, N(97), 0, 0, 200, 329},
    // gather: octave bass on the 100 ms grid and a rising run
    {1340, CUE_NOTE, SMP_SAW, ENV_BASS, 0, 0, N(38), 0, 0, 90, 2251},
    {1440, CUE_NOTE, SMP_SAW, ENV_BASS, 0, 0, N(50), 0, 0, 90, 1916},
    {1540, CUE_NOTE, SMP_SAW, ENV_BASS, 0, 0, N(38), 0, 0, 90, 2251},
    {1640, CUE_NOTE, SMP_SAW, ENV_BASS, 0, 0, N(50), 0, 0, 90, 1916},
    {1640, CUE_NOTE, SMP_KICK, ENV_THUD, 0, 0, N(38), 0, 0, 200, 1575},
    {1740, CUE_NOTE, SMP_SAW, ENV_BASS, 0, 0, N(38), 0, 0, 90, 2251},
    {1840, CUE_NOTE, SMP_SAW, ENV_BASS, 0, 0, N(50), 0, 0, 90, 1916},
    {1940, CUE_NOTE, SMP_SAW, ENV_BASS, 0, 0, N(38), 0, 0, 90, 2251},
    {2040, CUE_NOTE, SMP_SAW, ENV_BASS, 0, 0, N(50), 0, 0, 90, 1916},
    {2040, CUE_NOTE, SMP_KICK, ENV_THUD, 0, 0, N(38), 0, 0, 200, 1788},
    {2140, CUE_NOTE, SMP_SAW, ENV_BASS, 0, 0, N(38), 0, 0, 90, 2251},
    {1540, CUE_NOTE, SMP_BELL, ENV_GLASS, F_REV, -50, N(74), 0, 0, 180, 498},
    {1615, CUE_NOTE, SMP_BELL, ENV_GLASS, F_REV, 50, N(76), 0, 0, 180, 511},
    {1685, CUE_NOTE, SMP_BELL, ENV_GLASS, F_REV, -50, N(78), 0, 0, 180, 524},
    {1750, CUE_NOTE, SMP_BELL, ENV_GLASS, F_REV, 50, N(81), 0, 0, 180, 537},
    {1810, CUE_NOTE, SMP_BELL, ENV_GLASS, F_REV, -50, N(83), 0, 0, 180, 551},
    {1865, CUE_NOTE, SMP_BELL, ENV_GLASS, F_REV, 50, N(86), 0, 0, 180, 565},
    {1915, CUE_NOTE, SMP_BELL, ENV_GLASS, F_REV, -50, N(88), 0, 0, 180, 580},
    {1960, CUE_NOTE, SMP_BELL, ENV_GLASS, F_REV, 50, N(90), 0, 0, 180, 595},
    {2000, CUE_NOTE, SMP_BELL, ENV_GLASS, F_REV, -50, N(93), 0, 0, 180, 610},
    {2040, CUE_NOTE, SMP_BELL, ENV_GLASS, F_REV, 50, N(95), 0, 0, 180, 626},
    {2075, CUE_NOTE, SMP_BELL, ENV_GLASS, F_REV, -50, N(98), 0, 0, 180, 642},
    {2110, CUE_NOTE, SMP_BELL, ENV_GLASS, F_REV, 50, N(100), 0, 0, 180, 658},
    // lock on E add9: the biggest hit, then bells ring out
    {2240, CUE_NOTE, SMP_KICK, ENV_KICK, 0, 0, N(40), N(28), 900, 1700, 2357},
    {2240, CUE_NOTE, SMP_SAW, ENV_PAD, F_REV | F_PAIR, 60, N(52), 0, 0, 2800,
     642},
    {2240, CUE_NOTE, SMP_SAW, ENV_PAD, F_REV | F_PAIR, 60, N(59), 0, 0, 2800,
     642},
    {2240, CUE_NOTE, SMP_SAW, ENV_PAD, F_REV | F_PAIR, 60, N(66), 0, 0, 2800,
     642},
    {2240, CUE_NOTE, SMP_SAW, ENV_PAD, F_REV | F_PAIR, 60, N(68), 0, 0, 2800,
     642},
    {2240, CUE_NOTE, SMP_SAW, ENV_BASS_SWELL, 0, 0, N(40), 0, 0, 2400, 1539},
    {2240, CUE_NOTE, SMP_SINE, ENV_SUB, 0, 0, N(28), 0, 0, 2800, 763},
    {2240, CUE_NOTE, SMP_BELL, ENV_BELL, F_REV, -30, N(88), 0, 0, 1000, 938},
    {2240, CUE_NOTE, SMP_BELL, ENV_BELL, F_REV, 30, N(95), 0, 0, 1000, 688},
    {2580, CUE_NOTE, SMP_BELL, ENV_BELL, F_REV, -80, N(83), 0, 0, 1000, 657},
    {2860, CUE_NOTE, SMP_BELL, ENV_BELL, F_REV, -5, N(88), 0, 0, 1000, 704},
    {3080, CUE_NOTE, SMP_BELL, ENV_BELL, F_REV, 90, N(90), 0, 0, 1000, 763},
    {3300, CUE_NOTE, SMP_BELL, ENV_BELL, F_REV, 45, N(92), 0, 0, 1000, 817},
    {3480, CUE_NOTE, SMP_BELL, ENV_BELL, F_REV, 5, N(95), 0, 0, 1000, 886},
};

#define CUE_COUNT ((int)(sizeof(score) / sizeof(score[0])))

static const short semitone[13] = {4096, 4340, 4598, 4871, 5161, 5468, 5793,
                                   6137, 6502, 6889, 7298, 7732, 8192};

static struct {
    u_long addr[SMP_COUNT];
    u_long next;
    int last; // time of the previous call, cues after it are due
    int key_on, key_off, rev_on, rev_off; // voice masks sent once per frame
    u_short adsr[ENV_COUNT][2];
    struct {
        const Cue* cue;
        int start, off, free;
        short cents, pan;
    } voice[VOICE_COUNT];
} au;

static int Pow2Cents(int c) {
    int oct = c >= 0 ? c / 1200 : -((-c + 1199) / 1200), s, v;
    c -= oct * 1200;
    s = c / 100;
    v = semitone[s] + (semitone[s + 1] - semitone[s]) * (c % 100) / 100;
    if (oct >= 0)
        return v << oct;
    return oct < -30 ? 0 : v >> -oct;
}

// One looped period, normalised to a 28672 peak.
static void Harmonic(const Wave* w, short* pcm) {
    short tab[PERIOD_MAX];
    int acc[PERIOD_MAX];
    int s, i, j, x, a, b, amp, peak = 1, sh = 0;
    for (i = 0; i < w->period; i++) {
        tab[i] = Sin(i * ONE / w->period);
        acc[i] = 0;
    }
    for (s = 1; s <= w->harm; s += w->odd ? 2 : 1) {
        amp = ONE / s;
        if (w->cut) {
            x = s * 64 / w->cut;
            a = 64 - x * x / 64;
            b = x * 16 / w->q;
            amp = amp * 64 / Max(1, SquareRoot0(a * a + b * b));
        }
        for (i = 0, j = 0; i < w->period; i++) {
            acc[i] += amp * tab[j];
            j += s;
            if (j >= w->period)
                j -= w->period;
        }
    }
    for (i = 0; i < w->period; i++)
        peak = Max(peak, Abs(acc[i]));
    while (peak >> sh > 32767)
        sh++;
    for (i = 0; i < w->period; i++)
        pcm[i] = (acc[i] >> sh) * 28672 / (peak >> sh);
}

// FM at 1:3.5, so the loop holds two carrier cycles; the strike's extra index
// and octave partial fade out by the end of the attack.
static int Bell(int i) {
    int k = Min(i, BELL_ATTACK), e, dev;
    e = ExpNeg(k * ONE / 530) * (BELL_ATTACK - k) / BELL_ATTACK;
    dev = ((2867 + (7782 * e >> 12)) * Sin(i * 512) >> 12) * 652 >> 12;
    return (Sin(i * 1024 / 7 + dev) +
            ((492 + (942 * e >> 12)) * Sin(i * 2048 / 7) >> 12)) *
           5;
}

// A sine falling from 5.5 times its tail pitch, settled exactly as the loop
// starts, driven into a soft clip so small speakers still get its harmonics.
// It fades in from a burst of noise, the beater.
static int Kick(int i) {
    int k = Min(i, KICK_ATTACK), e = ExpNeg(k * ONE / 530), ea = 60, s, s3;
    int t = (530 * (ONE - e) - k * ea) / (ONE - ea);
    s = Sin((i + t * 9 / 2) * ONE / KICK_PERIOD) * 3 / 2;
    s = s < -ONE ? -ONE : s > ONE ? ONE : s;
    s3 = (s * s >> 12) * s >> 12;
    s = (3 * s - s3) >> 1;
    if (i < 176)
        s = (s * i + (Rand() - 2048) * 2 * (176 - i)) / 176;
    return s * 7;
}

// SPU ADPCM: 28 samples in 16 bytes. The prediction filter with the smallest
// residual sets the shift, then the block is quantised once against the
// history the decoder will have.
static void EncodeBlock(
    const short* pcm, u_char* out, int flags, int* h1, int* h2) {
    static const signed char f0[5] = {0, 60, 115, 98, 122};
    static const signed char f1[5] = {0, 0, -52, -55, -60};
    int f, k, sh, bits, half, maxr, p1, p2, pred, q, y, best = 0x7FFFFFFF;
    int bf = 0;
    for (f = 0; f < 5; f++) {
        p1 = *h1;
        p2 = *h2;
        for (maxr = 0, k = 0; k < 28; k++) {
            pred = (p1 * f0[f] >> 6) + (p2 * f1[f] >> 6);
            maxr = Max(maxr, Abs(pcm[k] - pred));
            p2 = p1;
            p1 = pcm[k];
        }
        if (maxr < best) {
            best = maxr;
            bf = f;
        }
    }
    for (sh = 12; sh > 0 && (7 << (12 - sh)) < best; sh--)
        ;
    bits = 12 - sh;
    half = bits ? 1 << (bits - 1) : 0;
    out[0] = (bf << 4) | sh;
    out[1] = flags;
    p1 = *h1;
    p2 = *h2;
    for (k = 0; k < 28; k++) {
        pred = (p1 * f0[bf] >> 6) + (p2 * f1[bf] >> 6);
        q = pcm[k] - pred;
        q = q >= 0 ? (q + half) >> bits : -((-q + half) >> bits);
        q = q < -8 ? -8 : q > 7 ? 7 : q;
        y = (q << bits) + pred;
        y = y < -32768 ? -32768 : y > 32767 ? 32767 : y;
        if (k & 1)
            out[2 + (k >> 1)] |= (q & 15) << 4;
        else
            out[2 + (k >> 1)] = q & 15;
        p2 = p1;
        p1 = y;
    }
    *h1 = p1;
    *h2 = p2;
}

static void BlockPcm(const Wave* w, const short* period, int b, short* pcm) {
    int k, s;
    for (k = 0; k < 28; k++) {
        s = b * 28 + k;
        s = w->kind == WAVE_HARMONIC ? period[s % w->period]
            : w->kind == WAVE_BELL   ? Bell(s)
                                     : Kick(s);
        pcm[k] = s < -32768 ? -32768 : s > 32767 ? 32767 : s;
    }
}

// The decoder wraps into the loop with the loop's own last two samples as
// history, so the loop is encoded from there. A dry pass over it finds them.
static void LoopHistory(const Wave* w, const short* period, int* h1, int* h2) {
    u_char out[16];
    short pcm[28];
    int b;
    *h1 = *h2 = 0;
    for (b = w->loop; b < w->blocks; b++) {
        BlockPcm(w, period, b, pcm);
        EncodeBlock(pcm, out, 0, h1, h2);
    }
}

// SPU DMA moves 64-byte chunks, so every upload is padded to four blocks.
static void UploadSample(int kind) {
    static u_char buf[32 * 16];
    const Wave* w = &waves[kind];
    short period[PERIOD_MAX], pcm[28];
    int b, k, n = 0, h1 = 0, h2 = 0, l1 = 0, l2 = 0, flags;
    au.addr[kind] = au.next;
    if (w->kind == WAVE_HARMONIC)
        Harmonic(w, period);
    LoopHistory(w, period, &l1, &l2);
    for (b = 0; b < w->blocks; b++) {
        flags = (b == w->loop ? 4 : 0) | (b == w->blocks - 1 ? 3 : 0);
        if (b == w->loop) {
            h1 = l1;
            h2 = l2;
        }
        BlockPcm(w, period, b, pcm);
        EncodeBlock(pcm, buf + n * 16, flags, &h1, &h2);
        if (++n == 32 || b == w->blocks - 1) {
            for (; n & 3; n++)
                for (k = 0; k < 16; k++)
                    buf[n * 16 + k] = 0;
            SpuSetTransferStartAddr(au.next);
            SpuWrite(buf, n * 16);
            SpuIsTransferCompleted(SPU_TRANSFER_WAIT);
            au.next += n * 16;
            n = 0;
        }
    }
}

static int AdsrRate(int us, int decrease) {
    int r, shift, step, time, best = 0, best_err = 0x7FFFFFFF;
    for (r = 0; r < 128; r++) {
        shift = r >> 2;
        step = (decrease ? 8 : 7) - (r & 3);
        if (shift < 11)
            time = 743039 / (step << (11 - shift));
        else if (shift > 21)
            break;
        else
            time = 743039 / step << (shift - 11);
        if (Abs(time - us) < best_err) {
            best_err = Abs(time - us);
            best = r;
        }
    }
    return best;
}

// Without a decay phase the decay rate is the slowest: the first decay step
// still runs and rate 0 would halve the level.
static void EnvAdsr(const Env* e, u_short* adsr) {
    int sr = e->tau ? AdsrRate(e->tau * 1000, 1) : 127;
    int dr = e->dec ? Min((AdsrRate(e->dec * 1000, 1) + 2) >> 2, 15) : 15;
    int rr = Min((AdsrRate(e->rel * 1000, 1) + 2) >> 2, 31);
    adsr[0] = (AdsrRate(e->att * 1000, 0) << 8) | (dr << 4) | e->sl;
    adsr[1] = (e->tau ? 0xC000 : 0) | (sr << 6) | 0x20 | rr;
}

static void VoiceAttr(
    SpuVoiceAttr* a, int v, const Cue* c, int cents, int gain, int pan) {
    int pitch = Pow2Cents(cents - waves[c->smp].cents);
    int l = gain * Cos((pan + ONE) >> 3) >> 12;
    int r = gain * Sin((pan + ONE) >> 3) >> 12;
    a->voice = 1 << v;
    a->mask |= SPU_VOICE_PITCH | SPU_VOICE_VOLL | SPU_VOICE_VOLR;
    a->pitch = pitch < 1 ? 1 : pitch > 0x3FFF ? 0x3FFF : pitch;
    a->volume.left = Min(l, ONE) * 0x3FFF >> 12;
    a->volume.right = Min(r, ONE) * 0x3FFF >> 12;
}

// A voice that has finished its release, else the one keyed off earliest.
static int VoiceAlloc(int time) {
    int v, best = 0;
    for (v = 0; v < VOICE_COUNT; v++) {
        if (au.voice[v].free <= time)
            return v;
        if (au.voice[v].off < au.voice[best].off)
            best = v;
    }
    return best;
}

static void VoiceStart(
    int v, const Cue* c, int time, int cents, int pan, int gain) {
    SpuVoiceAttr a;
    au.voice[v].cue = c;
    au.voice[v].start = time;
    au.voice[v].off = time + c->dur;
    au.voice[v].free = au.voice[v].off + envs[c->env].rel * 3;
    au.voice[v].cents = cents;
    au.voice[v].pan = pan;
    a.mask = SPU_VOICE_WDSA | SPU_VOICE_ADSR_ADSR1 | SPU_VOICE_ADSR_ADSR2;
    a.addr = au.addr[c->smp];
    a.adsr1 = au.adsr[c->env][0];
    a.adsr2 = au.adsr[c->env][1];
    VoiceAttr(&a, v, c, cents, gain, pan);
    SpuSetVoiceAttr(&a);
    au.key_on |= 1 << v;
    if (c->flags & F_REV)
        au.rev_on |= 1 << v;
    else
        au.rev_off |= 1 << v;
}

static int Swell(int range, int u) {
    return Pow2Cents(-(range * (ONE - Sat(u)) >> 12));
}

static int CueGain(const Cue* c, int age) {
    if (c->kind == CUE_SWELL)
        return c->vol * Swell(SWELL_RANGE, age * ONE / c->glide) >> 12;
    return c->vol;
}

static void Trigger(const Cue* c, int time) {
    int pan = c->pan * ONE / 100;
    if (c->flags & F_PAIR) {
        VoiceStart(VoiceAlloc(time), c, time, c->c0 - 8, -pan, CueGain(c, 0));
        VoiceStart(VoiceAlloc(time), c, time, c->c0 + 8, pan, CueGain(c, 0));
    } else {
        VoiceStart(VoiceAlloc(time), c, time, c->c0, pan, CueGain(c, 0));
    }
}

static void VoiceUpdate(int v, int time) {
    SpuVoiceAttr a;
    const Cue* c = au.voice[v].cue;
    int age = time - au.voice[v].start, cents = au.voice[v].cents;
    if (c->kind == CUE_NOTE && (!c->c1 || age > c->glide + 100))
        return;
    if (c->c1 && c->glide)
        cents += (c->c1 - c->c0) * Sat(age * ONE / c->glide) >> 12;
    a.mask = 0;
    VoiceAttr(&a, v, c, cents, CueGain(c, age), au.voice[v].pan);
    SpuSetVoiceAttr(&a);
}

static void JingleInit(void) {
    SpuCommonAttr common;
    SpuReverbAttr rev;
    int i;

    SpuInit();
#ifdef __psyz
    Psyz_AudioInit();
#endif
    SpuSetTransferMode(SPU_TRANSFER_BY_DMA);
    au.next = SPU_BASE;
    rng = 0x2545F491;
    for (i = 0; i < SMP_COUNT; i++)
        UploadSample(i);
    for (i = 0; i < ENV_COUNT; i++)
        EnvAdsr(&envs[i], au.adsr[i]);

    common.mask = SPU_COMMON_MVOLL | SPU_COMMON_MVOLR;
    common.mvol.left = common.mvol.right = 0x3FFF;
    SpuSetCommonAttr(&common);
    rev.mask = SPU_REV_MODE;
    rev.mode = SPU_REV_MODE_HALL | SPU_REV_MODE_CLEAR_WA;
    SpuSetReverbModeParam(&rev);
    rev.mask = SPU_REV_DEPTHL | SPU_REV_DEPTHR;
    rev.depth.left = rev.depth.right = REVERB_DEPTH;
    SpuSetReverbDepth(&rev);
    SpuSetReverb(SPU_ON);
    au.last = -1;
    for (i = 0; i < VOICE_COUNT; i++) {
        au.voice[i].cue = 0;
        au.voice[i].off = au.voice[i].free = 0;
    }
}

static void Jingle(int time, int volume) {
    SpuCommonAttr common;
    const Cue* c;
    int v;
    au.key_on = au.key_off = au.rev_on = au.rev_off = 0;
    for (c = score; c < score + CUE_COUNT; c++)
        if (c->at > au.last && c->at <= time)
            Trigger(c, time);
    au.last = time;
    for (v = 0; v < VOICE_COUNT; v++) {
        if (!au.voice[v].cue)
            continue;
        if (time >= au.voice[v].off) {
            au.key_off |= 1 << v;
            au.voice[v].cue = 0;
        } else if (au.voice[v].start < time) {
            VoiceUpdate(v, time);
        }
    }
    if (au.rev_on)
        SpuSetReverbVoice(SPU_ON, au.rev_on);
    if (au.rev_off)
        SpuSetReverbVoice(SPU_OFF, au.rev_off);
    if (au.key_off & ~au.key_on)
        SpuSetKey(SPU_OFF, au.key_off & ~au.key_on);
    if (au.key_on)
        SpuSetKey(SPU_ON, au.key_on);
    common.mask = SPU_COMMON_MVOLL | SPU_COMMON_MVOLR;
    common.mvol.left = common.mvol.right = 0x3FFF * volume >> 12;
    SpuSetCommonAttr(&common);
}

static void JingleStop(void) {
    SpuCommonAttr common;
    SpuSetKey(SPU_OFF, SPU_ALLCH);
    SpuSetReverbVoice(SPU_OFF, SPU_ALLCH);
    SpuSetReverb(SPU_OFF);
    common.mask = SPU_COMMON_MVOLL | SPU_COMMON_MVOLR;
    common.mvol.left = common.mvol.right = 0;
    SpuSetCommonAttr(&common);
}

static void ClearScreen(void) {
    RECT rc;
    setRECT(&rc, 0, 0, g.w, g.h > 256 ? g.h : g.h * 2);
    ClearImage(&rc, 0, 0, 0);
    DrawSync(0);
}

static void InitVideo(int w, int h) {
    int i, j, inter = h > 256, sxy;
    for (i = 0; i <= 1024; i++)
        sc->sin_q[i] = rsin(i);
    ResetGraph(0);
    SetGraphDebug(0);
    InitGeom();
    SetGeomOffset(w / 2, h / 2);
    SetGeomScreen(FOCAL * (h * ONE / LAYOUT_H) >> 12);
    g.w = w;
    g.h = h;
    g.sx = w * ONE / LAYOUT_W;
    g.sy = h * ONE / LAYOUT_H;
    g.inv_sx = (1 << 24) / g.sx;
    g.inv_sy = (1 << 24) / g.sy;
    g.bg_inv_h = (ONE << 8) / h;
    g.text_k = h > 256 ? 2 : 1;
    for (i = 0; i < 2; i++) {
        int draw_y = inter ? 0 : i * h, disp_y = inter ? 0 : (1 - i) * h;
        SetDefDrawEnv(&sc->dbuf[i].draw, 0, draw_y, w, h);
        SetDefDispEnv(&sc->dbuf[i].disp, 0, disp_y, w, h);
        sc->dbuf[i].draw.isbg = 0;
        sc->dbuf[i].draw.dtd = 1;
        sc->dbuf[i].disp.isinter = inter;
        sc->dbuf[i].fin = &sc->stage.fin[i];
        for (j = 0; j < 5; j++) {
            setTile(&sc->dbuf[i].crt[j]);
            setSemiTrans(&sc->dbuf[i].crt[j], j >= 3);
        }
    }
    g.dfe = sc->dbuf[0].draw.dfe;
    ClearScreen();

    // isotropic scaling lives in the focal length, only x keeps a row scale
    g.cam_x = g.sx * ONE / g.sy;
    g.cam.m[0][0] = g.cam_x;
    g.cam.m[1][1] = ONE;
    g.cam.m[2][2] = ONE;
    Rot(&g.ring, RAD(-300), 0, RAD(260));
    UseCamera();
    ProjectWorld(0, -29, 11 * WU, &sxy);
    g.logo_y = (short)(sxy >> 16);
}

// Holds the tube's first line on screen while the scene and jingle get built.
static void ShowBeam(void) {
    DB* db = &sc->dbuf[0];
    ClearOTagR(db->ot, OT_LEN);
    CrtOn(db, 89);
    PutDrawEnv(&db->draw);
    DrawOTag(db->ot + OT_LEN - 1);
    DrawSync(0);
    VSync(0);
    PutDispEnv(&sc->dbuf[1].disp);
    SetDispMask(1);
}

static void InitScene(void) {
    static const EggState egg_rest;
    int sxy;
    rng = 1997;
    g.egg = egg_rest;
    g.shards = 0;
    g.settle = 0;
    g.step = STEP_WARM_UP;
    BuildStar();
    BuildLetters();
    BuildShards();
    BuildStars();
    BuildTitle();
    UseCamera();
    ProjectWorld(g.sc[0], g.sc[1], g.sc[2], &sxy);
    g.shatter_y = (short)(sxy >> 16);
    InitPrims(&sc->dbuf[0]);
    InitPrims(&sc->dbuf[1]);
}

// Adds the push to a velocity that friction keeps bounded, returns this frame's
// travel. k is 16 at 60 Hz and grows as the refresh rate drops.
static int Glide(int* v, int push, int k) {
    *v = (*v + push) * 7 / 8;
    return *v * k >> 8;
}

static int WrapAngle(int a) { return ((a + ONE / 2) & (ONE - 1)) - ONE / 2; }

static void Egg(int pad, int time, int hz) {
    int k = 960 / hz;
    int right = !!(pad & PADLright) - !!(pad & PADLleft);
    int up = !!(pad & PADLup) - !!(pad & PADLdown);
    int r1 = !!(pad & PADR1) - !!(pad & PADL1);
    int r2 = !!(pad & PADR2) - !!(pad & PADL2);
    int warp = (pad & (PADL1 | PADR1)) == (PADL1 | PADR1);
    int yaw = Glide(&g.egg.yaw_v, -78 * r1, k);
    int pitch = Glide(&g.egg.pitch_v, -78 * r2, k);

    g.egg.ring_yaw += Glide(&g.egg.ring_yaw_v, -40 * right, k);
    g.egg.ring_pitch += Glide(&g.egg.ring_pitch_v, 40 * up, k);
    Rot(&g.ring, RAD(-300) + g.egg.ring_pitch, g.egg.ring_yaw, RAD(260));
    if (time < T_SHATTER) {
        g.egg.star_yaw = WrapAngle(g.egg.star_yaw + yaw);
        g.egg.star_pitch = WrapAngle(g.egg.star_pitch + pitch);
    } else {
        g.egg.logo_yaw = WrapAngle(g.egg.logo_yaw + yaw);
        g.egg.logo_pitch = WrapAngle(g.egg.logo_pitch + pitch);
    }
    g.egg.warp = (g.egg.warp + Glide(&g.egg.warp_v, 146 * warp, k)) % STAR_SPAN;
}

static int PadSkips(PsyzBootlogoConfig config, int pad) {
    if (config.skip_cb) {
        return 0;
    }
    if (pad & (PADstart | PADselect))
        return 1;
    if (!config.frame_step) {
        if (pad & (PADRup | PADRdown | PADRleft | PADRright)) {
            return 1;
        }
    }
    return 0;
}

int Psyz_Bootlogo(PsyzBootlogoConfig config) {
    int w = config.width ? config.width : LAYOUT_W;
    int h = config.height > 256 ? 480 : LAYOUT_H;
    int jingle = !config.jingle_off;
    int hz, start, time, frame, stepped = 0, pad, held = 0, pad_was_idle = 0;
    int skipped = -1, dark, ds, end = T_MAX_DEFAULT, released = 0, ticks = 0,
        vs;
    DB* db;
    Scene scene;

    sc = &scene;
    if (config.time_max)
        end = config.time_max;
    if (end < 4000)
        end = 4000;
    w = w < 256 ? 256 : w > 640 ? 640 : w;
    InitVideo(w, h);
    ShowBeam();
    InitScene();
    PadInit(0);
    if (jingle)
        JingleInit();

    hz = GetVideoMode() == MODE_PAL ? 50 : 60;
    VSync(0);
    start = VSync(-1);
    for (frame = 0;; frame++) {
        pad = PadRead(0);
        pad = (pad | pad >> 16) & 0xFFFF;
        if (!pad)
            pad_was_idle = 1;
        // buttons held since before the logo started do not count
        if (!pad_was_idle)
            pad = 0;
        if (config.frame_step) {
            if ((pad & ~held & (PADLup | PADLdown | PADLleft | PADLright)) ||
                (pad & (PADRup | PADRdown | PADRleft | PADRright)))
                stepped++;
            time = stepped * 1000 / hz;
        } else {
            // PsyZ's VSync(-1) counter is 16-bit
            vs = VSync(-1);
            ticks += (vs - start) & 0xFFFF;
            start = vs;
            time = ticks * 1000 / hz;
        }
        if (skipped < 0 && PadSkips(config, pad)) {
            skipped = time;
            released = 1;
            // the clock is paused, so the fade would never finish
            if (config.frame_step)
                break;
        }
        if (!config.no_easter_egg) {
            // frame_step owns the D-Pad
            if (config.frame_step)
                Egg(pad & ~(PADLup | PADLdown | PADLleft | PADLright), time,
                    hz);
            else
                Egg(pad, time, hz);
        }
        held = pad;
        if (config.skip_cb && !released) {
            if (config.skip_cb(time)) {
                released = 1;
                if (time < end - T_FADE)
                    skipped = time;
            } else {
                end = Max(end, time + T_FADE);
            }
        }
        dark = Smooth((time - (end - T_FADE)) * ONE / T_FADE);
        if (skipped >= 0) {
            ds = Ramp(time, skipped, T_SKIP_FADE);
            dark = ONE - ((ONE - dark) * (ONE - ds) >> 12);
            if (ds >= ONE)
                break;
        }
        if (time >= end)
            break;
        db = &sc->dbuf[frame & 1];
        Frame(db, time, dark);
        if (jingle)
            Jingle(time, ONE - dark);
        DrawSync(0);
        VSync(0);
        PutDispEnv(&db->disp);
        PutDrawEnv(&db->draw);
        DrawOTag(db->ot + OT_LEN - 1);
#ifdef __psyz
        if (Psyz_QuitRequested())
            break;
#endif
    }

    DrawSync(0);
    if (jingle)
        JingleStop();
    ClearScreen();
    VSync(0);
    SetDispMask(0);
    PadStop();
    return skipped >= 0;
}
