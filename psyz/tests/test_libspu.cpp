#include <gtest/gtest.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
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

namespace {

// from PCSX Redux PR: https://github.com/grumpycoders/pcsx-redux/pull/2018
static const unsigned char kAdpcmSilent[64] = {
    0x00, 0x06, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0x00, 0x00, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0x00, 0x00, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0x00, 0x03, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
static const unsigned char kAdpcmSine[64] = {
    0x00, 0x06, 0x10, 0x43, 0x76, 0x77, 0x77, 0x46, 0x13, 0xe0, 0xbc,
    0x89, 0x88, 0x88, 0xb9, 0xec, 0x00, 0x00, 0x10, 0x43, 0x76, 0x77,
    0x77, 0x46, 0x13, 0xe0, 0xbc, 0x89, 0x88, 0x88, 0xb9, 0xec, 0x00,
    0x00, 0x10, 0x43, 0x76, 0x77, 0x77, 0x46, 0x13, 0xe0, 0xbc, 0x89,
    0x88, 0x88, 0xb9, 0xec, 0x00, 0x03, 0x10, 0x43, 0x76, 0x77, 0x77,
    0x46, 0x13, 0xe0, 0xbc, 0x89, 0x88, 0x88, 0xb9, 0xec,
};
static const unsigned char kAdpcmTriangle[64] = {
    0x00, 0x06, 0x10, 0x32, 0x54, 0x76, 0x56, 0x34, 0x12, 0xe0, 0xcd,
    0xab, 0x89, 0xa9, 0xcb, 0xed, 0x00, 0x00, 0x10, 0x32, 0x54, 0x76,
    0x56, 0x34, 0x12, 0xe0, 0xcd, 0xab, 0x89, 0xa9, 0xcb, 0xed, 0x00,
    0x00, 0x10, 0x32, 0x54, 0x76, 0x56, 0x34, 0x12, 0xe0, 0xcd, 0xab,
    0x89, 0xa9, 0xcb, 0xed, 0x00, 0x03, 0x10, 0x32, 0x54, 0x76, 0x56,
    0x34, 0x12, 0xe0, 0xcd, 0xab, 0x89, 0xa9, 0xcb, 0xed,
};
static const unsigned char kAdpcmSquare[64] = {
    0x00, 0x06, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x88, 0x88,
    0x88, 0x88, 0x88, 0x88, 0x88, 0x00, 0x00, 0x77, 0x77, 0x77, 0x77,
    0x77, 0x77, 0x77, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x00,
    0x00, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x88, 0x88, 0x88,
    0x88, 0x88, 0x88, 0x88, 0x00, 0x03, 0x77, 0x77, 0x77, 0x77, 0x77,
    0x77, 0x77, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
};
static const unsigned char kAdpcmSine394Hz[64] = {
    0x00, 0x06, 0x00, 0x10, 0x21, 0x32, 0x33, 0x44, 0x54, 0x55, 0x66,
    0x76, 0x77, 0x77, 0x77, 0x77, 0x00, 0x00, 0x77, 0x77, 0x77, 0x77,
    0x77, 0x66, 0x56, 0x55, 0x44, 0x34, 0x33, 0x22, 0x11, 0x00, 0x00,
    0x00, 0xf0, 0xef, 0xde, 0xcd, 0xcc, 0xbb, 0xab, 0xaa, 0x99, 0x89,
    0x88, 0x88, 0x88, 0x88, 0x00, 0x03, 0x88, 0x88, 0x88, 0x88, 0x88,
    0x99, 0xa9, 0xaa, 0xbb, 0xcb, 0xcc, 0xdd, 0xee, 0xff,
};
static const unsigned char kAdpcmSine5512Hz[64] = {
    0x00, 0x06, 0x50, 0x57, 0xa0, 0xa8, 0x50, 0x57, 0xa0, 0xa8, 0x50,
    0x57, 0xa0, 0xa8, 0x50, 0x57, 0x00, 0x00, 0xa0, 0xa8, 0x50, 0x57,
    0xa0, 0xa8, 0x50, 0x57, 0xa0, 0xa8, 0x50, 0x57, 0xa0, 0xa8, 0x00,
    0x00, 0x50, 0x57, 0xa0, 0xa8, 0x50, 0x57, 0xa0, 0xa8, 0x50, 0x57,
    0xa0, 0xa8, 0x50, 0x57, 0x00, 0x03, 0xa0, 0xa8, 0x50, 0x57, 0xa0,
    0xa8, 0x50, 0x57, 0xa0, 0xa8, 0x50, 0x57, 0xa0, 0xa8,
};

constexpr unsigned int kSampleAddr = 0x1080;

// One full voice-1 capture ring: 512 shorts mirrored to SPU RAM at 0x0800.
constexpr unsigned int kCaptureBytes = 1024;

// Envelope set with voice at full peak from sample 0
static void setup_voice1(unsigned int spu_addr, unsigned short pitch = 0x1000) {
    const unsigned int voice = 1;
    const unsigned int base = voice << 4;
    Psyz_SpuWrite(base + 0x04, pitch);
    Psyz_SpuWrite(base + 0x06, (unsigned short)(spu_addr >> 3));
    Psyz_SpuWrite(base + 0x08, 0x000f); // instant attack, sustain=0xF
    Psyz_SpuWrite(base + 0x0A, 0x1fc0); // sustain rate=0x7F linear
    Psyz_SpuWrite(base + 0x0E, (unsigned short)(spu_addr >> 3));
}

static void pull_samples_nop(int nframes) {
    std::vector<short> scratch(nframes * 2);
    Psyz_SpuPullSamples(scratch.data(), nframes);
}

// SPU enabled, all voices off, reverb off. Used before each ADPCM scenario so
// the capture ring starts clean.
static void spu_reset_quiet() {
    Psyz_SpuReset(0);
    Psyz_SpuWrite(0x1AA, 0);      // SPU_CTRL: disable
    Psyz_SpuWrite(0x180, 0);      // SPU_VOL_MAIN_LEFT
    Psyz_SpuWrite(0x182, 0);      // SPU_VOL_MAIN_RIGHT
    Psyz_SpuWrite(0x184, 0);      // SPU_REVERB_LEFT
    Psyz_SpuWrite(0x186, 0);      // SPU_REVERB_RIGHT
    Psyz_SpuWrite(0x18C, 0xFFFF); // KEY_OFF_LOW
    Psyz_SpuWrite(0x18E, 0xFFFF); // KEY_OFF_HIGH
    Psyz_SpuWrite(0x190, 0);      // PITCH_MOD_LOW
    Psyz_SpuWrite(0x192, 0);      // PITCH_MOD_HIGH
    Psyz_SpuWrite(0x194, 0);      // NOISE_EN_LOW
    Psyz_SpuWrite(0x196, 0);      // NOISE_EN_HIGH
    Psyz_SpuWrite(0x198, 0);      // REVERB_EN_LOW
    Psyz_SpuWrite(0x19A, 0);      // REVERB_EN_HIGH
    Psyz_SpuWrite(0x1A2, 0xFFFF); // REVERB_ADDR
    Psyz_SpuWrite(0x1AA, 0x8000); // SPU_CTRL: enable
}

static void spu_voice1_keyon(unsigned int spuAddr, unsigned short pitch) {
    setup_voice1(spuAddr, pitch);
    Psyz_SpuWrite(0x18C, 0);       // KEY_OFF_LOW
    Psyz_SpuWrite(0x18E, 0);       // KEY_OFF_HIGH
    Psyz_SpuWrite(0x188, 1u << 1); // KEY_ON voice 1
}

static void run_voice1_with_sample(
    const unsigned char* sample64, unsigned short pitch,
    unsigned char out_capture[kCaptureBytes]) {
    spu_reset_quiet();
    unsigned char upload[128];
    memcpy(upload, sample64, 64);
    memset(upload + 64, 0xAA, 64);
    Psyz_SpuMemWrite(kSampleAddr, upload, sizeof(upload));
    Psyz_SpuWrite(0x1AA, 0x8000 | 0x4000); // SPU enable + unmute
    Psyz_SpuWrite(0x180, 0x3FFF);
    Psyz_SpuWrite(0x182, 0x3FFF);

    spu_voice1_keyon(kSampleAddr, pitch);
    pull_samples_nop(512);

    Psyz_SpuMemRead(0x0800, out_capture, kCaptureBytes);
    Psyz_SpuWrite(0x18C, 0xFFFF);
    Psyz_SpuWrite(0x18E, 0xFFFF);
}

static std::vector<unsigned char> load_expected_pcm(const char* name) {
    std::string path = std::string("../expected/spu/") + name + ".test.pcm";
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        ADD_FAILURE() << "cannot open expected PCM file: " << path;
        return {};
    }
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<unsigned char> buf(n);
    if (std::fread(buf.data(), 1, n, f) != (size_t)n) {
        ADD_FAILURE() << "short read: " << path;
    }
    std::fclose(f);
    return buf;
}

static constexpr int SampleTolerance = 1500;

static bool samples_close(
    const unsigned char* a, const unsigned char* b, unsigned bytes) {
    for (unsigned i = 0; i + 1 < bytes; i += 2) {
        short av = (short)(a[i] | (a[i + 1] << 8));
        short bv = (short)(b[i] | (b[i + 1] << 8));
        int d = av - bv;
        if (d < -SampleTolerance || d > SampleTolerance) {
            return false;
        }
    }
    return true;
}

struct PcmTestHeader {
    uint32_t magic;  // 'PCMT' header magic code
    uint32_t length; // size of this header, for versioning or expandability
    uint32_t warmup; // ADPCM-decoded bytes with artifacts from previous run
    uint32_t period; // repeatable ADPCM-decoded bytes after warm-up
};

// Same warmup-skipping period-locating algorithm as pcsx-redux's
// spu_compare_golden(): the warmup prefix is non-deterministic (depends on
// the pipeline's state at KEY_ON) so we slide the period over the capture
// looking for a tolerance-close match, then verify periodicity from there.
static ::testing::AssertionResult compare_golden(
    const char* name, const unsigned char* cap,
    const std::vector<unsigned char>& gold) {
    if (gold.size() < sizeof(PcmTestHeader))
        return ::testing::AssertionFailure() << name << ": golden too small";
    PcmTestHeader h;
    memcpy(&h, gold.data(), sizeof(h));
    if (h.magic != 0x544D4350u)
        return ::testing::AssertionFailure()
               << name << ": bad magic 0x" << std::hex << h.magic;
    if (h.period == 0 || h.period > 1024)
        return ::testing::AssertionFailure()
               << name << ": bad period " << h.period;
    const unsigned char* period_start = gold.data() + h.length + h.warmup;

    uint32_t found_at = 0xFFFFFFFFu;
    for (uint32_t off = 0; off + h.period <= 1024; off += 2) {
        if (samples_close(cap + off, period_start, h.period)) {
            found_at = off;
            break;
        }
    }
    if (found_at == 0xFFFFFFFFu) {
        return ::testing::AssertionFailure()
               << name << ": period not found in capture (tol="
               << SampleTolerance << ")";
    }
    for (uint32_t i = found_at; i + 1 < 1024; i += 2) {
        uint32_t base = (i - found_at) % h.period;
        short av = (short)(cap[i] | (cap[i + 1] << 8));
        short ev = (short)(period_start[base] | (period_start[base + 1] << 8));
        int d = av - ev;
        if (d < -SampleTolerance || d > SampleTolerance) {
            return ::testing::AssertionFailure()
                   << name << ": periodicity broken at sample " << (i / 2)
                   << ": got " << av << ", want " << ev
                   << " (found_at=" << found_at << ")";
        }
    }
    return ::testing::AssertionSuccess();
}

#define SPU_EXPECT_GOLDEN(name, cap)                                           \
    EXPECT_TRUE(compare_golden(#name, (cap), load_expected_pcm(#name)))

} // namespace

TEST_F(spu_Test, adpcm_decode_silent) {
    // A silent ADPCM payload should produce a capture ring full of zeros
    unsigned char cap[1024];
    run_voice1_with_sample(kAdpcmSilent, 0x1000, cap);
    for (int i = 0; i < 1024; i++) {
        ASSERT_EQ(cap[i], 0) << "byte " << i;
    }
}

TEST_F(spu_Test, adpcm_decode_sinewave) {
    unsigned char cap[1024];
    run_voice1_with_sample(kAdpcmSine, 0x1000, cap);
    SPU_EXPECT_GOLDEN(sine, cap);
}

TEST_F(spu_Test, adpcm_decode_sinewave_lowpitch) {
    unsigned char cap[1024];
    run_voice1_with_sample(kAdpcmSine394Hz, 0x1000, cap);
    SPU_EXPECT_GOLDEN(sine_low, cap);
}

TEST_F(spu_Test, adpcm_decode_sinewave_highpitch) {
    unsigned char cap[1024];
    run_voice1_with_sample(kAdpcmSine5512Hz, 0x1000, cap);
    SPU_EXPECT_GOLDEN(sine_high, cap);
}

TEST_F(spu_Test, adpcm_decode_trianglewave) {
    unsigned char cap[1024];
    run_voice1_with_sample(kAdpcmTriangle, 0x1000, cap);
    SPU_EXPECT_GOLDEN(triangle, cap);
}

TEST_F(spu_Test, adpcm_decode_squarewave) {
    unsigned char cap[1024];
    run_voice1_with_sample(kAdpcmSquare, 0x1000, cap);
    SPU_EXPECT_GOLDEN(square, cap);
}

TEST_F(spu_Test, adpcm_decode_with_loop) {
    // Captures one full lap of triangle output. Then we key off, let the ring
    // drain, key on with the same payload and capture again.
    unsigned char cap[1024];
    run_voice1_with_sample(kAdpcmTriangle, 0x1000, cap);
    SPU_EXPECT_GOLDEN(loop_t0, cap);

    Psyz_SpuWrite(0x18C, 0xFFFF);
    Psyz_SpuWrite(0x18E, 0xFFFF);
    pull_samples_nop(512);

    spu_voice1_keyon(kSampleAddr, 0x1000);
    pull_samples_nop(512);
    Psyz_SpuMemRead(0x0800, cap, sizeof(cap));
    SPU_EXPECT_GOLDEN(loop_t1, cap);

    Psyz_SpuWrite(0x18C, 0xFFFF);
    Psyz_SpuWrite(0x18E, 0xFFFF);
}

// For pitch changes during voice on, enable vibrato or bends.
// This is used during the first five notes on FF7 Main Theme intro
TEST_F(spu_Test, ChangePitchWhileVoiceIsOn) {
    // self-looping ADPCM on block 0
    unsigned char payload[32];
    memset(payload, 0, sizeof(payload));
    payload[1] = 0x04;  // block 0: loop-start
    payload[17] = 0x03; // block 1: loop-end + repeat
    for (int i = 0; i < 14; i++)
        payload[18 + i] = 0x77;

    const unsigned int base = 1u << 4; // voice 1 register base

    spu_reset_quiet();
    Psyz_SpuWrite(0x1AA, 0x8000 | 0x4000);
    Psyz_SpuWrite(0x180, 0x3FFF);
    Psyz_SpuWrite(0x182, 0x3FFF);
    Psyz_SpuMemWrite(kSampleAddr, payload, sizeof(payload));
    spu_voice1_keyon(kSampleAddr, 0x0800); // set ADPCM pitch at 50%
    pull_samples_nop(256);
    unsigned char zeros[1024] = {0};
    Psyz_SpuMemWrite(0x0800, zeros, sizeof(zeros));
    pull_samples_nop(512);
    unsigned char cap_const[1024];
    Psyz_SpuMemRead(0x0800, cap_const, sizeof(cap_const));
    Psyz_SpuWrite(0x18C, 0xFFFF);
    Psyz_SpuWrite(0x18E, 0xFFFF);

    // modify pitch without resetting the voice key
    spu_reset_quiet();
    Psyz_SpuWrite(0x1AA, 0x8000 | 0x4000);
    Psyz_SpuWrite(0x180, 0x3FFF);
    Psyz_SpuWrite(0x182, 0x3FFF);
    Psyz_SpuMemWrite(kSampleAddr, payload, sizeof(payload));
    spu_voice1_keyon(kSampleAddr, 0x0800);
    pull_samples_nop(256);
    Psyz_SpuWrite(base + 0x04, 0x2000); // set ADPCM pitch at 200%
    Psyz_SpuMemWrite(0x0800, zeros, sizeof(zeros));
    pull_samples_nop(512);
    unsigned char cap_changed[1024];
    Psyz_SpuMemRead(0x0800, cap_changed, sizeof(cap_changed));
    Psyz_SpuWrite(0x18C, 0xFFFF);
    Psyz_SpuWrite(0x18E, 0xFFFF);

    // if done correctly, the two captures will differ; it's very hard to test
    // byte-by-byte here due to how gauss interpolation works, a memcpy will do
    EXPECT_NE(0, memcmp(cap_const, cap_changed, sizeof(cap_const)))
        << "mid-playback pitch write had no effect on voice output";
}

TEST_F(spu_Test, KeyOnLatchesStartAddrAndActivates) {
    Psyz_SpuMemWrite(kSampleAddr, kAdpcmSine, sizeof(kAdpcmSine));
    setup_voice1(kSampleAddr);
    // Before KEY_ON: voice 1 capture region stays zero through a pull.
    pull_samples_nop(28);
    short cap[28];
    Psyz_SpuMemRead(0x0800, cap, sizeof(cap));
    for (int i = 0; i < 28; i++) {
        EXPECT_EQ(cap[i], 0) << "pre-keyon sample " << i;
    }
    // KEY_ON, then pull. The capture_pos has advanced 28*2=56 bytes from
    // the first pull, so new samples land at 0x0800+56. Read the whole
    // ring (512 shorts = 1024 bytes) and assert *some* sample is non-zero.
    Psyz_SpuWrite(0x188, 1u << 1);
    pull_samples_nop(28);
    short ring[512];
    Psyz_SpuMemRead(0x0800, ring, sizeof(ring));
    int nonzero = 0;
    for (int i = 0; i < 512; i++) {
        if (ring[i] != 0)
            nonzero++;
    }
    EXPECT_GT(nonzero, 0);
}

TEST_F(spu_Test, KeyOffSilencesVoice) {
    Psyz_SpuMemWrite(kSampleAddr, kAdpcmSine, sizeof(kAdpcmSine));
    setup_voice1(kSampleAddr);
    Psyz_SpuWrite(0x188, 1u << 1);
    pull_samples_nop(56);
    Psyz_SpuWrite(0x18C, 1u << 1); // KEY_OFF voice 1
    // Zero out the capture region so we observe only post-keyoff writes.
    unsigned char zeros[128] = {0};
    Psyz_SpuMemWrite(0x0800, zeros, sizeof(zeros));
    pull_samples_nop(64);
    short cap[64];
    Psyz_SpuMemRead(0x0800, cap, sizeof(cap));
    for (int i = 0; i < 64; i++) {
        EXPECT_EQ(cap[i], 0) << "post-keyoff sample " << i;
    }
}

TEST_F(spu_Test, Bit11TogglesEveryHalfCaptureRing) {
    unsigned short s0 = Psyz_SpuRead(0x1AE) & 0x800;
    pull_samples_nop(256);
    unsigned short s1 = Psyz_SpuRead(0x1AE) & 0x800;
    EXPECT_NE(s0, s1);
    pull_samples_nop(256);
    unsigned short s2 = Psyz_SpuRead(0x1AE) & 0x800;
    EXPECT_EQ(s0, s2);
}

TEST_F(spu_Test, AdpcmLoopRepeatJumpsToLoopAddr) {
    // Two blocks: block 0 = silent (flag=0x04 = loop-start marker);
    // block 1 = non-zero (flag=0x03 = loop-end + repeat). After exhausting
    // block 1 the voice must jump back to block 0 (not stop). With the
    // gauss pipeline the first ~3 samples are pre-fill; thereafter the
    // capture should contain non-zero samples (from block 1) and the voice
    // must remain active across multiple lap-equivalent durations.
    unsigned char payload[32];
    memset(payload, 0, sizeof(payload));
    payload[0] = 0x00;
    payload[1] = 0x04; // block 0: silent, loop-start
    payload[16] = 0x00;
    payload[17] = 0x03;
    for (int i = 0; i < 14; i++)
        payload[16 + 2 + i] = 0x77;

    Psyz_SpuMemWrite(kSampleAddr, payload, sizeof(payload));
    setup_voice1(kSampleAddr);
    Psyz_SpuWrite(0x188, 1u << 1);

    pull_samples_nop(300); // pull as many samples necessary to trigger loop
    short ring[512];
    Psyz_SpuMemRead(0x0800, ring, sizeof(ring));
    int nonzero = 0;
    for (int i = 0; i < 300; i++)
        if (ring[i] != 0)
            nonzero++;
    EXPECT_GT(nonzero, 14) << "block 1 decoded samples never reached capture";

    // Verify voice keeps producing output
    unsigned char zeros[1024] = {0};
    Psyz_SpuMemWrite(0x0800, zeros, sizeof(zeros));
    pull_samples_nop(300);
    Psyz_SpuMemRead(0x0800, ring, sizeof(ring));
    int still_running = 0;
    for (int i = 0; i < 512; i++)
        if (ring[i] != 0)
            still_running++;
    EXPECT_GT(still_running, 0) << "voice stopped instead of looping";
}
