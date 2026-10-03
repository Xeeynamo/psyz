#include "ztest.h"
#include <psyz.h>
#include <kernel.h>
#include <libgte.h>
#include <psyz/gte.h>
#include <libgpu.h>
#include <string.h>

#ifdef __psyz
#define GTE_SET_ZSF3(v) Psyz_GteCtrlWrite(29, (unsigned int)(v))
#define GTE_SET_ZSF4(v) Psyz_GteCtrlWrite(30, (unsigned int)(v))
#define GTE_SET_DQA(v) Psyz_GteCtrlWrite(27, (unsigned int)(v))
#define GTE_SET_DQB(v) Psyz_GteCtrlWrite(28, (unsigned int)(v))
#define GTE_READ_IR0(v) ((v) = (int)Psyz_GteDataRead(8))
#else
#define GTE_SET_ZSF3(v) __asm__ volatile("ctc2	%0, $29" : : "r"(v))
#define GTE_SET_ZSF4(v) __asm__ volatile("ctc2	%0, $30" : : "r"(v))
#define GTE_SET_DQA(v) __asm__ volatile("ctc2	%0, $27" : : "r"(v))
#define GTE_SET_DQB(v) __asm__ volatile("ctc2	%0, $28" : : "r"(v))
#define GTE_READ_IR0(v) __asm__ volatile("mfc2	%0, $8;nop" : "=r"(v))
#endif

ZTEST_SETUP(gte) { InitGeom(); }
ZTEST_TEARDOWN(gte) { InitGeom(); }

#define zexpect_matrix_eq(exp, act) zexpect_matrix_eq_(__LINE__, exp, act)
static int zexpect_matrix_eq_(int line, MATRIX* exp, MATRIX* act) {
    int i, j, eq = 1;
    for (i = 0; i < 3; i++) {
        eq &= exp->t[i] == act->t[i];
        for (j = 0; j < 3; j++) {
            eq &= exp->m[i][j] == act->m[i][j];
        }
    }
    if (eq) {
        return 1;
    }
    ztest__fail(line, 0, "Matrix mismatch");
    zprintf("  Expected:%21sActual:\n", "");
    for (i = 0; i < 3; i++) {
        zprintf("  %04X %04X %04X  %08X      %04X %04X %04X  %08X\n",
                (unsigned short)exp->m[i][0], (unsigned short)exp->m[i][1],
                (unsigned short)exp->m[i][2], (unsigned)exp->t[i],
                (unsigned short)act->m[i][0], (unsigned short)act->m[i][1],
                (unsigned short)act->m[i][2], (unsigned)act->t[i]);
    }
    return 0;
}

typedef struct {
    MATRIX m;
    SVECTOR svs[3];
    int lgs[3];
    int p;
    int flag;
} RTPContext;

static void RTP_Init(RTPContext* ctx) {
    RTPContext zero = {0};
    *ctx = zero;
    InitGeom();
    SetGeomOffset(0, 0);
    SetRotMatrix(&ctx->m);
    SetTransMatrix(&ctx->m);
}

static long RTP_RotTransPers(RTPContext* ctx) {
    return RotTransPers(&ctx->svs[0], &ctx->lgs[0], &ctx->p, &ctx->flag);
}

static long RTP_RotTransPers3(RTPContext* ctx) {
    return RotTransPers3(&ctx->svs[0], &ctx->svs[1], &ctx->svs[2], &ctx->lgs[0],
                         &ctx->lgs[1], &ctx->lgs[2], &ctx->p, &ctx->flag);
}

static void RTP_SetTransM(RTPContext* ctx, long tx, long ty, long tz) {
    ctx->m.t[0] = tx;
    ctx->m.t[1] = ty;
    ctx->m.t[2] = tz;
    SetTransMatrix(&ctx->m);
}

static void RTP_SetRotM(
    RTPContext* ctx, short m00, short m01, short m02, short m10, short m11,
    short m12, short m20, short m21, short m22) {
    ctx->m.m[0][0] = m00;
    ctx->m.m[0][1] = m01;
    ctx->m.m[0][2] = m02;
    ctx->m.m[1][0] = m10;
    ctx->m.m[1][1] = m11;
    ctx->m.m[1][2] = m12;
    ctx->m.m[2][0] = m20;
    ctx->m.m[2][1] = m21;
    ctx->m.m[2][2] = m22;
    SetRotMatrix(&ctx->m);
}

static void RTP_SetSvs(RTPContext* ctx, short tx, short ty, short tz) {
    ctx->svs[0].vy = tx;
    ctx->svs[1].vy = ty;
    ctx->svs[2].vz = tz;
}

static void CheckRTP_(
    RTPContext* ctx, unsigned int lgs0, long p_exp, unsigned int flag_exp) {
    zexpect_s16_eq((short)lgs0, (short)ctx->lgs[0]);
    zexpect_s16_eq((short)(lgs0 >> 16), (short)(ctx->lgs[0] >> 16));
    zexpect_s32_eq(p_exp, ctx->p);
    zexpect_u32_eq(flag_exp, (unsigned int)ctx->flag);
}

static void CheckRTP3_(RTPContext* ctx, unsigned int lgs0, unsigned int lgs1,
                       unsigned int lgs2, long p_exp, unsigned int flag_exp) {
    zexpect_u32_eq(lgs0, (unsigned int)ctx->lgs[0]);
    zexpect_u32_eq(lgs1, (unsigned int)ctx->lgs[1]);
    zexpect_u32_eq(lgs2, (unsigned int)ctx->lgs[2]);
    zexpect_s32_eq(p_exp, ctx->p);
    zexpect_u32_eq(flag_exp, (unsigned int)ctx->flag);
}

static void TestRTP_(RTPContext* ctx, long ret_exp, unsigned int lgs0,
                     long p_exp, unsigned int flag_exp, int line) {
    zprintf("called from line %d\n", line);
    zexpect_s32_eq(ret_exp, RTP_RotTransPers(ctx));
    CheckRTP_(ctx, lgs0, p_exp, flag_exp);
}

static void TestRTP3_(
    RTPContext* ctx, long ret_exp, unsigned int lgs0, unsigned int lgs1,
    unsigned int lgs2, long p_exp, unsigned int flag_exp, int line) {
    zprintf("called from line %d\n", line);
    zexpect_s32_eq(ret_exp, RTP_RotTransPers3(ctx));
    CheckRTP3_(ctx, lgs0, lgs1, lgs2, p_exp, flag_exp);
}

#define SXY(x, y)                                                              \
    (((unsigned int)(x) & 0xFFFF) | (((unsigned int)(y) & 0xFFFF) << 16))
#define MV(x, y, z) x, y, z
#define CheckRTP3(ctx, lgs0, lgs1, lgs2, p_exp, flag_exp)                      \
    (zprintf("called from line %d\n", __LINE__),                               \
     CheckRTP3_(ctx, lgs0, lgs1, lgs2, p_exp, flag_exp))
#define TestRTP(ctx, ret_exp, lgs0, p_exp, flag_exp)                           \
    TestRTP_(ctx, ret_exp, lgs0, p_exp, flag_exp, __LINE__)
#define TestRTP3(ctx, ret_exp, lgs0, lgs1, lgs2, p_exp, flag_exp)              \
    TestRTP3_(ctx, ret_exp, lgs0, lgs1, lgs2, p_exp, flag_exp, __LINE__)

ZTEST(gte, rsin) {
    zexpect_s32_eq(0x0000, rsin(0x0000));
    zexpect_s32_eq(0x0006, rsin(0x0001));
    zexpect_s32_eq(0x000D, rsin(0x0002));
    zexpect_s32_eq(0x0065, rsin(0x0010));
    zexpect_s32_eq(0x061F, rsin(0x0100));
    zexpect_s32_eq(0x0B50, rsin(0x0200));
    zexpect_s32_eq(0x1000, rsin(0x0400));
    zexpect_s32_eq(0x0000, rsin(0x0800));
    zexpect_s32_eq(0x0000, rsin(0x1000));
}

ZTEST(gte, trans_matrix) {
    MATRIX m = {{{0, 1, 2}, {3, 4, 5}, {6, 7, 8}}, {9, 10, 11}};
    MATRIX exp = {{{0, 1, 2}, {3, 4, 5}, {6, 7, 8}}, {16, 17, 18}};
    VECTOR t = {16, 17, 18};
    zexpect_ptr_eq(&m, TransMatrix(&m, &t));
    zexpect_matrix_eq(&exp, &m);
}

ZTEST(gte, rot_matrix) {
    MATRIX m = {{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}, {10, 11, 12}};
    MATRIX exp = {{{+0x0FFD, -0x0071, +0x006B},
                   {+0x0073, +0x0FFC, -0x0065},
                   {-0x0069, +0x0067, +0x0FFE}},
                  {10, 11, 12}};
    SVECTOR sv = {16, 17, 18};
    zexpect_ptr_eq(&m, RotMatrix(&sv, &m));
    zexpect_matrix_eq(&exp, &m);
}

ZTEST(gte, trans_matrix_negative) {
    MATRIX m = {{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}, {0, 0, 0}};
    VECTOR t = {-1, -2000, 30000};
    zexpect_ptr_eq(&m, TransMatrix(&m, &t));
    zexpect_s32_eq(-1, m.t[0]);
    zexpect_s32_eq(-2000, m.t[1]);
    zexpect_s32_eq(30000, m.t[2]);
    zexpect_s16_eq(1, m.m[0][0]);
    zexpect_s16_eq(9, m.m[2][2]);
}

ZTEST(gte, scale_matrix_identity) {
    MATRIX m = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}, {7, 8, 9}};
    MATRIX exp = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}, {7, 8, 9}};
    VECTOR s = {0x1000, 0x1000, 0x1000};
    zexpect_ptr_eq(&m, ScaleMatrix(&m, &s));
    zexpect_matrix_eq(&exp, &m);
}

ZTEST(gte, scale_matrix_per_axis) {
    MATRIX m = {{{0x1000, 0x1000, 0x1000},
                 {0x1000, 0x1000, 0x1000},
                 {0x1000, 0x1000, 0x1000}},
                {0, 0, 0}};
    VECTOR s = {0x800, 0x2000, 0};
    zexpect_ptr_eq(&m, ScaleMatrix(&m, &s));
    for (int i = 0; i < 3; i++) {
        if (!zexpect_s16_eq(0x800, m.m[i][0])) {
            zprintf("row %d\n", i);
        }
        if (!zexpect_s16_eq(0x2000, m.m[i][1])) {
            zprintf("row %d\n", i);
        }
        if (!zexpect_s16_eq(0, m.m[i][2])) {
            zprintf("row %d\n", i);
        }
    }
}

ZTEST(gte, scale_matrix_negative) {
    MATRIX m = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}, {0, 0, 0}};
    VECTOR s = {-0x1000, -0x800, 0x1000};
    ScaleMatrix(&m, &s);
    zexpect_s16_eq(-0x1000, m.m[0][0]);
    zexpect_s16_eq(-0x800, m.m[1][1]);
    zexpect_s16_eq(0x1000, m.m[2][2]);
}

ZTEST(gte, mul_matrix_identity) {
    MATRIX a = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}, {0, 0, 0}};
    MATRIX b = {{{0x0100, 0x0200, 0x0300},
                 {0x0400, 0x0500, 0x0600},
                 {0x0700, 0x0800, 0x0900}},
                {0, 0, 0}};
    MATRIX exp = b;
    zexpect_ptr_eq(&a, MulMatrix(&a, &b));
    zexpect_matrix_eq(&exp, &a);
}

ZTEST(gte, mul_matrix_half_identity) {
    MATRIX a = {{{0x0800, 0, 0}, {0, 0x0800, 0}, {0, 0, 0x0800}}, {0, 0, 0}};
    MATRIX b = {{{0x1000, 0x2000, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}},
                {0, 0, 0}};
    MATRIX exp = {{{0x0800, 0x1000, 0}, {0, 0x0800, 0}, {0, 0, 0x0800}},
                  {0, 0, 0}};
    MulMatrix(&a, &b);
    zexpect_matrix_eq(&exp, &a);
}

ZTEST(gte, mul_matrix_permutation) {
    MATRIX a = {{{0, 0x1000, 0}, {0x1000, 0, 0}, {0, 0, 0x1000}}, {0, 0, 0}};
    MATRIX b = {{{0x0111, 0x0222, 0x0333},
                 {0x0444, 0x0555, 0x0666},
                 {0x0777, 0x0888, 0x0999}},
                {0, 0, 0}};
    MATRIX exp = {{{0x0444, 0x0555, 0x0666},
                   {0x0111, 0x0222, 0x0333},
                   {0x0777, 0x0888, 0x0999}},
                  {0, 0, 0}};
    MulMatrix(&a, &b);
    zexpect_matrix_eq(&exp, &a);
}

ZTEST(gte, comp_matrix_rotates_and_translates) {
    MATRIX m0 = {{{0, -0x1000, 0}, {0x1000, 0, 0}, {0, 0, 0x1000}},
                 {100, 200, 300}};
    MATRIX m1 = {{{0x1000, 0, 0}, {0, 0x0800, 0}, {0, 0, 0x1000}},
                 {10, 20, 30}};
    MATRIX exp = {{{0, -0x0800, 0}, {0x1000, 0, 0}, {0, 0, 0x1000}},
                  {80, 210, 330}};
    MATRIX m2;
    zexpect_ptr_eq(&m2, CompMatrix(&m0, &m1, &m2));
    zexpect_matrix_eq(&exp, &m2);
}

ZTEST(gte, comp_matrix_keeps_low_16_bits_of_m1_translation) {
    MATRIX m0 = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}, {0, 0, 0}};
    MATRIX m1 = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}},
                 {0x12345, -0x10002, 0x8000}};
    MATRIX exp = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}},
                  {0x2345, -2, -0x8000}};
    MATRIX m2;
    CompMatrix(&m0, &m1, &m2);
    zexpect_matrix_eq(&exp, &m2);
}

ZTEST(gte, comp_matrix_saturates_rotation) {
    MATRIX m0 = {{{0x7FFF, 0x7FFF, 0}, {-0x8000, -0x8000, 0}, {0, 0, 0x1000}},
                 {0, 0, 0}};
    MATRIX m1 = {{{0x7FFF, 0, 0}, {0x7FFF, 0, 0}, {0, 0, 0x1000}}, {0, 0, 0}};
    MATRIX exp = {{{0x7FFF, 0, 0}, {-0x8000, 0, 0}, {0, 0, 0x1000}}, {0, 0, 0}};
    MATRIX m2;
    CompMatrix(&m0, &m1, &m2);
    zexpect_matrix_eq(&exp, &m2);
}

ZTEST(gte, transpose_matrix) {
    MATRIX m = {{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}, {10, 11, 12}};
    MATRIX out = {0};
    // returns the destination, and only the 3x3 part is written
    zexpect_ptr_eq(&out, TransposeMatrix(&m, &out));
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            if (!zexpect_s16_eq(m.m[j][i], out.m[i][j])) {
                zprintf("at [%d][%d]\n", i, j);
            }
        }
    }
}

ZTEST(gte, transpose_matrix_twice_is_identity) {
    MATRIX m = {{{-1, 2, -3}, {4, -5, 6}, {-7, 8, -9}}, {0, 0, 0}};
    MATRIX once = {0};
    MATRIX twice = {0};
    TransposeMatrix(&m, &once);
    TransposeMatrix(&once, &twice);
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            if (!zexpect_s16_eq(m.m[i][j], twice.m[i][j])) {
                zprintf("at [%d][%d]\n", i, j);
            }
        }
    }
}

ZTEST(gte, transpose_matrix_symmetric_unchanged) {
    MATRIX m = {{{1, 2, 3}, {2, 4, 5}, {3, 5, 6}}, {0, 0, 0}};
    MATRIX out = {0};
    TransposeMatrix(&m, &out);
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            zexpect_s16_eq(m.m[i][j], out.m[i][j]);
        }
    }
}

ZTEST(gte, square_root_0) {
    zexpect_s32_eq(0, SquareRoot0(0));
    zexpect_s32_eq(1, SquareRoot0(1));
    zexpect_s32_eq(1, SquareRoot0(2));
    zexpect_s32_eq(2, SquareRoot0(4));
    zexpect_s32_eq(2, SquareRoot0(8));
    zexpect_s32_eq(3, SquareRoot0(9));
    zexpect_s32_eq(0x100, SquareRoot0(0x10000));
    zexpect_s32_eq(0x4000, SquareRoot0(0x10000000));
}

ZTEST(gte, square_root_12) {
    zexpect_s32_eq(0, SquareRoot12(0));
    zexpect_s32_eq(0x40, SquareRoot12(1));
    zexpect_s32_eq(0x5A, SquareRoot12(2));
    zexpect_s32_eq(0x80, SquareRoot12(4));
    zexpect_s32_eq(0xB5, SquareRoot12(8));
    zexpect_s32_eq(0xC0, SquareRoot12(9));
    zexpect_s32_eq(0x4000, SquareRoot12(0x10000));
    zexpect_s32_eq(0x100000, SquareRoot12(0x10000000));
}

// Some GTE tests are ports from the PCSX Redux regression suite
// https://github.com/grumpycoders/pcsx-redux/tree/main/src/mips/tests/gte

ZTEST(gte, avsz3_uses_sz123) {
    zexpect_s32_eq(499, AverageZ3(1000, 2000, 3000));
}

ZTEST(gte, avsz4_basic) {
    zexpect_s32_eq(0xA00, AverageZ4(0x1000, 0x2000, 0x3000, 0x4000));
}

ZTEST(gte, avsz4_stotz_macros) {
    long expected = AverageZ4(0x100, 0x200, 0x300, 0x400);
    unsigned int otz;

    gte_avsz4();
    gte_stotz(&otz);

    zexpect_u32_eq(expected, otz);
}

ZTEST(gte, nclip_ccw) {
    zexpect_s32_eq(10000, NormalClip(0x00000000, 0x00000064, 0x00640000));
}

ZTEST(gte, nclip_cw) {
    zexpect_s32_eq(-10000, NormalClip(0x00000000, 0x00640000, 0x00000064));
}

ZTEST(gte, nclip_collinear) {
    zexpect_s32_eq(0, NormalClip(0x00000000, 0x00320032, 0x00640064));
}

ZTEST(gte, nclip_large_coords) {
    zexpect_s32_eq(-2047, NormalClip(0xFC0003FF, 0x03FFFC00, 0x00000000));
}

ZTEST(gte, nclip_overflow) {
    zexpect_s32_eq(-131071, NormalClip(0x7FFF7FFF, 0x7FFF8000, 0x80007FFF));
}

ZTEST(gte, rtps_identity_center) {
    RTPContext ctx;
    RTP_Init(&ctx);
    RTP_SetRotM(&ctx, 0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000);
    RTP_SetTransM(&ctx, 0, 0, 1000);
    SetGeomOffset(160, 120);
    SetGeomScreen(200);
    ctx.svs[0].vx = 0;
    ctx.svs[0].vy = 0;
    ctx.svs[0].vz = 0;
    long sz = RTP_RotTransPers(&ctx);
    zexpect_s32_eq(1000 >> 2, sz);
    zexpect_s16_eq(160, (short)ctx.lgs[0]);
    zexpect_s16_eq(120, (short)(ctx.lgs[0] >> 16));
}

ZTEST(gte, rtps_offset_vertex) {
    RTPContext ctx;
    RTP_Init(&ctx);
    RTP_SetRotM(&ctx, 0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000);
    RTP_SetTransM(&ctx, 0, 0, 0);
    SetGeomOffset(160, 120);
    SetGeomScreen(200);
    ctx.svs[0].vx = 100;
    ctx.svs[0].vy = 50;
    ctx.svs[0].vz = 500;
    RTP_RotTransPers(&ctx);
    zexpect_s16_eq(199, (short)ctx.lgs[0]);
    zexpect_s16_eq(139, (short)(ctx.lgs[0] >> 16));
}

ZTEST(gte, rtps_division_overflow) {
    RTPContext ctx;
    RTP_Init(&ctx);
    RTP_SetRotM(&ctx, 0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000);
    RTP_SetTransM(&ctx, 0, 0, 0);
    SetGeomOffset(0, 0);
    SetGeomScreen(200);
    ctx.svs[0].vx = 100;
    ctx.svs[0].vy = 0;
    ctx.svs[0].vz = 1;
    RTP_RotTransPers(&ctx);
    zexpect_u32_eq(1u, (ctx.flag >> 17) & 1u);
}

ZTEST(gte, rtps_screen_saturation) {
    RTPContext ctx;
    RTP_Init(&ctx);
    RTP_SetRotM(&ctx, 0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000);
    RTP_SetTransM(&ctx, 0, 0, 0);
    SetGeomOffset(0, 0);
    SetGeomScreen(200);
    ctx.svs[0].vx = 0x7FFF;
    ctx.svs[0].vy = 0;
    ctx.svs[0].vz = 100;
    RTP_RotTransPers(&ctx);
    zexpect_s16_eq(0x3FF, (short)ctx.lgs[0]);
    zexpect_u32_eq(1u, (ctx.flag >> 14) & 1u);
}

ZTEST(gte, rtpt_three_vertices) {
    RTPContext ctx;
    RTP_Init(&ctx);
    RTP_SetRotM(&ctx, 0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000);
    RTP_SetTransM(&ctx, 0, 0, 0);
    SetGeomOffset(160, 120);
    SetGeomScreen(200);
    ctx.svs[0].vx = 0;
    ctx.svs[0].vy = 0;
    ctx.svs[0].vz = 1000;
    ctx.svs[1].vx = 100;
    ctx.svs[1].vy = 0;
    ctx.svs[1].vz = 1000;
    ctx.svs[2].vx = 0;
    ctx.svs[2].vy = 100;
    ctx.svs[2].vz = 1000;
    RTP_RotTransPers3(&ctx);
    zexpect_s16_eq(160, (short)ctx.lgs[0]);
    zexpect_s16_eq(120, (short)(ctx.lgs[0] >> 16));
    zexpect_s16_eq(120, (short)(ctx.lgs[1] >> 16));
}

ZTEST(gte, rtpt_sz_fifo) {
    RTPContext ctx;
    RTP_Init(&ctx);
    RTP_SetRotM(&ctx, 0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000);
    RTP_SetTransM(&ctx, 0, 0, 0);
    SetGeomOffset(160, 120);
    SetGeomScreen(200);
    ctx.svs[0].vx = 0;
    ctx.svs[0].vy = 0;
    ctx.svs[0].vz = 100;
    ctx.svs[1].vx = 0;
    ctx.svs[1].vy = 0;
    ctx.svs[1].vz = 200;
    ctx.svs[2].vx = 0;
    ctx.svs[2].vy = 0;
    ctx.svs[2].vz = 300;
    long sz = RTP_RotTransPers3(&ctx);
    zexpect_s32_eq(300 >> 2, sz);
}

ZTEST(gte, rot_trans_pers_trans_matrix) {
    RTPContext ctx;
    RTP_Init(&ctx);

    SetGeomOffset(100, 100);
    RTP_SetTransM(&ctx, 0, 0, 0);
    TestRTP(&ctx, 0, SXY(100, 100), 0, 0x80021000);
    SetGeomOffset(0, 0);

    RTP_SetTransM(&ctx, 0, 0, 0);
    TestRTP(&ctx, 0, SXY(0, 0), 0, 0x80021000);

    RTP_SetTransM(&ctx, 10, 20, 0);
    TestRTP(&ctx, 0, SXY(19, 39), 0, 0x80021000);

    RTP_SetTransM(&ctx, -10, -20, 0);
    TestRTP(&ctx, 0, SXY(-20, -40), 0, 0x80021000);

    // TODO SXY is clipped at abs(0x3FF) but there are no tests for that
}

ZTEST(gte, rot_trans_pers_rot_matrix) {
    RTPContext ctx;
    RTP_Init(&ctx);
    SetGeomOffset(0, 0);

    RTP_SetRotM(&ctx, MV(0, 0, 0), MV(0, 0, 0), MV(0, 0, 0));
    TestRTP(&ctx, 0, SXY(0, 0), 0, 0x80021000);

    RTP_SetRotM(&ctx, MV(0x100, 0, 0), MV(0, 0x200, 0), MV(0, 0, 0x300));
    TestRTP(&ctx, 0, SXY(0, 0), 0, 0x80021000);
}

ZTEST(gte, rot_trans_pers3_trans_matrix_perspective) {
    RTPContext ctx;
    RTP_Init(&ctx);

    RTP_SetTransM(&ctx, 0, 0, 0x40);
    TestRTP3(&ctx, 0x0010, 0, 0, 0, 0, 0x80021000);

    RTP_SetTransM(&ctx, 0, 0, -4);
    TestRTP3(&ctx, 0x0000, 0, 0, 0, 0, 0x80061000);

    RTP_SetTransM(&ctx, 0, 0, 0x1A36);
    TestRTP3(&ctx, 0x068D, 0, 0, 0, 0, 0x1000);
}

ZTEST(gte, rot_trans_pers3_set_geom_offset) {
    RTPContext ctx;
    RTP_Init(&ctx);

    SetGeomOffset(1, 3);
    TestRTP3(&ctx, 0, 0x00030001, 0x00030001, 0x00030001, 0, 0x80021000);

    SetGeomOffset(0, 0);
    TestRTP3(&ctx, 0, 0x00000000, 0x00000000, 0x00000000, 0, 0x80021000);

    SetGeomOffset(0x3FF, 0x3FF);
    TestRTP3(&ctx, 0, 0x03FF03FF, 0x03FF03FF, 0x03FF03FF, 0, 0x80021000);

    SetGeomOffset(0x500, 0x100);
    TestRTP3(&ctx, 0, 0x010003FF, 0x010003FF, 0x010003FF, 0, 0x80025000);

    SetGeomOffset(0x100, 0x500);
    TestRTP3(&ctx, 0, 0x03FF0100, 0x03FF0100, 0x03FF0100, 0, 0x80023000);

    SetGeomOffset(0x600, 0x700);
    TestRTP3(&ctx, 0, 0x03FF03FF, 0x03FF03FF, 0x03FF03FF, 0, 0x80027000);

    SetGeomOffset(160, 120);
    TestRTP3(&ctx, 0, 0x007800A0, 0x007800A0, 0x007800A0, 0, 0x80021000);
}

ZTEST(gte, rot_trans_pers3_trans_and_offset) {
    RTPContext ctx;
    RTP_Init(&ctx);

    SetGeomOffset(0, 0);
    RTP_SetTransM(&ctx, 10, 20, 0x100);
    TestRTP3(&ctx, 0x40, 0x00270013, 0x00270013, 0x00270013, 0, 0x80021000);

    SetGeomOffset(-31, -63);
    RTP_SetTransM(&ctx, 0x10, 0x20, 0x100);
    TestRTP3(&ctx, 0x40, 0x00000000, 0x00000000, 0x00000000, 0, 0x80021000);

    SetGeomOffset(100, 120);
    RTP_SetTransM(&ctx, 50, 60, 0x100);
    TestRTP3(&ctx, 0x40, 0x00EF00C7, 0x00EF00C7, 0x00EF00C7, 0, 0x80021000);

    SetGeomOffset(0x500, 0x400);
    RTP_SetTransM(&ctx, 100, 200, 0x100);
    TestRTP3(&ctx, 0x40, 0x03FF03FF, 0x03FF03FF, 0x03FF03FF, 0, 0x80027000);
}

ZTEST(gte, rot_trans_pers3_rot_matrix) {
    RTPContext ctx;
    RTP_Init(&ctx);

    RTP_SetRotM(&ctx, 0x100, 0x100, 0x100, 0, 0x200, 0x200, 0x200, 0, 0xC00);
    RTP_SetSvs(&ctx, 0x10, 0x10, 0x10);
    TestRTP3(&ctx, 0x03, 0x00030001, 0x00030001, 0x00030001, 0, 0x80021000);

    RTP_SetRotM(&ctx, 0x100, 0x400, 0x100, 0, 0x200, 0x200, 0x200, 0, 0xC00);
    RTP_SetSvs(&ctx, 0x10, 0x10, 0x10);
    TestRTP3(&ctx, 0x03, 0x00030007, 0x00030007, 0x00030001, 0, 0x80021000);

    RTP_SetRotM(&ctx, 0x100, 0x100, 0x400, 0, 0x200, 0x200, 0x200, 0, 0xC00);
    RTP_SetSvs(&ctx, 0x10, 0x10, 0x10);
    TestRTP3(&ctx, 0x03, 0x00030001, 0x00030001, 0x00030007, 0, 0x80021000);

    RTP_SetRotM(&ctx, 0x100, 0x100, 0x100, 0, 0x400, 0x200, 0x200, 0, 0xC00);
    RTP_SetSvs(&ctx, 0x10, 0x10, 0x10);
    TestRTP3(&ctx, 0x03, 0x00070001, 0x00070001, 0x00030001, 0, 0x80021000);

    RTP_SetRotM(&ctx, 0x100, 0x100, 0x100, 0, 0x200, 0x400, 0x200, 0, 0xC00);
    RTP_SetSvs(&ctx, 0x10, 0x10, 0x10);
    TestRTP3(&ctx, 0x03, 0x00030001, 0x00030001, 0x00070001, 0, 0x80021000);

    RTP_SetRotM(
        &ctx, 0x100, 0x100, 0x100, 0xFFF, 0x200, 0x200, 0xFFF, 0xFFF, 0xC00);
    RTP_SetSvs(&ctx, 0x10, 0x10, 0x10);
    TestRTP3(&ctx, 0x03, 0x00030001, 0x00030001, 0x00030001, 0, 0x80021000);

    RTP_SetRotM(&ctx, -0x100, -0x200, -0x400, 0, -0x800, -0x1000, 0, 0, 0xC00);
    RTP_SetSvs(&ctx, -0x10, -0x10, -0x10);
    TestRTP3(&ctx, 0, 0x000F0003, 0x000F0003, 0x001F0007, 0, 0x80061000);

    RTP_SetRotM(&ctx, 0, 0, 0, 0, 0, 0, 0, 0, 0xC00);
    RTP_SetSvs(&ctx, 0, 0, -1);
    TestRTP3(&ctx, 0, 0, 0, 0, 0, 0x80061000);
}

ZTEST(gte, rot_trans_pers_set_geom_offset) {
    RTPContext ctx;
    RTP_Init(&ctx);

    SetGeomOffset(1, 3);
    TestRTP(&ctx, 0, 0x00030001, 0, 0x80021000);

    SetGeomOffset(0, 0);
    TestRTP(&ctx, 0, 0x00000000, 0, 0x80021000);

    SetGeomOffset(0x3FF, 0x3FF);
    TestRTP(&ctx, 0, 0x03FF03FF, 0, 0x80021000);

    SetGeomOffset(0x500, 0x100);
    TestRTP(&ctx, 0, 0x010003FF, 0, 0x80025000);

    SetGeomOffset(0x100, 0x500);
    TestRTP(&ctx, 0, 0x03FF0100, 0, 0x80023000);

    SetGeomOffset(0x600, 0x700);
    TestRTP(&ctx, 0, 0x03FF03FF, 0, 0x80027000);

    SetGeomOffset(160, 120);
    TestRTP(&ctx, 0, 0x007800A0, 0, 0x80021000);
}

ZTEST(gte, rot_trans_pers_trans_and_offset) {
    RTPContext ctx;
    RTP_Init(&ctx);

    SetGeomOffset(0, 0);
    RTP_SetTransM(&ctx, 10, 20, 0x100);
    TestRTP(&ctx, 0x40, 0x00270013, 0, 0x80021000);

    SetGeomOffset(-31, -63);
    RTP_SetTransM(&ctx, 0x10, 0x20, 0x100);
    TestRTP(&ctx, 0x40, 0x00000000, 0, 0x80021000);

    SetGeomOffset(100, 120);
    RTP_SetTransM(&ctx, 50, 60, 0x100);
    TestRTP(&ctx, 0x40, 0x00EF00C7, 0, 0x80021000);

    SetGeomOffset(0x500, 0x400);
    RTP_SetTransM(&ctx, 100, 200, 0x100);
    TestRTP(&ctx, 0x40, 0x03FF03FF, 0, 0x80027000);
}

ZTEST(gte, rcos_rsin) {
    zexpect_s32_eq(0x1000, rcos(0x0000));
    zexpect_s32_eq(0x0000, rcos(0x0400));
    zexpect_s32_eq(-0x1000, rcos(0x0800));
    zexpect_s32_eq(0x1000, rsin(0x0400));
    zexpect_s32_eq(rcos(0), rcos(0x1000));
    zexpect_s32_eq(rsin(0), rsin(0x1000));
}

ZTEST(gte, rot_matrix_x) {
    MATRIX m = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}, {0, 0, 0}};
    zexpect_ptr_eq(&m, RotMatrixX(0x400, &m));
    zexpect_s16_eq(0x1000, m.m[0][0]);
    zexpect_s16_eq(0, m.m[1][1]);
    zexpect_s16_eq(-0x1000, m.m[1][2]);
    zexpect_s16_eq(0x1000, m.m[2][1]);
    zexpect_s16_eq(0, m.m[2][2]);
}

ZTEST(gte, rot_matrix_y) {
    MATRIX m = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}, {0, 0, 0}};
    zexpect_ptr_eq(&m, RotMatrixY(0x400, &m));
    zexpect_s16_eq(0, m.m[0][0]);
    zexpect_s16_eq(0x1000, m.m[0][2]);
    zexpect_s16_eq(0x1000, m.m[1][1]);
    zexpect_s16_eq(-0x1000, m.m[2][0]);
    zexpect_s16_eq(0, m.m[2][2]);
}

ZTEST(gte, rot_matrix_z) {
    MATRIX m = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}, {0, 0, 0}};
    zexpect_ptr_eq(&m, RotMatrixZ(0x400, &m));
    zexpect_s16_eq(0, m.m[0][0]);
    zexpect_s16_eq(-0x1000, m.m[0][1]);
    zexpect_s16_eq(0x1000, m.m[1][0]);
    zexpect_s16_eq(0, m.m[1][1]);
    zexpect_s16_eq(0x1000, m.m[2][2]);
}

ZTEST(gte, rot_matrix_arbitrary_angles) {
    MATRIX m = {{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}, {10, 11, 12}};
    MATRIX exp = {{{+0x0212, +0x0061, +0x0FDC},
                   {-0x041C, -0x0F71, +0x00E8},
                   {+0x0F52, -0x0431, -0x01E7}},
                  {10, 11, 12}};
    SVECTOR sv = {0x123, 0x456, 0x789};
    zexpect_ptr_eq(&m, RotMatrix(&sv, &m));
    zexpect_matrix_eq(&exp, &m);
}

ZTEST(gte, rot_matrix_negative_and_wrapped_angles) {
    MATRIX m = {{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}, {10, 11, 12}};
    MATRIX exp = {{{-0x0541, +0x0588, -0x0E10},
                   {+0x0D03, -0x05EB, -0x0730},
                   {-0x07B1, -0x0DCC, -0x0290}},
                  {10, 11, 12}};
    SVECTOR sv = {-0x321, 0x0ABC, -0x1DEF};
    zexpect_ptr_eq(&m, RotMatrix(&sv, &m));
    zexpect_matrix_eq(&exp, &m);
}

ZTEST(gte, rot_matrix_yxz_arbitrary_angles) {
    MATRIX m = {{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}, {10, 11, 12}};
    MATRIX exp = {{{+0x0350, -0x0659, +0x0E4E},
                   {+0x029F, -0x0E32, -0x06E8},
                   {+0x0F6E, +0x03C6, -0x01E7}},
                  {10, 11, 12}};
    SVECTOR sv = {0x123, 0x456, 0x789};
    zexpect_ptr_eq(&m, RotMatrixYXZ(&sv, &m));
    zexpect_matrix_eq(&exp, &m);
}

ZTEST(gte, rot_matrix_yxz_negative_and_wrapped_angles) {
    MATRIX m = {{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}, {10, 11, 12}};
    MATRIX exp = {{{+0x045A, +0x0EA7, -0x04B8},
                   {+0x03E4, +0x03B1, +0x0F13},
                   {+0x0EE4, -0x0542, -0x0290}},
                  {10, 11, 12}};
    SVECTOR sv = {-0x321, 0x0ABC, -0x1DEF};
    zexpect_ptr_eq(&m, RotMatrixYXZ(&sv, &m));
    zexpect_matrix_eq(&exp, &m);
}

ZTEST(gte, rot_matrix_yxz_axis_only) {
    MATRIX m = {0};
    SVECTOR sv = {0x400, 0, 0};
    RotMatrixYXZ(&sv, &m);
    zexpect_s16_eq(-0x1000, m.m[1][2]);
    zexpect_s16_eq(0, m.m[0][2]);
    zexpect_s16_eq(0, m.m[2][2]);
    zexpect_s16_eq(0x1000, m.m[0][0]);
}

ZTEST(gte, rot_matrix_zyx_arbitrary_angles) {
    MATRIX m = {{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}, {10, 11, 12}};
    MATRIX exp = {{{+0x0212, -0x095A, -0x0CD1},
                   {-0x0062, -0x0CF4, +0x0964},
                   {-0x0FDC, -0x00E9, -0x01E7}},
                  {10, 11, 12}};
    SVECTOR sv = {0x123, 0x456, 0x789};
    zexpect_ptr_eq(&m, RotMatrixZYX(&sv, &m));
    zexpect_matrix_eq(&exp, &m);
}

ZTEST(gte, rot_matrix_zyx_negative_and_wrapped_angles) {
    MATRIX m = {{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}, {10, 11, 12}};
    MATRIX exp = {{{-0x0541, +0x053A, -0x0E30},
                   {-0x0589, +0x0D4C, +0x06F4},
                   {+0x0E10, +0x072F, -0x0290}},
                  {10, 11, 12}};
    SVECTOR sv = {-0x321, 0x0ABC, -0x1DEF};
    zexpect_ptr_eq(&m, RotMatrixZYX(&sv, &m));
    zexpect_matrix_eq(&exp, &m);
}

ZTEST(gte, rot_matrix_zyx_axis_only) {
    MATRIX m = {0};
    SVECTOR sv = {0, 0x400, 0};
    RotMatrixZYX(&sv, &m);
    zexpect_s16_eq(0x1000, m.m[0][2]);
    zexpect_s16_eq(0x1000, m.m[1][1]);
    zexpect_s16_eq(-0x1000, m.m[2][0]);
    zexpect_s16_eq(0, m.m[0][0]);
    zexpect_s16_eq(0, m.m[2][2]);
}

ZTEST(gte, rot_matrix_x_arbitrary_angle) {
    MATRIX m = {{{+0x0F00, -0x0234, +0x0123},
                 {+0x0456, +0x0E12, -0x0789},
                 {-0x0321, +0x0654, +0x0FA0}},
                {7, 8, 9}};
    MATRIX exp = {{{+0x0F00, -0x0234, +0x0123},
                   {+0x0543, +0x09F6, -0x0D8B},
                   {-0x00F4, +0x0BC8, +0x0AD7}},
                  {7, 8, 9}};
    zexpect_ptr_eq(&m, RotMatrixX(0x123, &m));
    zexpect_matrix_eq(&exp, &m);
}

ZTEST(gte, rot_matrix_x_negative_angle) {
    MATRIX m = {{{+0x0F00, -0x0234, +0x0123},
                 {+0x0456, +0x0E12, -0x0789},
                 {-0x0321, +0x0654, +0x0FA0}},
                {7, 8, 9}};
    MATRIX exp = {{{+0x0F00, -0x0234, +0x0123},
                   {-0x04D6, -0x0CB0, +0x0A3F},
                   {+0x024A, -0x08C8, -0x0E00}},
                  {7, 8, 9}};
    zexpect_ptr_eq(&m, RotMatrixX(-0x789, &m));
    zexpect_matrix_eq(&exp, &m);
}

ZTEST(gte, rot_matrix_y_arbitrary_angle) {
    MATRIX m = {{{+0x0F00, -0x0234, +0x0123},
                 {+0x0456, +0x0E12, -0x0789},
                 {-0x0321, +0x0654, +0x0FA0}},
                {7, 8, 9}};
    MATRIX exp = {{{+0x0C2E, +0x00BE, +0x07C5},
                   {+0x0456, +0x0E12, -0x0789},
                   {-0x094D, +0x06A8, +0x0D9A}},
                  {7, 8, 9}};
    zexpect_ptr_eq(&m, RotMatrixY(0x123, &m));
    zexpect_matrix_eq(&exp, &m);
}

ZTEST(gte, rot_matrix_y_negative_angle) {
    MATRIX m = {{{+0x0F00, -0x0234, +0x0123},
                 {+0x0456, +0x0E12, -0x0789},
                 {-0x0321, +0x0654, +0x0FA0}},
                {7, 8, 9}};
    MATRIX exp = {{{-0x0E2F, +0x0104, -0x03F5},
                   {+0x0456, +0x0E12, -0x0789},
                   {+0x05CD, -0x06A0, -0x0F29}},
                  {7, 8, 9}};
    zexpect_ptr_eq(&m, RotMatrixY(-0x789, &m));
    zexpect_matrix_eq(&exp, &m);
}

ZTEST(gte, rot_matrix_z_arbitrary_angle) {
    MATRIX m = {{{+0x0F00, -0x0234, +0x0123},
                 {+0x0456, +0x0E12, -0x0789},
                 {-0x0321, +0x0654, +0x0FA0}},
                {7, 8, 9}};
    MATRIX exp = {{{+0x0BA8, -0x0810, +0x0447},
                   {+0x0A62, +0x0BBD, -0x064F},
                   {-0x0321, +0x0654, +0x0FA0}},
                  {7, 8, 9}};
    zexpect_ptr_eq(&m, RotMatrixZ(0x123, &m));
    zexpect_matrix_eq(&exp, &m);
}

ZTEST(gte, rot_matrix_z_negative_angle) {
    MATRIX m = {{{+0x0F00, -0x0234, +0x0123},
                 {+0x0456, +0x0E12, -0x0789},
                 {-0x0321, +0x0654, +0x0FA0}},
                {7, 8, 9}};
    MATRIX exp = {{{-0x0DF7, +0x04B8, -0x027D},
                   {-0x06FE, -0x0D70, +0x0734},
                   {-0x0321, +0x0654, +0x0FA0}},
                  {7, 8, 9}};
    zexpect_ptr_eq(&m, RotMatrixZ(-0x789, &m));
    zexpect_matrix_eq(&exp, &m);
}

ZTEST(gte, rot_matrix_angle_wraps_full_turn) {
    SVECTOR base = {0x123, 0x456, 0x789};
    SVECTOR wrapped = {0x123 + 0x1000, 0x456 - 0x1000, 0x789 + 0x2000};
    MATRIX a = {0}, b = {0};
    RotMatrix(&base, &a);
    RotMatrix(&wrapped, &b);
    zexpect_matrix_eq(&b, &a);
    MATRIX c = {0}, d = {0};
    RotMatrixYXZ(&base, &c);
    RotMatrixYXZ(&wrapped, &d);
    zexpect_matrix_eq(&d, &c);
}

ZTEST(gte, rot_matrix_zero) {
    MATRIX m = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}, {0, 0, 0}};
    MATRIX exp = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}, {0, 0, 0}};
    RotMatrixX(0, &m);
    RotMatrixY(0, &m);
    RotMatrixZ(0, &m);
    zexpect_matrix_eq(&exp, &m);
}

ZTEST(gte, apply_matrix_identity) {
    MATRIX m = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}, {0, 0, 0}};
    SVECTOR in = {100, -200, 300};
    VECTOR out = {0};
    ApplyMatrix(&m, &in, &out);
    zexpect_s32_eq(100, out.vx);
    zexpect_s32_eq(-200, out.vy);
    zexpect_s32_eq(300, out.vz);
}

ZTEST(gte, apply_matrix_permutation) {
    MATRIX m = {{{0, 0x1000, 0}, {0, 0, 0x1000}, {0x1000, 0, 0}}, {0, 0, 0}};
    SVECTOR in = {1, 2, 3};
    VECTOR out = {0};
    ApplyMatrix(&m, &in, &out);
    zexpect_s32_eq(2, out.vx);
    zexpect_s32_eq(3, out.vy);
    zexpect_s32_eq(1, out.vz);
}

ZTEST(gte, apply_matrix_ignores_translation) {
    MATRIX m = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}},
                {1000, 2000, 3000}};
    SVECTOR in = {5, 6, 7};
    VECTOR out = {0};
    ApplyMatrix(&m, &in, &out);
    zexpect_s32_eq(5, out.vx);
    zexpect_s32_eq(6, out.vy);
    zexpect_s32_eq(7, out.vz);
}

ZTEST(gte, rot_trans_applies_translation) {
    MATRIX m = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}, {10, 20, 30}};
    SVECTOR in = {1, 2, 3};
    VECTOR out = {0};
    int flag = 0;
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    RotTrans(&in, &out, &flag);
    zexpect_s32_eq(11, out.vx);
    zexpect_s32_eq(22, out.vy);
    zexpect_s32_eq(33, out.vz);
}

ZTEST(gte, outer_product_0) {
    VECTOR a = {1, 2, 3};
    VECTOR b = {4, 5, 6};
    VECTOR out = {0};
    OuterProduct0(&a, &b, &out);
    zexpect_s32_eq(-3, out.vx);
    zexpect_s32_eq(6, out.vy);
    zexpect_s32_eq(-3, out.vz);
}

ZTEST(gte, outer_product_0_large) {
    VECTOR a = {-0x8000, 0x7FFF, -0x8000};
    VECTOR b = {0x7FFF, -0x8000, 0x7FFF};
    VECTOR out = {0};
    OuterProduct0(&a, &b, &out);
    zexpect_s32_eq(-0xFFFF, out.vx);
    zexpect_s32_eq(0, out.vy);
    zexpect_s32_eq(0xFFFF, out.vz);
}

ZTEST(gte, outer_product_0_truncates_inputs_to_s16) {
    VECTOR a = {0x10001, 0x20002, 0x30003};
    VECTOR b = {0x40004, 0x50005, 0xFFFF0006};
    VECTOR out = {0};
    OuterProduct0(&a, &b, &out);
    zexpect_s32_eq(-3, out.vx);
    zexpect_s32_eq(6, out.vy);
    zexpect_s32_eq(-3, out.vz);
}

ZTEST(gte, outer_product_12) {
    VECTOR a = {0x1000, 0, 0};
    VECTOR b = {0, 0x1000, 0};
    VECTOR out = {0};
    OuterProduct12(&a, &b, &out);
    zexpect_s32_eq(0, out.vx);
    zexpect_s32_eq(0, out.vy);
    zexpect_s32_eq(0x1000, out.vz);
}

ZTEST(gte, outer_product_12_rounds_toward_negative_infinity) {
    VECTOR a = {1, 0, 0};
    VECTOR b = {0, -1, 1};
    VECTOR out = {0};
    OuterProduct12(&a, &b, &out);
    zexpect_s32_eq(0, out.vx);
    zexpect_s32_eq(-1, out.vy);
    zexpect_s32_eq(-1, out.vz);
}

ZTEST(gte, outer_product_12_mixed_signs) {
    VECTOR a = {0x0800, -0x1800, 0x2000};
    VECTOR b = {-0x3000, 0x0400, 0x1000};
    VECTOR out = {0};
    OuterProduct12(&a, &b, &out);
    zexpect_s32_eq(-0x2000, out.vx);
    zexpect_s32_eq(-0x6800, out.vy);
    zexpect_s32_eq(-0x4600, out.vz);
}

ZTEST(gte, outer_product_preserves_rot_matrix) {
    MATRIX m = {
        {{0x100, 0x200, 0x300}, {0x400, 0x500, 0x600}, {0x700, 0x800, 0x900}},
        {0, 0, 0}};
    MATRIX read = {0};
    VECTOR a = {1, 2, 3};
    VECTOR b = {4, 5, 6};
    VECTOR out = {0};
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    OuterProduct0(&a, &b, &out);
    OuterProduct12(&a, &b, &out);
    ReadRotMatrix(&read);
    zexpect_matrix_eq(&m, &read);
}

ZTEST(gte, rt_stores_untruncated_mac) {
    MATRIX m = {{{0x2000, 0, 0}, {0, 0x2000, 0}, {0, 0, 0x2000}},
                {100, 200, 300}};
    SVECTOR in = {0x7000, -0x7000, 100};
    VECTOR out = {0};
    unsigned int flag = 0;
    gte_SetRotMatrix(&m);
    gte_SetTransMatrix(&m);
    gte_ldv0(&in);
    gte_rt();
    gte_stlvnl(&out);
    gte_stflg(&flag);
    zexpect_s32_eq(0xE064, out.vx);
    zexpect_s32_eq(-0xDF38, out.vy);
    zexpect_s32_eq(500, out.vz);
    zexpect_u32_eq(0x81800000, flag);
}

ZTEST(gte, vector_normal) {
    VECTOR in[] = {{0x1000, 0, 0},
                   {0x300, 0x400, 0},
                   {-0x300, 0, 0x400},
                   {0x10000, 0x10000, 0x10000},
                   {7, -11, 13}};
    VECTOR exp[] = {{0x1000, 0, 0},
                    {2457, 3276, 0},
                    {-2457, 0, 3276},
                    {0, 0, 0},
                    {1564, -2458, 2904}};
    int i;
    for (i = 0; i < 5; i++) {
        VECTOR out = {0};
        VectorNormal(&in[i], &out);
        zprintf("vector %d\n", i);
        zexpect_s32_eq(exp[i].vx, out.vx);
        zexpect_s32_eq(exp[i].vy, out.vy);
        zexpect_s32_eq(exp[i].vz, out.vz);
    }
}

ZTEST(gte, vector_normal_varied) {
    VECTOR in[] = {
        {1, 1, 1},  {100, 200, -300}, {0x7FFF, 0, 0},   {-5000, 3000, 12000},
        {-1, 0, 0}, {0, 0, 0},        {0x12345, -2, 3}, {20000, 20000, 0}};
    VECTOR exp[] = {{2364, 2364, 2364}, {1097, 2194, -3292}, {4103, 0, 0},
                    {-1539, 922, 3691}, {-4096, 0, 0},       {0, 0, 0},
                    {4115, -1, 1},      {2901, 2901, 0}};
    int i;
    for (i = 0; i < 8; i++) {
        VECTOR out = {0};
        VectorNormal(&in[i], &out);
        zexpect_s32_eq(exp[i].vx, out.vx);
        zexpect_s32_eq(exp[i].vy, out.vy);
        zexpect_s32_eq(exp[i].vz, out.vz);
    }
}

ZTEST(gte, vector_normal_s) {
    VECTOR in[] = {{1, 1, 1},
                   {100, 200, -300},
                   {0x7FFF, 0, 0},
                   {-5000, 3000, 12000},
                   {0x12345, -2, 3},
                   {0, 0, 0},
                   {0x10000, 0x10000, 0x10000}};
    SVECTOR exp[] = {{2364, 2364, 2364}, {1097, 2194, -3292}, {4103, 0, 0},
                     {-1539, 922, 3691}, {4115, -1, 1},       {0, 0, 0},
                     {0, 0, 0}};
    long exp_ret[] = {3, 140000, 0x3FFF0001, 178000000, 81522854, 0, 0};
    int i;
    for (i = 0; i < 7; i++) {
        SVECTOR out = {0x5555, 0x5555, 0x5555, 0x5555};
        zexpect_s32_eq(exp_ret[i], VectorNormalS(&in[i], &out));
        zexpect_s16_eq(exp[i].vx, out.vx);
        zexpect_s16_eq(exp[i].vy, out.vy);
        zexpect_s16_eq(exp[i].vz, out.vz);
        zexpect_s16_eq(0x5555, out.pad);
    }
}

ZTEST(gte, vector_normal_ss) {
    SVECTOR in[] = {
        {1, 1, 1},  {100, 200, -300}, {0x7FFF, 0, 0}, {-5000, 3000, 12000},
        {-1, 0, 0}, {-0x8000, 0, 0},  {0, 0, 0}};
    SVECTOR exp[] = {{2364, 2364, 2364}, {1097, 2194, -3292}, {4103, 0, 0},
                     {-1539, 922, 3691}, {-4096, 0, 0},       {-4096, 0, 0},
                     {0, 0, 0}};
    long exp_ret[] = {3, 140000, 0x3FFF0001, 178000000, 1, 0x40000000, 0};
    int i;
    for (i = 0; i < 7; i++) {
        SVECTOR out = {0x5555, 0x5555, 0x5555, 0x5555};
        zexpect_s32_eq(exp_ret[i], VectorNormalSS(&in[i], &out));
        zexpect_s16_eq(exp[i].vx, out.vx);
        zexpect_s16_eq(exp[i].vy, out.vy);
        zexpect_s16_eq(exp[i].vz, out.vz);
        zexpect_s16_eq(0x5555, out.pad);
    }
}

static void SetupProjection(void) {
    MATRIX m = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}, {0, 0, 1000}};
    SetGeomOffset(160, 120);
    SetGeomScreen(1000);
    gte_SetRotMatrix(&m);
    gte_SetTransMatrix(&m);
}

ZTEST(gte, rt_applies_rotation_and_translation) {
    MATRIX m = {{{0, 0x1000, 0}, {0, 0, 0x1000}, {0x1000, 0, 0}},
                {-10, 20, -30}};
    SVECTOR in = {1, 2, 3};
    VECTOR out = {0};
    gte_SetRotMatrix(&m);
    gte_SetTransMatrix(&m);
    gte_ldv0(&in);
    gte_rt();
    gte_stlvnl(&out);
    zexpect_s32_eq(-8, out.vx);
    zexpect_s32_eq(23, out.vy);
    zexpect_s32_eq(-29, out.vz);
}

ZTEST(gte, rt_keeps_screen_xy_fifo) {
    SVECTOR v0 = {100, 50, 0}, v1 = {-100, -50, 0}, v2 = {0, 0, 0};
    SVECTOR other = {500, 500, 500};
    int sxy0 = 0, sxy1 = 0, sxy2 = 0;
    SetupProjection();
    gte_ldv3(&v0, &v1, &v2);
    gte_rtpt();
    gte_ldv0(&other);
    gte_rt();
    gte_stsxy3(&sxy0, &sxy1, &sxy2);
    zexpect_u32_eq(SXY(260, 170), sxy0);
    zexpect_u32_eq(SXY(60, 70), sxy1);
    zexpect_u32_eq(SXY(160, 120), sxy2);
}

ZTEST(gte, stflg_is_zero_without_overflow) {
    SVECTOR in = {1, 2, 3};
    unsigned int flag = 0xDEADBEEF;
    SetupProjection();
    gte_ldv0(&in);
    gte_rt();
    gte_stflg(&flag);
    zexpect_u32_eq(0, flag);
}

ZTEST(gte, readflg_matches_stflg) {
    MATRIX m = {{{0x2000, 0, 0}, {0, 0x2000, 0}, {0, 0, 0x2000}}, {0, 0, 0}};
    SVECTOR in = {0x7000, 1, -0x7000};
    unsigned int stored = 0, read = 0;
    gte_SetRotMatrix(&m);
    gte_SetTransMatrix(&m);
    gte_ldv0(&in);
    gte_rt();
    gte_stflg(&stored);
    gte_readflg(read);
    zexpect_u32_eq(0x81400000, stored);
    zexpect_u32_eq(stored, read);
}

ZTEST(gte, stsxy3_g3_fills_poly_vertices) {
    SVECTOR v0 = {100, 50, 0}, v1 = {-100, -50, 0}, v2 = {30, -40, 1000};
    POLY_G3 poly;
    memset(&poly, 0xCC, sizeof(poly));
    SetupProjection();
    gte_ldv3(&v0, &v1, &v2);
    gte_rtpt();
    gte_stsxy3_g3(&poly);
    zexpect_s16_eq(260, poly.x0);
    zexpect_s16_eq(170, poly.y0);
    zexpect_s16_eq(60, poly.x1);
    zexpect_s16_eq(70, poly.y1);
    zexpect_s16_eq(175, poly.x2);
    zexpect_s16_eq(100, poly.y2);
    zexpect_u8_eq(0xCC, poly.r1);
    zexpect_u8_eq(0xCC, poly.code);
}

static void RtWithIrSaturation(void) {
    MATRIX m = {{{0x2000, 0, 0}, {0, 0x2000, 0}, {0, 0, 0x2000}}, {0, 0, 0}};
    SVECTOR in = {0x7000, 0x7000, 0};
    gte_SetRotMatrix(&m);
    gte_SetTransMatrix(&m);
    gte_ldv0(&in);
    gte_rt();
}

ZTEST(gte, stflg_sign_extends_into_long) {
    long flag = 0x12345678;
    RtWithIrSaturation();
    gte_stflg(&flag);
    zexpect_s32_eq((int)0x81800000, (int)flag);
    zexpect_s32_eq(1, flag < 0);
}

ZTEST(gte, readflg_sign_extends_into_long) {
    long flag = 0x12345678;
    RtWithIrSaturation();
    gte_readflg(flag);
    zexpect_s32_eq((int)0x81800000, (int)flag);
    zexpect_s32_eq(1, flag < 0);
}

static void RtvSetup(void) {
    static MATRIX m = {{{0x1000, 0, 0}, {0, 0x800, 0}, {0, 0, 0x2000}},
                       {100, 200, 300}};
    static SVECTOR v0 = {10, 20, 30}, v1 = {-40, 50, 7}, v2 = {1000, -3, -500};
    gte_SetRotMatrix(&m);
    gte_SetTransMatrix(&m);
    gte_ldv3(&v0, &v1, &v2);
}

ZTEST(gte, rtv0_rotates_v0_without_translation) {
    VECTOR out = {0};
    RtvSetup();
    gte_rtv0();
    gte_stlvnl(&out);
    zexpect_s32_eq(10, out.vx);
    zexpect_s32_eq(10, out.vy);
    zexpect_s32_eq(60, out.vz);
}

ZTEST(gte, rtv1_rotates_v1_without_translation) {
    VECTOR out = {0};
    RtvSetup();
    gte_rtv1();
    gte_stlvnl(&out);
    zexpect_s32_eq(-40, out.vx);
    zexpect_s32_eq(25, out.vy);
    zexpect_s32_eq(14, out.vz);
}

ZTEST(gte, rtv2_rotates_v2_without_translation) {
    VECTOR out = {0};
    RtvSetup();
    gte_rtv2();
    gte_stlvnl(&out);
    zexpect_s32_eq(1000, out.vx);
    zexpect_s32_eq(-2, out.vy);
    zexpect_s32_eq(-1000, out.vz);
}

ZTEST(gte, ldlv0_keeps_low_16_bits) {
    MATRIX m = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}, {0, 0, 0}};
    VECTOR v = {0x12345, -0x10002, 7};
    VECTOR out = {0};
    gte_SetRotMatrix(&m);
    gte_ldlv0(&v);
    gte_rtv0();
    gte_stlvnl(&out);
    zexpect_s32_eq(0x2345, out.vx);
    zexpect_s32_eq(-2, out.vy);
    zexpect_s32_eq(7, out.vz);
}

ZTEST(gte, rtir_rotates_ir) {
    MATRIX m = {{{0, 0x1000, 0}, {0x1000, 0, 0}, {0, 0, 0x0800}}, {0, 0, 0}};
    VECTOR v = {0x100, 0x200, 0x300};
    VECTOR out = {0};
    gte_SetRotMatrix(&m);
    gte_ldlv0(&v);
    gte_rtv0();
    gte_stlvl(&out);
    zexpect_s32_eq(0x200, out.vx);
    zexpect_s32_eq(0x100, out.vy);
    zexpect_s32_eq(0x180, out.vz);
    gte_rtir();
    gte_stlvl(&out);
    zexpect_s32_eq(0x100, out.vx);
    zexpect_s32_eq(0x200, out.vy);
    zexpect_s32_eq(0xC0, out.vz);
}

ZTEST(gte, stlvl_stores_saturated_ir) {
    MATRIX m = {{{0x7FFF, 0, 0}, {0, 0x7FFF, 0}, {0, 0, 0x1000}}, {0, 0, 0}};
    VECTOR v = {0x7FFF, -0x8000, 5};
    VECTOR ir = {0};
    VECTOR mac = {0};
    gte_SetRotMatrix(&m);
    gte_ldlv0(&v);
    gte_rtv0();
    gte_stlvl(&ir);
    gte_stlvnl(&mac);
    zexpect_s32_eq(0x7FFF, ir.vx);
    zexpect_s32_eq(-0x8000, ir.vy);
    zexpect_s32_eq(5, ir.vz);
    zexpect_s32_eq(0x3FFF0, mac.vx);
    zexpect_s32_eq(-0x3FFF8, mac.vy);
}

static void SzFifoSetup(void) {
    SVECTOR a = {0, 0, 11};
    SVECTOR v0 = {0, 0, 22}, v1 = {0, 0, 33}, v2 = {0, 0, 44};
    SetupProjection();
    gte_ldv0(&a);
    gte_rtps();
    gte_ldv3(&v0, &v1, &v2);
    gte_rtpt();
}

ZTEST(gte, stsz_stores_sz3) {
    unsigned int sz = 0xDEADBEEF;
    SzFifoSetup();
    gte_stsz(&sz);
    zexpect_u32_eq(1044, sz);
}

ZTEST(gte, stsz3_stores_sz1_to_sz3) {
    unsigned int sz1 = 0xDEADBEEF, sz2 = 0xDEADBEEF, sz3 = 0xDEADBEEF;
    SzFifoSetup();
    gte_stsz3(&sz1, &sz2, &sz3);
    zexpect_u32_eq(1022, sz1);
    zexpect_u32_eq(1033, sz2);
    zexpect_u32_eq(1044, sz3);
}

ZTEST(gte, stsz4_stores_sz0_to_sz3) {
    unsigned int sz0 = 0xDEADBEEF, sz1 = 0xDEADBEEF, sz2 = 0xDEADBEEF,
                 sz3 = 0xDEADBEEF;
    SzFifoSetup();
    gte_stsz4(&sz0, &sz1, &sz2, &sz3);
    zexpect_u32_eq(1011, sz0);
    zexpect_u32_eq(1022, sz1);
    zexpect_u32_eq(1033, sz2);
    zexpect_u32_eq(1044, sz3);
}

ZTEST(gte, stsz3c_stores_sz1_to_sz3_contiguously) {
    unsigned int sz[4] = {0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF};
    SzFifoSetup();
    gte_stsz3c(sz);
    zexpect_u32_eq(1022, sz[0]);
    zexpect_u32_eq(1033, sz[1]);
    zexpect_u32_eq(1044, sz[2]);
    zexpect_u32_eq(0xDEADBEEF, sz[3]);
}

ZTEST(gte, stsz4c_stores_sz0_to_sz3_contiguously) {
    unsigned int sz[5] = {0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF,
                          0xDEADBEEF};
    SzFifoSetup();
    gte_stsz4c(sz);
    zexpect_u32_eq(1011, sz[0]);
    zexpect_u32_eq(1022, sz[1]);
    zexpect_u32_eq(1033, sz[2]);
    zexpect_u32_eq(1044, sz[3]);
    zexpect_u32_eq(0xDEADBEEF, sz[4]);
}

ZTEST(gte, push_pop_matrix) {
    MATRIX a = {{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}, {10, 20, 30}};
    MATRIX b = {{{-1, -2, -3}, {-4, -5, -6}, {-7, -8, -9}}, {-10, -20, -30}};
    MATRIX c = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}, {0, 0, 0}};
    MATRIX read = {0};
    SetRotMatrix(&a);
    SetTransMatrix(&a);
    PushMatrix();
    SetRotMatrix(&b);
    SetTransMatrix(&b);
    PushMatrix();
    SetRotMatrix(&c);
    SetTransMatrix(&c);
    PopMatrix();
    ReadRotMatrix(&read);
    zexpect_matrix_eq(&b, &read);
    PopMatrix();
    ReadRotMatrix(&read);
    zexpect_matrix_eq(&a, &read);
}

ZTEST(gte, pop_matrix_on_empty_stack_keeps_matrix) {
    MATRIX a = {{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}, {10, 20, 30}};
    MATRIX read = {0};
    SetRotMatrix(&a);
    SetTransMatrix(&a);
    PopMatrix();
    ReadRotMatrix(&read);
    zexpect_matrix_eq(&a, &read);
}

ZTEST(gte, push_matrix_holds_20_entries) {
    MATRIX m = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}, {0, 0, 0}};
    MATRIX read = {0};
    int i;
    SetRotMatrix(&m);
    for (i = 0; i < 21; i++) {
        m.t[0] = i;
        SetTransMatrix(&m);
        PushMatrix();
    }
    m.t[0] = 99;
    SetTransMatrix(&m);
    for (i = 19; i >= 0; i--) {
        PopMatrix();
        ReadRotMatrix(&read);
        zexpect_s32_eq(i, read.t[0]);
    }
    PopMatrix();
    ReadRotMatrix(&read);
    zexpect_s32_eq(0, read.t[0]);
}

#define RGBCD(r, g, b, cd)                                                     \
    ((unsigned int)(r) | ((unsigned int)(g) << 8) |                            \
     ((unsigned int)(b) << 16) | ((unsigned int)(cd) << 24))

ZTEST(gte, ldrgb3_strgb3_round_trips_color_fifo) {
    unsigned int in0 = RGBCD(1, 2, 3, 4), in1 = RGBCD(5, 6, 7, 8),
                 in2 = RGBCD(9, 10, 11, 12);
    unsigned int out0 = 0, out1 = 0, out2 = 0;
    gte_ldrgb3(&in0, &in1, &in2);
    gte_strgb3(&out0, &out1, &out2);
    zexpect_u32_eq(in0, out0);
    zexpect_u32_eq(in1, out1);
    zexpect_u32_eq(in2, out2);
}

ZTEST(gte, ldrgb3c_loads_contiguous_colors) {
    unsigned int in[3] = {
        RGBCD(0x10, 0x20, 0x30, 0x40), RGBCD(0x50, 0x60, 0x70, 0x80),
        RGBCD(0x90, 0xA0, 0xB0, 0xC0)};
    unsigned int out0 = 0, out1 = 0, out2 = 0;
    gte_ldrgb3c(in);
    gte_strgb3(&out0, &out1, &out2);
    zexpect_u32_eq(in[0], out0);
    zexpect_u32_eq(in[1], out1);
    zexpect_u32_eq(in[2], out2);
}

ZTEST(gte, strgb_stores_rgb2) {
    unsigned int in0 = RGBCD(1, 2, 3, 4), in1 = RGBCD(5, 6, 7, 8),
                 in2 = RGBCD(9, 10, 11, 12);
    unsigned int out = 0;
    gte_ldrgb3(&in0, &in1, &in2);
    gte_strgb(&out);
    zexpect_u32_eq(in2, out);
}

ZTEST(gte, ldrgb_sets_rgbc) {
    unsigned int in = RGBCD(40, 80, 120, 0x2C);
    unsigned int out = 0;
    gte_ldrgb(&in);
    gte_lddp(0);
    gte_dpcs();
    gte_strgb(&out);
    zexpect_u32_eq(in, out);
}

ZTEST(gte, ldrgb3_sets_rgbc_to_third_color) {
    unsigned int in0 = RGBCD(1, 2, 3, 4), in1 = RGBCD(5, 6, 7, 8),
                 in2 = RGBCD(90, 100, 110, 0x2C);
    unsigned int out = 0;
    gte_ldrgb3(&in0, &in1, &in2);
    gte_lddp(0);
    gte_dpcs();
    gte_strgb(&out);
    zexpect_u32_eq(in2, out);
}

ZTEST(gte, ldrgb3c_sets_rgbc_to_third_color) {
    unsigned int in[3] = {RGBCD(1, 2, 3, 4), RGBCD(5, 6, 7, 8),
                          RGBCD(90, 100, 110, 0x2C)};
    unsigned int out = 0;
    gte_ldrgb3c(in);
    gte_lddp(0);
    gte_dpcs();
    gte_strgb(&out);
    zexpect_u32_eq(in[2], out);
}

static void LoadColorFifo(void) {
    static unsigned int in[3] = {
        RGBCD(0x11, 0x12, 0x13, 0x14), RGBCD(0x21, 0x22, 0x23, 0x24),
        RGBCD(0x31, 0x32, 0x33, 0x34)};
    gte_ldrgb3c(in);
}

ZTEST(gte, strgb3_g3_fills_poly_colors) {
    POLY_G3 poly;
    memset(&poly, 0xCC, sizeof(poly));
    LoadColorFifo();
    gte_strgb3_g3(&poly);
    zexpect_u8_eq(0x11, poly.r0);
    zexpect_u8_eq(0x12, poly.g0);
    zexpect_u8_eq(0x13, poly.b0);
    zexpect_u8_eq(0x14, poly.code); // gets replaced, confirmed on real HW
    zexpect_u8_eq(0x21, poly.r1);
    zexpect_u8_eq(0x22, poly.g1);
    zexpect_u8_eq(0x23, poly.b1);
    zexpect_u8_eq(0x24, poly.pad1);
    zexpect_u8_eq(0x31, poly.r2);
    zexpect_u8_eq(0x32, poly.g2);
    zexpect_u8_eq(0x33, poly.b2);
    zexpect_u8_eq(0x34, poly.pad2);
    zexpect_s16_eq((short)0xCCCC, poly.x0);
    zexpect_s16_eq((short)0xCCCC, poly.y2);
}

ZTEST(gte, strgb3_gt3_fills_poly_colors) {
    POLY_GT3 poly;
    memset(&poly, 0xCC, sizeof(poly));
    LoadColorFifo();
    gte_strgb3_gt3(&poly);
    zexpect_u8_eq(0x11, poly.r0);
    zexpect_u8_eq(0x12, poly.g0);
    zexpect_u8_eq(0x13, poly.b0);
    zexpect_u8_eq(0x14, poly.code);
    zexpect_u8_eq(0x21, poly.r1);
    zexpect_u8_eq(0x22, poly.g1);
    zexpect_u8_eq(0x23, poly.b1);
    zexpect_u8_eq(0x24, poly.p1);
    zexpect_u8_eq(0x31, poly.r2);
    zexpect_u8_eq(0x32, poly.g2);
    zexpect_u8_eq(0x33, poly.b2);
    zexpect_u8_eq(0x34, poly.p2);
    zexpect_u8_eq(0xCC, poly.u0);
    zexpect_u16_eq(0xCCCC, poly.tpage);
}

ZTEST(gte, strgb3_g4_fills_first_three_poly_colors) {
    POLY_G4 poly;
    memset(&poly, 0xCC, sizeof(poly));
    LoadColorFifo();
    gte_strgb3_g4(&poly);
    zexpect_u8_eq(0x11, poly.r0);
    zexpect_u8_eq(0x14, poly.code);
    zexpect_u8_eq(0x21, poly.r1);
    zexpect_u8_eq(0x24, poly.pad1);
    zexpect_u8_eq(0x31, poly.r2);
    zexpect_u8_eq(0x33, poly.b2);
    zexpect_u8_eq(0x34, poly.pad2);
    zexpect_u8_eq(0xCC, poly.r3);
    zexpect_u8_eq(0xCC, poly.pad3);
}

ZTEST(gte, strgb3_gt4_fills_first_three_poly_colors) {
    POLY_GT4 poly;
    memset(&poly, 0xCC, sizeof(poly));
    LoadColorFifo();
    gte_strgb3_gt4(&poly);
    zexpect_u8_eq(0x11, poly.r0);
    zexpect_u8_eq(0x14, poly.code);
    zexpect_u8_eq(0x21, poly.r1);
    zexpect_u8_eq(0x24, poly.p1);
    zexpect_u8_eq(0x31, poly.r2);
    zexpect_u8_eq(0x33, poly.b2);
    zexpect_u8_eq(0x34, poly.p2);
    zexpect_u8_eq(0xCC, poly.r3);
    zexpect_u8_eq(0xCC, poly.p3);
}

ZTEST(gte, nccs_lights_and_colors_normal) {
    MATRIX light = {{{0x1000, 0x800, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}};
    MATRIX color = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0x800, 0, 0x800}}};
    SVECTOR normal = {0x400, 0x800, 0xC00};
    unsigned int rgb = RGBCD(128, 64, 200, 0x30);
    unsigned int out = 0;
    gte_SetLightMatrix(&light);
    gte_SetColorMatrix(&color);
    gte_SetBackColor(16, 32, 48);
    gte_ldv0(&normal);
    gte_ldrgb(&rgb);
    gte_nccs();
    gte_strgb(&out);
    zexpect_u32_eq(RGBCD(72, 40, 162, 0x30), out);
}

ZTEST(gte, nccs_saturates_color) {
    MATRIX light = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}};
    MATRIX color = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}};
    SVECTOR normal = {0x2000, -0x1000, 0x1000};
    unsigned int rgb = RGBCD(255, 255, 255, 0x3C);
    unsigned int out = 0;
    int flag = 0;
    gte_SetLightMatrix(&light);
    gte_SetColorMatrix(&color);
    gte_SetBackColor(0, 0, 0);
    gte_ldv0(&normal);
    gte_ldrgb(&rgb);
    gte_nccs();
    gte_strgb(&out);
    gte_stflg(&flag);
    zexpect_u32_eq(RGBCD(255, 0, 255, 0x3C), out);
    zexpect_u32_eq(0x80A00000, (unsigned int)flag);
}

ZTEST(gte, dpcs_moves_color_toward_far_color) {
    unsigned int rgb = RGBCD(40, 80, 120, 0x2C);
    unsigned int out = 0;
    gte_SetFarColor(200, 100, 50);
    gte_ldrgb(&rgb);
    gte_lddp(0x800);
    gte_dpcs();
    gte_strgb(&out);
    zexpect_u32_eq(RGBCD(120, 90, 85, 0x2C), out);
}

static void GeomRtps(SVECTOR* v, int* sxy) {
    MATRIX m = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}, {0, 0, 1000}};
    gte_SetRotMatrix(&m);
    gte_SetTransMatrix(&m);
    gte_ldv0(v);
    gte_rtps();
    gte_stsxy(sxy);
}

ZTEST(gte, setgeomoffset_and_screen_project) {
    SVECTOR v = {100, 50, 0};
    int sxy = 0;
    gte_SetGeomOffset(160, 120);
    gte_SetGeomScreen(500);
    GeomRtps(&v, &sxy);
    zexpect_u32_eq(SXY(210, 145), sxy);
}

ZTEST(gte, setgeomoffset_accepts_negative_offset) {
    SVECTOR v = {100, 50, 0};
    int sxy = 0;
    gte_SetGeomOffset(-31, -63);
    gte_SetGeomScreen(1000);
    GeomRtps(&v, &sxy);
    zexpect_u32_eq(SXY(69, -13), sxy);
}

typedef struct {
    int sxy, sz, mac0, flag;
    VECTOR mac;
} RtpsResult;

static void RtpsResultRead(RtpsResult* r) {
    gte_stsxy(&r->sxy);
    gte_stsz(&r->sz);
    gte_stopz(&r->mac0);
    gte_stflg(&r->flag);
    gte_stlvnl(&r->mac);
}

ZTEST(gte, cmd_00_behaves_like_rtps) {
    SVECTOR v = {100, -50, 300}, other = {-20, 70, 900};
    RtpsResult exp, act;
    SetupProjection();
    gte_ldv0(&v);
    Psyz_GteCommand(0x0180001);
    RtpsResultRead(&exp);
    gte_ldv0(&other);
    gte_rtps();
    gte_ldv0(&v);
    Psyz_GteCommand(0x0180000);
    RtpsResultRead(&act);
    zexpect_u32_eq(SXY(236, 81), exp.sxy);
    zexpect_u32_eq(exp.sxy, act.sxy);
    zexpect_u32_eq(exp.sz, act.sz);
    zexpect_s32_eq(exp.mac0, act.mac0);
    zexpect_u32_eq(exp.flag, act.flag);
    zexpect_s32_eq(exp.mac.vx, act.mac.vx);
    zexpect_s32_eq(exp.mac.vy, act.mac.vy);
    zexpect_s32_eq(exp.mac.vz, act.mac.vz);
}

static void DcplSetup(void) {
    MATRIX m = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}};
    SVECTOR ir = {0x800, 0x400, 0xC00};
    unsigned int rgb = RGBCD(100, 150, 200, 0x38);
    gte_SetRotMatrix(&m);
    gte_ldv0(&ir);
    gte_rtv0();
    gte_SetFarColor(20, 250, 60);
    gte_ldrgb(&rgb);
    gte_lddp(0x600);
}

ZTEST(gte, cmd_1a_behaves_like_dcpl) {
    unsigned int exp_rgb = 0, act_rgb = 0;
    int exp_flag = 0, act_flag = 0;
    VECTOR exp_mac, act_mac;
    DcplSetup();
    Psyz_GteCommand(0x0680029);
    gte_strgb(&exp_rgb);
    gte_stflg(&exp_flag);
    gte_stlvnl(&exp_mac);
    DcplSetup();
    Psyz_GteCommand(0x068001A);
    gte_strgb(&act_rgb);
    gte_stflg(&act_flag);
    gte_stlvnl(&act_mac);
    zexpect_u32_eq(RGBCD(38, 117, 116, 0x38), exp_rgb);
    zexpect_u32_eq(exp_rgb, act_rgb);
    zexpect_u32_eq(exp_flag, act_flag);
    zexpect_s32_eq(exp_mac.vx, act_mac.vx);
    zexpect_s32_eq(exp_mac.vy, act_mac.vy);
    zexpect_s32_eq(exp_mac.vz, act_mac.vz);
}

ZTEST(gte, gte_ldsz3_loads_sz1_to_sz3) {
    unsigned int sz1 = 0xDEADBEEF, sz2 = 0xDEADBEEF, sz3 = 0xDEADBEEF;
    gte_ldsz3(1111, 2222, 3333);
    gte_stsz3(&sz1, &sz2, &sz3);
    zexpect_u32_eq(1111, sz1);
    zexpect_u32_eq(2222, sz2);
    zexpect_u32_eq(3333, sz3);
}

ZTEST(gte, gte_ldsz4_loads_sz0_to_sz3) {
    unsigned int sz0 = 0xDEADBEEF, sz1 = 0xDEADBEEF, sz2 = 0xDEADBEEF,
                 sz3 = 0xDEADBEEF;
    gte_ldsz4(1111, 2222, 3333, 4444);
    gte_stsz4(&sz0, &sz1, &sz2, &sz3);
    zexpect_u32_eq(1111, sz0);
    zexpect_u32_eq(2222, sz1);
    zexpect_u32_eq(3333, sz2);
    zexpect_u32_eq(4444, sz3);
}

ZTEST(gte, gte_ldsz3_keeps_low_16_bits) {
    unsigned int sz1 = 0xDEADBEEF, sz2 = 0xDEADBEEF, sz3 = 0xDEADBEEF;
    gte_ldsz3(0x12345, 0xFFFF, 0x10000);
    gte_stsz3(&sz1, &sz2, &sz3);
    zexpect_u32_eq(0x2345, sz1);
    zexpect_u32_eq(0xFFFF, sz2);
    zexpect_u32_eq(0, sz3);
}

ZTEST(gte, gte_ldsz3_feeds_avsz3) {
    unsigned int otz = 0xDEADBEEF;
    gte_ldsz3(1000, 2000, 3000);
    gte_avsz3();
    gte_stotz(&otz);
    zexpect_u32_eq(499, otz);
}

ZTEST(gte, gte_ldsz4_feeds_avsz4) {
    unsigned int otz = 0xDEADBEEF;
    gte_ldsz4(0x1000, 0x2000, 0x3000, 0x4000);
    gte_avsz4();
    gte_stotz(&otz);
    zexpect_u32_eq(0xA00, otz);
}

static void ZeroIr(void) {
    SVECTOR zero = {0, 0, 0};
    gte_ldv0(&zero);
    gte_rtv0();
}

ZTEST(gte, dpcs_flags_far_color_difference_saturation) {
    unsigned int rgb = RGBCD(0, 0, 0, 0x20), out = 0;
    int flag = 0;
    gte_SetFarColor(0x1000, 0, 0);
    gte_ldrgb(&rgb);
    gte_lddp(0);
    gte_dpcs();
    gte_strgb(&out);
    gte_stflg(&flag);
    zexpect_u32_eq(rgb, out);
    zexpect_u32_eq(0x81000000, (unsigned int)flag);
}

ZTEST(gte, dpcs_far_color_difference_ignores_lm) {
    unsigned int rgb = RGBCD(100, 0, 0, 0x20), out = 0;
    int flag = 0;
    gte_SetFarColor(0, 0, 0);
    gte_ldrgb(&rgb);
    gte_lddp(0);
    Psyz_GteCommand(0x0780410);
    gte_strgb(&out);
    gte_stflg(&flag);
    zexpect_u32_eq(rgb, out);
    zexpect_u32_eq(0, (unsigned int)flag);
}

ZTEST(gte, dpcs_flags_far_color_difference_overflow) {
    unsigned int rgb = RGBCD(1, 0, 0, 0x20), out = 0;
    int flag = 0;
    gte_SetFarColor(0x08000000, 0, 0);
    gte_ldrgb(&rgb);
    gte_lddp(0);
    gte_dpcs();
    gte_strgb(&out);
    gte_stflg(&flag);
    zexpect_u32_eq(rgb, out);
    zexpect_u32_eq(0x89000000, (unsigned int)flag);
}

ZTEST(gte, dpct_flags_far_color_difference_saturation) {
    unsigned int c[3] = {RGBCD(0, 0, 0, 0x20), RGBCD(0, 0, 0, 0x20),
                         RGBCD(0, 0, 0, 0x20)};
    int flag = 0;
    gte_SetFarColor(0, 0x1000, 0);
    gte_ldrgb3c(c);
    gte_lddp(0);
    Psyz_GteCommand(0x0F8002A);
    gte_stflg(&flag);
    zexpect_u32_eq(0x80800000, (unsigned int)flag);
}

ZTEST(gte, intpl_flags_far_color_difference_saturation) {
    unsigned int rgb = RGBCD(0, 0, 0, 0x20);
    int flag = 0;
    ZeroIr();
    gte_SetFarColor(0, 0, 0x1000);
    gte_ldrgb(&rgb);
    gte_lddp(0);
    Psyz_GteCommand(0x0980011);
    gte_stflg(&flag);
    zexpect_u32_eq(0x00400000, (unsigned int)flag);
}

ZTEST(gte, dcpl_flags_far_color_difference_saturation) {
    unsigned int rgb = RGBCD(0, 0, 0, 0x20);
    int flag = 0;
    ZeroIr();
    gte_SetFarColor(-0x1000, 0, 0);
    gte_ldrgb(&rgb);
    gte_lddp(0);
    Psyz_GteCommand(0x0680029);
    gte_stflg(&flag);
    zexpect_u32_eq(0x81000000, (unsigned int)flag);
}

ZTEST(gte, dcpl_ignores_unused_command_bits) {
    unsigned int exp_rgb = 0, act_rgb = 0;
    int exp_flag = 0, act_flag = 0;
    VECTOR exp_mac, act_mac;
    DcplSetup();
    Psyz_GteCommand(0x0680029);
    gte_strgb(&exp_rgb);
    gte_stflg(&exp_flag);
    gte_stlvnl(&exp_mac);
    DcplSetup();
    Psyz_GteCommand(0x1F81BE9);
    gte_strgb(&act_rgb);
    gte_stflg(&act_flag);
    gte_stlvnl(&act_mac);
    zexpect_u32_eq(RGBCD(38, 117, 116, 0x38), act_rgb);
    zexpect_u32_eq(exp_flag, act_flag);
    zexpect_s32_eq(exp_mac.vx, act_mac.vx);
    zexpect_s32_eq(exp_mac.vy, act_mac.vy);
    zexpect_s32_eq(exp_mac.vz, act_mac.vz);
}

ZTEST(gte, mvmva_far_color_first_step_ignores_lm) {
    MATRIX m = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}};
    int flag = 0;
    gte_SetRotMatrix(&m);
    ZeroIr();
    gte_SetFarColor(-1, 0, 0);
    Psyz_GteCommand(0x0084412);
    gte_stflg(&flag);
    zexpect_u32_eq(0, (unsigned int)flag);
}

typedef struct {
    int ir0, mac0, flag;
} DepthResult;

static void RtpsWithDepth(int dqb, DepthResult* r) {
    SVECTOR v = {10, 20, 30};
    SetupProjection();
    GTE_SET_DQA(0);
    GTE_SET_DQB(dqb);
    gte_ldv0(&v);
    gte_rtps();
    GTE_READ_IR0(r->ir0);
    gte_stopz(&r->mac0);
    gte_stflg(&r->flag);
}

ZTEST(gte, rtps_ir0_flags_fraction_above_limit) {
    DepthResult r;
    RtpsWithDepth(0x1000001, &r);
    zexpect_s32_eq(0x1000, r.ir0);
    zexpect_s32_eq(0x1000001, r.mac0);
    zexpect_u32_eq(0x00001000, (unsigned int)r.flag);
}
