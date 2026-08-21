#include <gtest/gtest.h>
extern "C" {
#include <psyz.h>
#include <kernel.h>
#include <libgte.h>
}

class gte_Test : public testing::Test {
  protected:
    gte_Test() {}
    ~gte_Test() override = default;
    void SetUp() override { InitGeom(); }
    void TearDown() override { InitGeom(); }
    static void EqMatrix(MATRIX* m1, MATRIX* m2) {
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                EXPECT_EQ(m1->m[i][j], m2->m[i][j])
                    << "Matrix coefficient mismatch at m[" << i << "][" << j
                    << "]";
            }
        }
        for (int i = 0; i < 3; i++) {
            EXPECT_EQ(m1->t[i], m2->t[i])
                << "Translation vector mismatch at t[" << i << "]";
        }
    }

    struct RTPContext {
        MATRIX m = {0};
        SVECTOR svs[3] = {};
        int lgs[3] = {};
        int p = 0;
        int flag = 0;

        RTPContext() {
            InitGeom();
            SetGeomOffset(0, 0);
            SetRotMatrix(&m);
            SetTransMatrix(&m);
        }

        long RotTransPers() {
            return ::RotTransPers(&svs[0], &lgs[0], &p, &flag);
        }

        long RotTransPers3() {
            return ::RotTransPers3(&svs[0], &svs[1], &svs[2], &lgs[0], &lgs[1],
                                   &lgs[2], &p, &flag);
        }

        void SetTransM(long tx, long ty, long tz) {
            m.t[0] = tx;
            m.t[1] = ty;
            m.t[2] = tz;
            SetTransMatrix(&m);
        }

        void SetRotM(short m00, short m01, short m02, short m10, short m11,
                     short m12, short m20, short m21, short m22) {
            m.m[0][0] = m00;
            m.m[0][1] = m01;
            m.m[0][2] = m02;
            m.m[1][0] = m10;
            m.m[1][1] = m11;
            m.m[1][2] = m12;
            m.m[2][0] = m20;
            m.m[2][1] = m21;
            m.m[2][2] = m22;
            SetRotMatrix(&m);
        }

        void SetSvs(short tx, short ty, short tz) {
            svs[0].vy = tx;
            svs[1].vy = ty;
            svs[2].vz = tz;
        }

        void CheckRTP_(unsigned int lgs0, long p_exp, unsigned int flag_exp,
                       const char* file, int line) {
            SCOPED_TRACE(
                ::testing::Message() << "Called from " << file << ":" << line);
            EXPECT_EQ((short)lgs[0], (short)lgs0);
            EXPECT_EQ((short)(lgs[0] >> 16), (short)(lgs0 >> 16));
            EXPECT_EQ(p, p_exp);
            EXPECT_EQ((unsigned int)flag, flag_exp);
        }

        void CheckRTP3_(
            unsigned int lgs0, unsigned int lgs1, unsigned int lgs2, long p_exp,
            unsigned int flag_exp, const char* file, int line) {
            SCOPED_TRACE(
                ::testing::Message() << "Called from " << file << ":" << line);
            EXPECT_EQ((unsigned int)lgs[0], lgs0);
            EXPECT_EQ((unsigned int)lgs[1], lgs1);
            EXPECT_EQ((unsigned int)lgs[2], lgs2);
            EXPECT_EQ(p, p_exp);
            EXPECT_EQ((unsigned int)flag, flag_exp);
        }

        void TestRTP_(long ret_exp, unsigned int lgs0, long p_exp,
                      unsigned int flag_exp, const char* file, int line) {
            SCOPED_TRACE(
                ::testing::Message() << "Called from " << file << ":" << line);
            EXPECT_EQ(ret_exp, RotTransPers());
            CheckRTP_(lgs0, p_exp, flag_exp, file, line);
        }

        void TestRTP3_(long ret_exp, unsigned int lgs0, unsigned int lgs1,
                       unsigned int lgs2, long p_exp, unsigned int flag_exp,
                       const char* file, int line) {
            SCOPED_TRACE(
                ::testing::Message() << "Called from " << file << ":" << line);
            EXPECT_EQ(ret_exp, RotTransPers3());
            CheckRTP3_(lgs0, lgs1, lgs2, p_exp, flag_exp, file, line);
        }

#define SXY(x, y)                                                              \
    (((unsigned int)(x) & 0xFFFF) | (((unsigned int)(y) & 0xFFFF) << 16))
#define MV(x, y, z) x, y, z
#define CheckRTP3(lgs0, lgs1, lgs2, p_exp, flag_exp)                           \
    CheckRTP3_(lgs0, lgs1, lgs2, p_exp, flag_exp, __FILE__, __LINE__)
#define TestRTP(ret_exp, lgs0, p_exp, flag_exp)                                \
    TestRTP_(ret_exp, lgs0, p_exp, flag_exp, __FILE__, __LINE__)
#define TestRTP3(ret_exp, lgs0, lgs1, lgs2, p_exp, flag_exp)                   \
    TestRTP3_(ret_exp, lgs0, lgs1, lgs2, p_exp, flag_exp, __FILE__, __LINE__)
    };
};

TEST_F(gte_Test, rsin) {
    EXPECT_EQ(rsin(0x0000), 0x0000);
    EXPECT_EQ(rsin(0x0001), 0x0006);
    EXPECT_EQ(rsin(0x0002), 0x000D);
    EXPECT_EQ(rsin(0x0010), 0x0065);
    EXPECT_EQ(rsin(0x0100), 0x061F);
    EXPECT_EQ(rsin(0x0200), 0x0B50);
    EXPECT_EQ(rsin(0x0400), 0x1000);
    EXPECT_EQ(rsin(0x0800), 0x0000);
    EXPECT_EQ(rsin(0x1000), 0x0000);
}

TEST_F(gte_Test, trans_matrix) {
    MATRIX m = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    MATRIX exp = {0, 1, 2, 3, 4, 5, 6, 7, 8, 16, 17, 18};
    VECTOR t = {16, 17, 18};
    EXPECT_EQ(TransMatrix(&m, &t), &m);
    EqMatrix(&m, &exp);
}

TEST_F(gte_Test, rot_matrix) {
    MATRIX m = {1,  2,  3, //
                4,  5,  6, //
                7,  8,  9, //
                10, 11, 12};
    MATRIX exp = {+0x0FFD, -0x0071, +0x006B, //
                  +0x0073, +0x0FFC, -0x0065, //
                  -0x0069, +0x0067, +0x0FFE, //
                  10,      11,      12};
    SVECTOR sv = {16, 17, 18};
    EXPECT_EQ(RotMatrix(&sv, &m), &m);
    EqMatrix(&m, &exp);
}

TEST_F(gte_Test, trans_matrix_negative) {
    MATRIX m = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0};
    VECTOR t = {-1, -2000, 30000};
    EXPECT_EQ(TransMatrix(&m, &t), &m);
    EXPECT_EQ(m.t[0], -1);
    EXPECT_EQ(m.t[1], -2000);
    EXPECT_EQ(m.t[2], 30000);
    EXPECT_EQ(m.m[0][0], 1);
    EXPECT_EQ(m.m[2][2], 9);
}

TEST_F(gte_Test, scale_matrix_identity) {
    MATRIX m = {0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000, 7, 8, 9};
    MATRIX exp = {0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000, 7, 8, 9};
    VECTOR s = {0x1000, 0x1000, 0x1000};
    EXPECT_EQ(ScaleMatrix(&m, &s), &m);
    EqMatrix(&m, &exp);
}

TEST_F(gte_Test, scale_matrix_per_axis) {
    MATRIX m = {0x1000, 0x1000, 0x1000, //
                0x1000, 0x1000, 0x1000, //
                0x1000, 0x1000, 0x1000, //
                0,      0,      0};
    VECTOR s = {0x800, 0x2000, 0};
    EXPECT_EQ(ScaleMatrix(&m, &s), &m);
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(m.m[i][0], 0x800) << "row " << i;
        EXPECT_EQ(m.m[i][1], 0x2000) << "row " << i;
        EXPECT_EQ(m.m[i][2], 0) << "row " << i;
    }
}

TEST_F(gte_Test, scale_matrix_negative) {
    MATRIX m = {0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000, 0, 0, 0};
    VECTOR s = {-0x1000, -0x800, 0x1000};
    ScaleMatrix(&m, &s);
    EXPECT_EQ(m.m[0][0], -0x1000);
    EXPECT_EQ(m.m[1][1], -0x800);
    EXPECT_EQ(m.m[2][2], 0x1000);
}

TEST_F(gte_Test, mul_matrix_identity) {
    MATRIX a = {0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000, 0, 0, 0};
    MATRIX b = {0x0100, 0x0200, 0x0300, 0x0400, 0x0500, 0x0600,
                0x0700, 0x0800, 0x0900, 0,      0,      0};
    MATRIX exp = b;
    EXPECT_EQ(MulMatrix(&a, &b), &a);
    EqMatrix(&a, &exp);
}

TEST_F(gte_Test, mul_matrix_half_identity) {
    MATRIX a = {0x0800, 0, 0, 0, 0x0800, 0, 0, 0, 0x0800, 0, 0, 0};
    MATRIX b = {0x1000, 0x2000, 0, 0, 0x1000, 0, 0, 0, 0x1000, 0, 0, 0};
    MATRIX exp = {0x0800, 0x1000, 0, 0, 0x0800, 0, 0, 0, 0x0800, 0, 0, 0};
    MulMatrix(&a, &b);
    EqMatrix(&a, &exp);
}

TEST_F(gte_Test, mul_matrix_permutation) {
    MATRIX a = {0, 0x1000, 0, 0x1000, 0, 0, 0, 0, 0x1000, 0, 0, 0};
    MATRIX b = {0x0111, 0x0222, 0x0333, 0x0444, 0x0555, 0x0666,
                0x0777, 0x0888, 0x0999, 0,      0,      0};
    MATRIX exp = {0x0444, 0x0555, 0x0666, 0x0111, 0x0222, 0x0333,
                  0x0777, 0x0888, 0x0999, 0,      0,      0};
    MulMatrix(&a, &b);
    EqMatrix(&a, &exp);
}

TEST_F(gte_Test, transpose_matrix) {
    MATRIX m = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    MATRIX out = {0};
    // returns the destination, and only the 3x3 part is written
    EXPECT_EQ(TransposeMatrix(&m, &out), &out);
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            EXPECT_EQ(out.m[i][j], m.m[j][i])
                << "at [" << i << "][" << j << "]";
        }
    }
}

TEST_F(gte_Test, transpose_matrix_twice_is_identity) {
    MATRIX m = {-1, 2, -3, 4, -5, 6, -7, 8, -9, 0, 0, 0};
    MATRIX once = {0};
    MATRIX twice = {0};
    TransposeMatrix(&m, &once);
    TransposeMatrix(&once, &twice);
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            EXPECT_EQ(twice.m[i][j], m.m[i][j])
                << "at [" << i << "][" << j << "]";
        }
    }
}

TEST_F(gte_Test, transpose_matrix_symmetric_unchanged) {
    MATRIX m = {1, 2, 3, 2, 4, 5, 3, 5, 6, 0, 0, 0};
    MATRIX out = {0};
    TransposeMatrix(&m, &out);
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            EXPECT_EQ(out.m[i][j], m.m[i][j]);
        }
    }
}

TEST_F(gte_Test, square_root_0) {
    EXPECT_EQ(SquareRoot0(0), 0);
    EXPECT_EQ(SquareRoot0(1), 1);
    EXPECT_EQ(SquareRoot0(2), 1);
    EXPECT_EQ(SquareRoot0(4), 2);
    EXPECT_EQ(SquareRoot0(8), 2);
    EXPECT_EQ(SquareRoot0(9), 3);
    EXPECT_EQ(SquareRoot0(0x10000), 0x100);
    EXPECT_EQ(SquareRoot0(0x10000000), 0x4000);
}

TEST_F(gte_Test, square_root_12) {
    EXPECT_EQ(SquareRoot12(0), 0);
    EXPECT_EQ(SquareRoot12(1), 0x40);
    EXPECT_EQ(SquareRoot12(2), 0x5A);
    EXPECT_EQ(SquareRoot12(4), 0x80);
    EXPECT_EQ(SquareRoot12(8), 0xB5);
    EXPECT_EQ(SquareRoot12(9), 0xC0);
    EXPECT_EQ(SquareRoot12(0x10000), 0x4000);
    EXPECT_EQ(SquareRoot12(0x10000000), 0x100000);
}

// Some GTE tests are ports from the PCSX Redux regression suite
// https://github.com/grumpycoders/pcsx-redux/tree/main/src/mips/tests/gte

TEST_F(gte_Test, avsz3_uses_sz123) {
    EXPECT_EQ(AverageZ3(1000, 2000, 3000), 499);
}

TEST_F(gte_Test, avsz4_basic) {
    EXPECT_EQ(AverageZ4(0x1000, 0x2000, 0x3000, 0x4000), 0xA00);
}

TEST_F(gte_Test, avsz4_stotz_macros) {
    long expected = AverageZ4(0x100, 0x200, 0x300, 0x400);
    unsigned int otz;

    gte_avsz4();
    gte_stotz(&otz);

    EXPECT_EQ(otz, expected);
}

TEST_F(gte_Test, nclip_ccw) {
    EXPECT_EQ(NormalClip(0x00000000, 0x00000064, 0x00640000), 10000);
}

TEST_F(gte_Test, nclip_cw) {
    EXPECT_EQ(NormalClip(0x00000000, 0x00640000, 0x00000064), -10000);
}

TEST_F(gte_Test, nclip_collinear) {
    EXPECT_EQ(NormalClip(0x00000000, 0x00320032, 0x00640064), 0);
}

TEST_F(gte_Test, nclip_large_coords) {
    EXPECT_EQ(NormalClip(0xFC0003FF, 0x03FFFC00, 0x00000000), -2047);
}

TEST_F(gte_Test, nclip_overflow) {
    EXPECT_EQ(NormalClip(0x7FFF7FFF, 0x7FFF8000, 0x80007FFF), -131071);
}

TEST_F(gte_Test, rtps_identity_center) {
    RTPContext ctx;
    ctx.SetRotM(0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000);
    ctx.SetTransM(0, 0, 1000);
    SetGeomOffset(160, 120);
    SetGeomScreen(200);
    ctx.svs[0].vx = 0;
    ctx.svs[0].vy = 0;
    ctx.svs[0].vz = 0;
    long sz = ctx.RotTransPers();
    EXPECT_EQ(sz, 1000 >> 2);
    EXPECT_EQ((short)ctx.lgs[0], 160);
    EXPECT_EQ((short)(ctx.lgs[0] >> 16), 120);
}

TEST_F(gte_Test, rtps_offset_vertex) {
    RTPContext ctx;
    ctx.SetRotM(0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000);
    ctx.SetTransM(0, 0, 0);
    SetGeomOffset(160, 120);
    SetGeomScreen(200);
    ctx.svs[0].vx = 100;
    ctx.svs[0].vy = 50;
    ctx.svs[0].vz = 500;
    ctx.RotTransPers();
    EXPECT_EQ((short)ctx.lgs[0], 199);
    EXPECT_EQ((short)(ctx.lgs[0] >> 16), 139);
}

TEST_F(gte_Test, rtps_division_overflow) {
    RTPContext ctx;
    ctx.SetRotM(0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000);
    ctx.SetTransM(0, 0, 0);
    SetGeomOffset(0, 0);
    SetGeomScreen(200);
    ctx.svs[0].vx = 100;
    ctx.svs[0].vy = 0;
    ctx.svs[0].vz = 1;
    ctx.RotTransPers();
    EXPECT_EQ((ctx.flag >> 17) & 1u, 1u);
}

TEST_F(gte_Test, rtps_screen_saturation) {
    RTPContext ctx;
    ctx.SetRotM(0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000);
    ctx.SetTransM(0, 0, 0);
    SetGeomOffset(0, 0);
    SetGeomScreen(200);
    ctx.svs[0].vx = 0x7FFF;
    ctx.svs[0].vy = 0;
    ctx.svs[0].vz = 100;
    ctx.RotTransPers();
    EXPECT_EQ((short)ctx.lgs[0], 0x3FF);
    EXPECT_EQ((ctx.flag >> 14) & 1u, 1u);
}

TEST_F(gte_Test, rtpt_three_vertices) {
    RTPContext ctx;
    ctx.SetRotM(0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000);
    ctx.SetTransM(0, 0, 0);
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
    ctx.RotTransPers3();
    EXPECT_EQ((short)ctx.lgs[0], 160);
    EXPECT_EQ((short)(ctx.lgs[0] >> 16), 120);
    EXPECT_EQ((short)(ctx.lgs[1] >> 16), 120);
}

TEST_F(gte_Test, rtpt_sz_fifo) {
    RTPContext ctx;
    ctx.SetRotM(0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000);
    ctx.SetTransM(0, 0, 0);
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
    long sz = ctx.RotTransPers3();
    EXPECT_EQ(sz, 300 >> 2);
}

TEST_F(gte_Test, rot_trans_pers_trans_matrix) {
    RTPContext ctx;

    SetGeomOffset(100, 100);
    ctx.SetTransM(0, 0, 0);
    ctx.TestRTP(0, SXY(100, 100), 0, 0x80021000);
    SetGeomOffset(0, 0);

    ctx.SetTransM(0, 0, 0);
    ctx.TestRTP(0, SXY(0, 0), 0, 0x80021000);

    ctx.SetTransM(10, 20, 0);
    ctx.TestRTP(0, SXY(19, 39), 0, 0x80021000);

    ctx.SetTransM(-10, -20, 0);
    ctx.TestRTP(0, SXY(-20, -40), 0, 0x80021000);

    // TODO SXY is clipped at abs(0x3FF) but there are no tests for that
}

TEST_F(gte_Test, rot_trans_pers_rot_matrix) {
    RTPContext ctx;
    SetGeomOffset(0, 0);

    ctx.SetRotM(MV(0, 0, 0), MV(0, 0, 0), MV(0, 0, 0));
    ctx.TestRTP(0, SXY(0, 0), 0, 0x80021000);

    ctx.SetRotM(MV(0x100, 0, 0), MV(0, 0x200, 0), MV(0, 0, 0x300));
    ctx.TestRTP(0, SXY(0, 0), 0, 0x80021000);
}

TEST_F(gte_Test, rot_trans_pers3_trans_matrix_perspective) {
    RTPContext ctx;

    ctx.SetTransM(0, 0, 0x40);
    ctx.TestRTP3(0x0010, 0, 0, 0, 0, 0x80021000);

    ctx.SetTransM(0, 0, -4);
    ctx.TestRTP3(0x0000, 0, 0, 0, 0, 0x80061000);

    ctx.SetTransM(0, 0, 0x1A36);
    ctx.TestRTP3(0x068D, 0, 0, 0, 0, 0x1000);
}

TEST_F(gte_Test, rot_trans_pers3_set_geom_offset) {
    RTPContext ctx;

    SetGeomOffset(1, 3);
    ctx.TestRTP3(0, 0x00030001, 0x00030001, 0x00030001, 0, 0x80021000);

    SetGeomOffset(0, 0);
    ctx.TestRTP3(0, 0x00000000, 0x00000000, 0x00000000, 0, 0x80021000);

    SetGeomOffset(0x3FF, 0x3FF);
    ctx.TestRTP3(0, 0x03FF03FF, 0x03FF03FF, 0x03FF03FF, 0, 0x80021000);

    SetGeomOffset(0x500, 0x100);
    ctx.TestRTP3(0, 0x010003FF, 0x010003FF, 0x010003FF, 0, 0x80025000);

    SetGeomOffset(0x100, 0x500);
    ctx.TestRTP3(0, 0x03FF0100, 0x03FF0100, 0x03FF0100, 0, 0x80023000);

    SetGeomOffset(0x600, 0x700);
    ctx.TestRTP3(0, 0x03FF03FF, 0x03FF03FF, 0x03FF03FF, 0, 0x80027000);

    SetGeomOffset(160, 120);
    ctx.TestRTP3(0, 0x007800A0, 0x007800A0, 0x007800A0, 0, 0x80021000);
}

TEST_F(gte_Test, rot_trans_pers3_trans_and_offset) {
    RTPContext ctx;

    SetGeomOffset(0, 0);
    ctx.SetTransM(10, 20, 0x100);
    ctx.TestRTP3(0x40, 0x00270013, 0x00270013, 0x00270013, 0, 0x80021000);

    SetGeomOffset(-31, -63);
    ctx.SetTransM(0x10, 0x20, 0x100);
    ctx.TestRTP3(0x40, 0x00000000, 0x00000000, 0x00000000, 0, 0x80021000);

    SetGeomOffset(100, 120);
    ctx.SetTransM(50, 60, 0x100);
    ctx.TestRTP3(0x40, 0x00EF00C7, 0x00EF00C7, 0x00EF00C7, 0, 0x80021000);

    SetGeomOffset(0x500, 0x400);
    ctx.SetTransM(100, 200, 0x100);
    ctx.TestRTP3(0x40, 0x03FF03FF, 0x03FF03FF, 0x03FF03FF, 0, 0x80027000);
}

TEST_F(gte_Test, rot_trans_pers3_rot_matrix) {
    RTPContext ctx;

    ctx.SetRotM(0x100, 0x100, 0x100, 0, 0x200, 0x200, 0x200, 0, 0xC00);
    ctx.SetSvs(0x10, 0x10, 0x10);
    ctx.TestRTP3(0x03, 0x00030001, 0x00030001, 0x00030001, 0, 0x80021000);

    ctx.SetRotM(0x100, 0x400, 0x100, 0, 0x200, 0x200, 0x200, 0, 0xC00);
    ctx.SetSvs(0x10, 0x10, 0x10);
    ctx.TestRTP3(0x03, 0x00030007, 0x00030007, 0x00030001, 0, 0x80021000);

    ctx.SetRotM(0x100, 0x100, 0x400, 0, 0x200, 0x200, 0x200, 0, 0xC00);
    ctx.SetSvs(0x10, 0x10, 0x10);
    ctx.TestRTP3(0x03, 0x00030001, 0x00030001, 0x00030007, 0, 0x80021000);

    ctx.SetRotM(0x100, 0x100, 0x100, 0, 0x400, 0x200, 0x200, 0, 0xC00);
    ctx.SetSvs(0x10, 0x10, 0x10);
    ctx.TestRTP3(0x03, 0x00070001, 0x00070001, 0x00030001, 0, 0x80021000);

    ctx.SetRotM(0x100, 0x100, 0x100, 0, 0x200, 0x400, 0x200, 0, 0xC00);
    ctx.SetSvs(0x10, 0x10, 0x10);
    ctx.TestRTP3(0x03, 0x00030001, 0x00030001, 0x00070001, 0, 0x80021000);

    ctx.SetRotM(0x100, 0x100, 0x100, 0xFFF, 0x200, 0x200, 0xFFF, 0xFFF, 0xC00);
    ctx.SetSvs(0x10, 0x10, 0x10);
    ctx.TestRTP3(0x03, 0x00030001, 0x00030001, 0x00030001, 0, 0x80021000);

    ctx.SetRotM(-0x100, -0x200, -0x400, 0, -0x800, -0x1000, 0, 0, 0xC00);
    ctx.SetSvs(-0x10, -0x10, -0x10);
    ctx.TestRTP3(0, 0x000F0003, 0x000F0003, 0x001F0007, 0, 0x80061000);

    ctx.SetRotM(0, 0, 0, 0, 0, 0, 0, 0, 0xC00);
    ctx.SetSvs(0, 0, -1);
    ctx.TestRTP3(0, 0, 0, 0, 0, 0x80061000);
}

TEST_F(gte_Test, rot_trans_pers_set_geom_offset) {
    RTPContext ctx;

    SetGeomOffset(1, 3);
    ctx.TestRTP(0, 0x00030001, 0, 0x80021000);

    SetGeomOffset(0, 0);
    ctx.TestRTP(0, 0x00000000, 0, 0x80021000);

    SetGeomOffset(0x3FF, 0x3FF);
    ctx.TestRTP(0, 0x03FF03FF, 0, 0x80021000);

    SetGeomOffset(0x500, 0x100);
    ctx.TestRTP(0, 0x010003FF, 0, 0x80025000);

    SetGeomOffset(0x100, 0x500);
    ctx.TestRTP(0, 0x03FF0100, 0, 0x80023000);

    SetGeomOffset(0x600, 0x700);
    ctx.TestRTP(0, 0x03FF03FF, 0, 0x80027000);

    SetGeomOffset(160, 120);
    ctx.TestRTP(0, 0x007800A0, 0, 0x80021000);
}

TEST_F(gte_Test, rot_trans_pers_trans_and_offset) {
    RTPContext ctx;

    SetGeomOffset(0, 0);
    ctx.SetTransM(10, 20, 0x100);
    ctx.TestRTP(0x40, 0x00270013, 0, 0x80021000);

    SetGeomOffset(-31, -63);
    ctx.SetTransM(0x10, 0x20, 0x100);
    ctx.TestRTP(0x40, 0x00000000, 0, 0x80021000);

    SetGeomOffset(100, 120);
    ctx.SetTransM(50, 60, 0x100);
    ctx.TestRTP(0x40, 0x00EF00C7, 0, 0x80021000);

    SetGeomOffset(0x500, 0x400);
    ctx.SetTransM(100, 200, 0x100);
    ctx.TestRTP(0x40, 0x03FF03FF, 0, 0x80027000);
}

TEST_F(gte_Test, rcos_rsin) {
    EXPECT_EQ(rcos(0x0000), 0x1000);
    EXPECT_EQ(rcos(0x0400), 0x0000);
    EXPECT_EQ(rcos(0x0800), -0x1000);
    EXPECT_EQ(rsin(0x0400), 0x1000);
    EXPECT_EQ(rcos(0x1000), rcos(0));
    EXPECT_EQ(rsin(0x1000), rsin(0));
}

TEST_F(gte_Test, rot_matrix_x) {
    MATRIX m = {0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000, 0, 0, 0};
    EXPECT_EQ(RotMatrixX(0x400, &m), &m);
    EXPECT_EQ(m.m[0][0], 0x1000);
    EXPECT_EQ(m.m[1][1], 0);
    EXPECT_EQ(m.m[1][2], -0x1000);
    EXPECT_EQ(m.m[2][1], 0x1000);
    EXPECT_EQ(m.m[2][2], 0);
}

TEST_F(gte_Test, rot_matrix_y) {
    MATRIX m = {0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000, 0, 0, 0};
    EXPECT_EQ(RotMatrixY(0x400, &m), &m);
    EXPECT_EQ(m.m[0][0], 0);
    EXPECT_EQ(m.m[0][2], 0x1000);
    EXPECT_EQ(m.m[1][1], 0x1000);
    EXPECT_EQ(m.m[2][0], -0x1000);
    EXPECT_EQ(m.m[2][2], 0);
}

TEST_F(gte_Test, rot_matrix_z) {
    MATRIX m = {0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000, 0, 0, 0};
    EXPECT_EQ(RotMatrixZ(0x400, &m), &m);
    EXPECT_EQ(m.m[0][0], 0);
    EXPECT_EQ(m.m[0][1], -0x1000);
    EXPECT_EQ(m.m[1][0], 0x1000);
    EXPECT_EQ(m.m[1][1], 0);
    EXPECT_EQ(m.m[2][2], 0x1000);
}

TEST_F(gte_Test, rot_matrix_arbitrary_angles) {
    MATRIX m = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    MATRIX exp = {+0x0212, +0x0061, +0x0FDC, //
                  -0x041C, -0x0F71, +0x00E8, //
                  +0x0F52, -0x0431, -0x01E7, //
                  10,      11,      12};
    SVECTOR sv = {0x123, 0x456, 0x789};
    EXPECT_EQ(RotMatrix(&sv, &m), &m);
    EqMatrix(&m, &exp);
}

TEST_F(gte_Test, rot_matrix_negative_and_wrapped_angles) {
    MATRIX m = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    MATRIX exp = {-0x0541, +0x0588, -0x0E10, //
                  +0x0D03, -0x05EB, -0x0730, //
                  -0x07B1, -0x0DCC, -0x0290, //
                  10,      11,      12};
    SVECTOR sv = {-0x321, 0x0ABC, -0x1DEF};
    EXPECT_EQ(RotMatrix(&sv, &m), &m);
    EqMatrix(&m, &exp);
}

TEST_F(gte_Test, rot_matrix_yxz_arbitrary_angles) {
    MATRIX m = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    MATRIX exp = {+0x0350, -0x0659, +0x0E4E, //
                  +0x029F, -0x0E32, -0x06E8, //
                  +0x0F6E, +0x03C6, -0x01E7, //
                  10,      11,      12};
    SVECTOR sv = {0x123, 0x456, 0x789};
    EXPECT_EQ(RotMatrixYXZ(&sv, &m), &m);
    EqMatrix(&m, &exp);
}

TEST_F(gte_Test, rot_matrix_yxz_negative_and_wrapped_angles) {
    MATRIX m = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    MATRIX exp = {+0x045A, +0x0EA7, -0x04B8, //
                  +0x03E4, +0x03B1, +0x0F13, //
                  +0x0EE4, -0x0542, -0x0290, //
                  10,      11,      12};
    SVECTOR sv = {-0x321, 0x0ABC, -0x1DEF};
    EXPECT_EQ(RotMatrixYXZ(&sv, &m), &m);
    EqMatrix(&m, &exp);
}

TEST_F(gte_Test, rot_matrix_yxz_axis_only) {
    MATRIX m = {0};
    SVECTOR sv = {0x400, 0, 0};
    RotMatrixYXZ(&sv, &m);
    EXPECT_EQ(m.m[1][2], -0x1000);
    EXPECT_EQ(m.m[0][2], 0);
    EXPECT_EQ(m.m[2][2], 0);
    EXPECT_EQ(m.m[0][0], 0x1000);
}

TEST_F(gte_Test, rot_matrix_x_arbitrary_angle) {
    MATRIX m = {+0x0F00, -0x0234, +0x0123, //
                +0x0456, +0x0E12, -0x0789, //
                -0x0321, +0x0654, +0x0FA0, //
                7,       8,       9};
    MATRIX exp = {+0x0F00, -0x0234, +0x0123, //
                  +0x0543, +0x09F6, -0x0D8B, //
                  -0x00F4, +0x0BC8, +0x0AD7, //
                  7,       8,       9};
    EXPECT_EQ(RotMatrixX(0x123, &m), &m);
    EqMatrix(&m, &exp);
}

TEST_F(gte_Test, rot_matrix_x_negative_angle) {
    MATRIX m = {+0x0F00, -0x0234, +0x0123, //
                +0x0456, +0x0E12, -0x0789, //
                -0x0321, +0x0654, +0x0FA0, //
                7,       8,       9};
    MATRIX exp = {+0x0F00, -0x0234, +0x0123, //
                  -0x04D6, -0x0CB0, +0x0A3F, //
                  +0x024A, -0x08C8, -0x0E00, //
                  7,       8,       9};
    EXPECT_EQ(RotMatrixX(-0x789, &m), &m);
    EqMatrix(&m, &exp);
}

TEST_F(gte_Test, rot_matrix_y_arbitrary_angle) {
    MATRIX m = {+0x0F00, -0x0234, +0x0123, //
                +0x0456, +0x0E12, -0x0789, //
                -0x0321, +0x0654, +0x0FA0, //
                7,       8,       9};
    MATRIX exp = {+0x0C2E, +0x00BE, +0x07C5, //
                  +0x0456, +0x0E12, -0x0789, //
                  -0x094D, +0x06A8, +0x0D9A, //
                  7,       8,       9};
    EXPECT_EQ(RotMatrixY(0x123, &m), &m);
    EqMatrix(&m, &exp);
}

TEST_F(gte_Test, rot_matrix_y_negative_angle) {
    MATRIX m = {+0x0F00, -0x0234, +0x0123, //
                +0x0456, +0x0E12, -0x0789, //
                -0x0321, +0x0654, +0x0FA0, //
                7,       8,       9};
    MATRIX exp = {-0x0E2F, +0x0104, -0x03F5, //
                  +0x0456, +0x0E12, -0x0789, //
                  +0x05CD, -0x06A0, -0x0F29, //
                  7,       8,       9};
    EXPECT_EQ(RotMatrixY(-0x789, &m), &m);
    EqMatrix(&m, &exp);
}

TEST_F(gte_Test, rot_matrix_z_arbitrary_angle) {
    MATRIX m = {+0x0F00, -0x0234, +0x0123, //
                +0x0456, +0x0E12, -0x0789, //
                -0x0321, +0x0654, +0x0FA0, //
                7,       8,       9};
    MATRIX exp = {+0x0BA8, -0x0810, +0x0447, //
                  +0x0A62, +0x0BBD, -0x064F, //
                  -0x0321, +0x0654, +0x0FA0, //
                  7,       8,       9};
    EXPECT_EQ(RotMatrixZ(0x123, &m), &m);
    EqMatrix(&m, &exp);
}

TEST_F(gte_Test, rot_matrix_z_negative_angle) {
    MATRIX m = {+0x0F00, -0x0234, +0x0123, //
                +0x0456, +0x0E12, -0x0789, //
                -0x0321, +0x0654, +0x0FA0, //
                7,       8,       9};
    MATRIX exp = {-0x0DF7, +0x04B8, -0x027D, //
                  -0x06FE, -0x0D70, +0x0734, //
                  -0x0321, +0x0654, +0x0FA0, //
                  7,       8,       9};
    EXPECT_EQ(RotMatrixZ(-0x789, &m), &m);
    EqMatrix(&m, &exp);
}

TEST_F(gte_Test, rot_matrix_angle_wraps_full_turn) {
    SVECTOR base = {0x123, 0x456, 0x789};
    SVECTOR wrapped = {0x123 + 0x1000, 0x456 - 0x1000, 0x789 + 0x2000};
    MATRIX a = {0}, b = {0};
    RotMatrix(&base, &a);
    RotMatrix(&wrapped, &b);
    EqMatrix(&a, &b);
    MATRIX c = {0}, d = {0};
    RotMatrixYXZ(&base, &c);
    RotMatrixYXZ(&wrapped, &d);
    EqMatrix(&c, &d);
}

TEST_F(gte_Test, rot_matrix_zero) {
    MATRIX m = {0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000, 0, 0, 0};
    MATRIX exp = {0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000, 0, 0, 0};
    RotMatrixX(0, &m);
    RotMatrixY(0, &m);
    RotMatrixZ(0, &m);
    EqMatrix(&m, &exp);
}

TEST_F(gte_Test, apply_matrix_identity) {
    MATRIX m = {0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000, 0, 0, 0};
    SVECTOR in = {100, -200, 300};
    VECTOR out = {0};
    ApplyMatrix(&m, &in, &out);
    EXPECT_EQ(out.vx, 100);
    EXPECT_EQ(out.vy, -200);
    EXPECT_EQ(out.vz, 300);
}

TEST_F(gte_Test, apply_matrix_permutation) {
    MATRIX m = {0, 0x1000, 0, 0, 0, 0x1000, 0x1000, 0, 0, 0, 0, 0};
    SVECTOR in = {1, 2, 3};
    VECTOR out = {0};
    ApplyMatrix(&m, &in, &out);
    EXPECT_EQ(out.vx, 2);
    EXPECT_EQ(out.vy, 3);
    EXPECT_EQ(out.vz, 1);
}

TEST_F(gte_Test, apply_matrix_ignores_translation) {
    MATRIX m = {0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000, 1000, 2000, 3000};
    SVECTOR in = {5, 6, 7};
    VECTOR out = {0};
    ApplyMatrix(&m, &in, &out);
    EXPECT_EQ(out.vx, 5);
    EXPECT_EQ(out.vy, 6);
    EXPECT_EQ(out.vz, 7);
}

TEST_F(gte_Test, rot_trans_applies_translation) {
    MATRIX m = {0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000, 10, 20, 30};
    SVECTOR in = {1, 2, 3};
    VECTOR out = {0};
    int flag = 0;
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    RotTrans(&in, &out, &flag);
    EXPECT_EQ(out.vx, 11);
    EXPECT_EQ(out.vy, 22);
    EXPECT_EQ(out.vz, 33);
}
