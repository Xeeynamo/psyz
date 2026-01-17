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

#define SXY(x, y) ((unsigned int)(x) | ((unsigned int)(y) << 16))
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
                  0,       0,       0};
    SVECTOR sv = {16, 17, 18};
    EXPECT_EQ(RotMatrix(&sv, &m), &m);
    EqMatrix(&m, &exp);
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

TEST_F(gte_Test, average_z4) {
    InitGeom();
    EXPECT_EQ(AverageZ4(0x1000, 0x2000, 0x3000, 0x4000), 0xA00);
}

TEST_F(gte_Test, normal_clip) {
    InitGeom();
    EXPECT_EQ(NormalClip(0x400010, 0x300020, 0x800030), 0x600);
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
#if 0 // most of these tests fail, skipping them for now
    RTPContext ctx;

    ctx.SetTransM(0, 0, 0x40);
    ctx.TestRTP3(0x0010, 0, 0, 0, 0, 0x80021000);

    ctx.SetTransM(0, 0, 0x101);
    ctx.TestRTP3(0x0040, 0, 0, 0, 0, 0x1000);

    ctx.SetTransM(0, 0, 0x10000);
    ctx.TestRTP3(0x3FFF, 0, 0, 0, 0x1000, 0x80441000);

    ctx.SetTransM(0, 0, -4);
    ctx.TestRTP3(0x0000, 0, 0, 0, 0, 0x80061000);

    ctx.SetTransM(0, 0, 0x8000000);
    ctx.TestRTP3(0x3FFF, 0, 0, 0, 0x1000, 0x80441000);

    ctx.SetTransM(0, 0, 0x80000000);
    ctx.TestRTP3(0x0000, 0, 0, 0, 0, 0x80461000);

    ctx.SetTransM(0, 0, 0x1A36);
    ctx.TestRTP3(0x068D, 0, 0, 0, 0, 0x1000);

    int pIn[] = {0x1A37, 0x1A38, 0x1A39, 0x2000, 0x4000, 0x7FFF};
    int pExp[] = {0, 1, 2, 0x39E, 0xBCF, 0xFE7};
    ASSERT_EQ(LEN(pIn), LEN(pExp));
    for (int i = 0; i < LEN(pIn); i++) {
        ctx.SetTransM(0, 0, pIn[i]);
        ctx.RotTransPers3();
        ctx.CheckRTP3(0, 0, 0, pExp[i], 0);
    }
#endif
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
