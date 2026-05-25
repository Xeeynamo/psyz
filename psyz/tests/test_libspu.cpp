#include <gtest/gtest.h>
extern "C" {
#include <psyz.h>
}

class spu_Test : public testing::Test {
  protected:
    void SetUp() override {
        Psyz_SpuInit();
        Psyz_SpuReset(0);
    }
};

TEST_F(spu_Test, SetTransferAddrMasksToRamRange) {
    Psyz_SpuSetTransferAddr(PSYZ_SPU_RAM_SIZE + 0x10);
    EXPECT_EQ(Psyz_SpuGetTransferAddr(), 0x10u);
}

TEST_F(spu_Test, WriteXferAddrRegSetsTransferAddr) {
    Psyz_SpuWrite(0x1A6, 0x0200);
    EXPECT_EQ(Psyz_SpuGetTransferAddr(), 0x1000u);
    EXPECT_EQ(Psyz_SpuRead(0x1A6), 0x0200);

    Psyz_SpuWrite(0x1A6, 0x0201);
    EXPECT_EQ(Psyz_SpuGetTransferAddr(), 0x1008u);
    EXPECT_EQ(Psyz_SpuRead(0x1A6), 0x0201);
}

TEST_F(spu_Test, SetTransferAddrWritesXferAddrReg) {
    Psyz_SpuSetTransferAddr(0x1000u);
    EXPECT_EQ(Psyz_SpuRead(0x1A6), 0x0200);

    Psyz_SpuSetTransferAddr(0x1008u);
    EXPECT_EQ(Psyz_SpuRead(0x1A6), 0x0201);
}

TEST_F(spu_Test, MemWriteAndReadByteForByte) {
    unsigned char payload[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    Psyz_SpuMemWrite(0x3000, payload, sizeof(payload));
    unsigned char buf[8] = {0};
    Psyz_SpuMemRead(0x3000, buf, sizeof(buf));
    for (size_t i = 0; i < sizeof(payload); i++) {
        EXPECT_EQ(buf[i], payload[i]) << "byte " << i;
    }
}

TEST_F(spu_Test, MemWriteDoesNotMoveFifoCursor) {
    Psyz_SpuSetTransferAddr(0x4000);
    unsigned char payload[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    Psyz_SpuMemWrite(0x8000, payload, sizeof(payload));
    EXPECT_EQ(Psyz_SpuGetTransferAddr(), 0x4000u);
    EXPECT_EQ(Psyz_SpuGetRam()[0x8000], 0xDE);
    EXPECT_EQ(Psyz_SpuGetRam()[0x8003], 0xEF);
}

TEST_F(spu_Test, MemReadWriteWrapsAtRamRange) {
    unsigned char payload[4] = {0x11, 0x22, 0x33, 0x44};
    Psyz_SpuMemWrite(PSYZ_SPU_RAM_SIZE - 2, payload, sizeof(payload));
    EXPECT_EQ(Psyz_SpuGetRam()[PSYZ_SPU_RAM_SIZE - 2], 0x11);
    EXPECT_EQ(Psyz_SpuGetRam()[PSYZ_SPU_RAM_SIZE - 1], 0x22);
    EXPECT_EQ(Psyz_SpuGetRam()[0], 0x33);
    EXPECT_EQ(Psyz_SpuGetRam()[1], 0x44);

    unsigned char buf[4] = {0};
    Psyz_SpuMemRead(PSYZ_SPU_RAM_SIZE - 2, buf, sizeof(buf));
    EXPECT_EQ(buf[0], 0x11);
    EXPECT_EQ(buf[1], 0x22);
    EXPECT_EQ(buf[2], 0x33);
    EXPECT_EQ(buf[3], 0x44);
}

TEST_F(spu_Test, FifoWrite) {
    Psyz_SpuSetTransferAddr(0x1000);
    EXPECT_EQ(Psyz_SpuGetTransferAddr(), 0x1000u);
    Psyz_SpuFifoWrite(0xDEAD);
    Psyz_SpuFifoWrite(0xBEEF);
    EXPECT_EQ(Psyz_SpuGetTransferAddr(), 0x1004u);
    EXPECT_EQ(Psyz_SpuGetRam()[0x1000], 0xAD);
    EXPECT_EQ(Psyz_SpuGetRam()[0x1001], 0xDE);
    EXPECT_EQ(Psyz_SpuGetRam()[0x1002], 0xEF);
    EXPECT_EQ(Psyz_SpuGetRam()[0x1003], 0xBE);
}

TEST_F(spu_Test, FifoWriteWrapsAtRamRange) {
    Psyz_SpuSetTransferAddr(PSYZ_SPU_RAM_SIZE - 2);
    Psyz_SpuFifoWrite(0xABCD);
    EXPECT_EQ(Psyz_SpuGetTransferAddr(), 0u);
    EXPECT_EQ(Psyz_SpuGetRam()[PSYZ_SPU_RAM_SIZE - 2], 0xCD);
    EXPECT_EQ(Psyz_SpuGetRam()[PSYZ_SPU_RAM_SIZE - 1], 0xAB);
}

TEST_F(spu_Test, ResetClearsRamUnlessHot) {
    Psyz_SpuMemWrite(0x100, "ABCD", 4);
    Psyz_SpuReset(0);
    EXPECT_EQ(Psyz_SpuGetRam()[0x100], 0);

    Psyz_SpuMemWrite(0x200, "WXYZ", 4);
    Psyz_SpuReset(1);
    EXPECT_EQ(Psyz_SpuGetRam()[0x200], 'W');
    EXPECT_EQ(Psyz_SpuGetRam()[0x203], 'Z');
}

TEST_F(spu_Test, RegWriteXferFifoDepositsAndAdvances) {
    Psyz_SpuWrite(0x1A6, 0x0100);
    Psyz_SpuWrite(0x1A8, 0xCAFE);
    EXPECT_EQ(Psyz_SpuGetRam()[0x800], 0xFE);
    EXPECT_EQ(Psyz_SpuGetRam()[0x801], 0xCA);
    EXPECT_EQ(Psyz_SpuGetTransferAddr(), 0x802u);
}

TEST_F(spu_Test, RegWritePureStorageRoundTrips) {
    Psyz_SpuWrite(0x050, 0x3FFF);
    EXPECT_EQ(Psyz_SpuRead(0x050), 0x3FFF);
    Psyz_SpuWrite(0x180, 0x4000);
    EXPECT_EQ(Psyz_SpuRead(0x180), 0x4000);
    Psyz_SpuWrite(0x1C0, 0x1234);
    EXPECT_EQ(Psyz_SpuRead(0x1C0), 0x1234);
}

TEST_F(spu_Test, RegWriteBulkUploadViaFifoMatchesPayload) {
    unsigned char payload[256];
    for (size_t i = 0; i < sizeof(payload); i++) {
        payload[i] = (unsigned char)((i * 13) ^ 0xA5);
    }
    Psyz_SpuWrite(0x1A6, 0x0080);
    for (size_t i = 0; i < sizeof(payload); i += 2) {
        Psyz_SpuWrite(
            0x1A8, (unsigned short)(payload[i] | (payload[i + 1] << 8)));
    }
    unsigned char buf[256];
    Psyz_SpuMemRead(0x400, buf, sizeof(buf));
    EXPECT_EQ(0, memcmp(buf, payload, sizeof(payload)));
    EXPECT_EQ(Psyz_SpuGetTransferAddr(), 0x400u + sizeof(payload));
}

TEST_F(spu_Test, MemWriteBulkDepositsAndAdvancesCursor) {
    Psyz_SpuSetTransferAddr(0x2000);
    unsigned char payload[6] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60};
    Psyz_SpuFifoWriteBulk(payload, sizeof(payload));
    EXPECT_EQ(Psyz_SpuGetTransferAddr(), 0x2000u + sizeof(payload));
    EXPECT_EQ(0, memcmp(&Psyz_SpuGetRam()[0x2000], payload, sizeof(payload)));
}

TEST_F(spu_Test, BulkUploadViaFifoMatchesDirectMemWrite) {
    unsigned char payload[1024];
    for (size_t i = 0; i < sizeof(payload); i++) {
        payload[i] = (unsigned char)(i * 7 + 13);
    }
    Psyz_SpuSetTransferAddr(0x10000);
    for (size_t i = 0; i < sizeof(payload); i += 2) {
        unsigned short w = (unsigned short)(payload[i] | (payload[i + 1] << 8));
        Psyz_SpuFifoWrite(w);
    }
    Psyz_SpuMemWrite(0x20000, payload, sizeof(payload));

    unsigned char a[1024];
    unsigned char b[1024];
    Psyz_SpuMemRead(0x10000, a, sizeof(a));
    Psyz_SpuMemRead(0x20000, b, sizeof(b));
    EXPECT_EQ(0, memcmp(a, b, sizeof(a)));
    EXPECT_EQ(0, memcmp(a, payload, sizeof(payload)));
}
