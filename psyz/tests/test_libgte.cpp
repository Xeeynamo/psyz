#include <gtest/gtest.h>
extern "C" {
#include "common.h"
#include "kernel.h"
#include "libgte.h"
}

class gte_Test : public testing::Test {
  protected:
    gte_Test() {}
    ~gte_Test() override = default;
    void SetUp() override {
    }
    void TearDown() override {
    }
    static void EqMatrix(MATRIX* m1, MATRIX* m2) {
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                EXPECT_EQ(m1->m[i][j], m2->m[i][j])
                    << "Matrix coefficient mismatch at m[" << i << "][" << j << "]";
            }
        }
        for (int i = 0; i < 3; i++) {
            EXPECT_EQ(m1->t[i], m2->t[i])
                << "Translation vector mismatch at t[" << i << "]";
        }
    }
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
    MATRIX m={0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    MATRIX exp={0, 1, 2, 3, 4, 5, 6, 7, 8, 16, 17, 18};
    VECTOR t={16, 17, 18};
    EXPECT_EQ(TransMatrix(&m, &t), &m);
    EqMatrix(&m, &exp);
}

TEST_F(gte_Test, rot_matrix) {
    MATRIX m={
        1, 2, 3, //
        4, 5, 6, //
        7, 8, 9, //
        10, 11, 12};
    MATRIX exp={
        +0x0FFD, -0x0071, +0x006B, //
        +0x0073, +0x0FFC, -0x0065, //
        -0x0069, +0x0067, +0x0FFE, //
        0, 0, 0};
    SVECTOR sv={16, 17, 18};
    EXPECT_EQ(RotMatrix(&sv, &m), &m);
    EqMatrix(&m, &exp);
}

extern "C" long MTC2_MFC2(long a) ;
TEST_F(gte_Test, mtc2_mfc2) {
    EXPECT_EQ(MTC2_MFC2(0), 0x20);
    EXPECT_EQ(MTC2_MFC2(1), 0x1F);
    EXPECT_EQ(MTC2_MFC2(2), 0x1E);
    EXPECT_EQ(MTC2_MFC2(4), 0x1D);
    EXPECT_EQ(MTC2_MFC2(8), 0x1C);
    EXPECT_EQ(MTC2_MFC2(9), 0x1C);
    EXPECT_EQ(MTC2_MFC2(0x10000), 15);
    EXPECT_EQ(MTC2_MFC2(0x10000000), 3);
    EXPECT_EQ(MTC2_MFC2(0x80000000), 1);
    EXPECT_EQ(MTC2_MFC2(0xF0000000), 4);
    EXPECT_EQ(MTC2_MFC2(-9), 0x1C);
    EXPECT_EQ(MTC2_MFC2(-8), 0x1D);
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
