#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <psyz.h>
#include <libspu.h>

#include "ztest.h"

static void spu_setup(void) {
    Psyz_SpuInit();
    Psyz_SpuReset(0);
}

ZTEST_SETUP(spu) {
    zskip_targets("psp;ps1");
    spu_setup();
}

ZTEST(spu, SetTransferAddrMasksToRamRange) {
    Psyz_SpuSetTransferAddr(PSYZ_SPU_RAM_SIZE + 0x10);
    zexpect_u32_eq(0x10, Psyz_SpuGetTransferAddr());
}

ZTEST(spu, WriteXferAddrRegSetsTransferAddr) {
    Psyz_SpuWrite(0x1A6, 0x0200);
    zexpect_u32_eq(0x1000, Psyz_SpuGetTransferAddr());
    zexpect_u16_eq(0x0200, Psyz_SpuRead(0x1A6));

    Psyz_SpuWrite(0x1A6, 0x0201);
    zexpect_u32_eq(0x1008, Psyz_SpuGetTransferAddr());
    zexpect_u16_eq(0x0201, Psyz_SpuRead(0x1A6));
}

ZTEST(spu, SetTransferAddrWritesXferAddrReg) {
    Psyz_SpuSetTransferAddr(0x1000u);
    zexpect_u16_eq(0x0200, Psyz_SpuRead(0x1A6));

    Psyz_SpuSetTransferAddr(0x1008u);
    zexpect_u16_eq(0x0201, Psyz_SpuRead(0x1A6));
}

ZTEST(spu, MemWriteAndReadByteForByte) {
    unsigned char payload[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    unsigned char buf[8] = {0};
    size_t i;
    Psyz_SpuMemWrite(0x3000, payload, sizeof(payload));
    Psyz_SpuMemRead(0x3000, buf, sizeof(buf));
    for (i = 0; i < sizeof(payload); i++) {
        if (!zexpect_u8_eq(payload[i], buf[i])) {
            zprintf("byte %zu\n", i);
        }
    }
}

ZTEST(spu, MemWriteDoesNotMoveFifoCursor) {
    unsigned char payload[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    Psyz_SpuSetTransferAddr(0x4000);
    Psyz_SpuMemWrite(0x8000, payload, sizeof(payload));
    zexpect_u32_eq(0x4000, Psyz_SpuGetTransferAddr());
    zexpect_u8_eq(0xDE, Psyz_SpuGetRam()[0x8000]);
    zexpect_u8_eq(0xEF, Psyz_SpuGetRam()[0x8003]);
}

ZTEST(spu, MemReadWriteWrapsAtRamRange) {
    unsigned char payload[4] = {0x11, 0x22, 0x33, 0x44};
    unsigned char buf[4] = {0};
    Psyz_SpuMemWrite(PSYZ_SPU_RAM_SIZE - 2, payload, sizeof(payload));
    zexpect_u8_eq(0x11, Psyz_SpuGetRam()[PSYZ_SPU_RAM_SIZE - 2]);
    zexpect_u8_eq(0x22, Psyz_SpuGetRam()[PSYZ_SPU_RAM_SIZE - 1]);
    zexpect_u8_eq(0x33, Psyz_SpuGetRam()[0]);
    zexpect_u8_eq(0x44, Psyz_SpuGetRam()[1]);

    Psyz_SpuMemRead(PSYZ_SPU_RAM_SIZE - 2, buf, sizeof(buf));
    zexpect_u8_eq(0x11, buf[0]);
    zexpect_u8_eq(0x22, buf[1]);
    zexpect_u8_eq(0x33, buf[2]);
    zexpect_u8_eq(0x44, buf[3]);
}

ZTEST(spu, FifoWrite) {
    Psyz_SpuSetTransferAddr(0x1000);
    zexpect_u32_eq(0x1000, Psyz_SpuGetTransferAddr());
    Psyz_SpuFifoWrite(0xDEAD);
    Psyz_SpuFifoWrite(0xBEEF);
    zexpect_u32_eq(0x1004, Psyz_SpuGetTransferAddr());
    zexpect_u8_eq(0xAD, Psyz_SpuGetRam()[0x1000]);
    zexpect_u8_eq(0xDE, Psyz_SpuGetRam()[0x1001]);
    zexpect_u8_eq(0xEF, Psyz_SpuGetRam()[0x1002]);
    zexpect_u8_eq(0xBE, Psyz_SpuGetRam()[0x1003]);
}

ZTEST(spu, FifoWriteWrapsAtRamRange) {
    Psyz_SpuSetTransferAddr(PSYZ_SPU_RAM_SIZE - 2);
    Psyz_SpuFifoWrite(0xABCD);
    zexpect_u32_eq(0, Psyz_SpuGetTransferAddr());
    zexpect_u8_eq(0xCD, Psyz_SpuGetRam()[PSYZ_SPU_RAM_SIZE - 2]);
    zexpect_u8_eq(0xAB, Psyz_SpuGetRam()[PSYZ_SPU_RAM_SIZE - 1]);
}

ZTEST(spu, ResetClearsRamUnlessHot) {
    Psyz_SpuMemWrite(0x100, "ABCD", 4);
    Psyz_SpuReset(0);
    zexpect_u8_eq(0, Psyz_SpuGetRam()[0x100]);

    Psyz_SpuMemWrite(0x200, "WXYZ", 4);
    Psyz_SpuReset(1);
    zexpect_u8_eq('W', Psyz_SpuGetRam()[0x200]);
    zexpect_u8_eq('Z', Psyz_SpuGetRam()[0x203]);
}

ZTEST(spu, RegWriteXferFifoDepositsAndAdvances) {
    Psyz_SpuWrite(0x1A6, 0x0100);
    Psyz_SpuWrite(0x1A8, 0xCAFE);
    zexpect_u8_eq(0xFE, Psyz_SpuGetRam()[0x800]);
    zexpect_u8_eq(0xCA, Psyz_SpuGetRam()[0x801]);
    zexpect_u32_eq(0x802, Psyz_SpuGetTransferAddr());
}

ZTEST(spu, RegWritePureStorageRoundTrips) {
    Psyz_SpuWrite(0x050, 0x3FFF);
    zexpect_u16_eq(0x3FFF, Psyz_SpuRead(0x050));
    Psyz_SpuWrite(0x180, 0x4000);
    zexpect_u16_eq(0x4000, Psyz_SpuRead(0x180));
    Psyz_SpuWrite(0x1C0, 0x1234);
    zexpect_u16_eq(0x1234, Psyz_SpuRead(0x1C0));
}

ZTEST(spu, RegWriteBulkUploadViaFifoMatchesPayload) {
    unsigned char payload[256];
    unsigned char buf[256];
    size_t i;
    for (i = 0; i < sizeof(payload); i++) {
        payload[i] = (unsigned char)((i * 13) ^ 0xA5);
    }
    Psyz_SpuWrite(0x1A6, 0x0080);
    for (i = 0; i < sizeof(payload); i += 2) {
        Psyz_SpuWrite(
            0x1A8, (unsigned short)(payload[i] | (payload[i + 1] << 8)));
    }
    Psyz_SpuMemRead(0x400, buf, sizeof(buf));
    zexpect_u8array_eq(payload, buf, sizeof(payload));
    zexpect_u32_eq(0x400u + sizeof(payload), Psyz_SpuGetTransferAddr());
}

ZTEST(spu, MemWriteBulkDepositsAndAdvancesCursor) {
    unsigned char payload[6] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60};
    Psyz_SpuSetTransferAddr(0x2000);
    Psyz_SpuFifoWriteBulk(payload, sizeof(payload));
    zexpect_u32_eq(0x2000u + sizeof(payload), Psyz_SpuGetTransferAddr());
    zexpect_u8array_eq(payload, &Psyz_SpuGetRam()[0x2000], sizeof(payload));
}

ZTEST(spu, BulkUploadViaFifoMatchesDirectMemWrite) {
    unsigned char payload[1024];
    unsigned char a[1024];
    unsigned char b[1024];
    size_t i;
    for (i = 0; i < sizeof(payload); i++) {
        payload[i] = (unsigned char)(i * 7 + 13);
    }
    Psyz_SpuSetTransferAddr(0x10000);
    for (i = 0; i < sizeof(payload); i += 2) {
        unsigned short w = (unsigned short)(payload[i] | (payload[i + 1] << 8));
        Psyz_SpuFifoWrite(w);
    }
    Psyz_SpuMemWrite(0x20000, payload, sizeof(payload));

    Psyz_SpuMemRead(0x10000, a, sizeof(a));
    Psyz_SpuMemRead(0x20000, b, sizeof(b));
    zexpect_u8array_eq(b, a, sizeof(a));
    zexpect_u8array_eq(payload, a, sizeof(payload));
}

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

enum {
    kSampleAddr = 0x1080,
    // One full voice-1 capture ring: 512 shorts mirrored to SPU RAM at 0x0800.
    kCaptureBytes = 1024,
};

// Envelope set with voice at full peak from sample 0
static void setup_voice1(unsigned int spu_addr, unsigned short pitch) {
    const unsigned int voice = 1;
    const unsigned int base = voice << 4;
    Psyz_SpuWrite(base + 0x04, pitch);
    Psyz_SpuWrite(base + 0x06, (unsigned short)(spu_addr >> 3));
    Psyz_SpuWrite(base + 0x08, 0x000f); // instant attack, sustain=0xF
    Psyz_SpuWrite(base + 0x0A, 0x1fc0); // sustain rate=0x7F linear
    Psyz_SpuWrite(base + 0x0E, (unsigned short)(spu_addr >> 3));
}

static void pull_samples_nop(int nframes) {
    short* scratch = (short*)calloc((size_t)nframes * 2, sizeof(short));
    Psyz_SpuPullSamples(scratch, nframes);
    free(scratch);
}

// SPU enabled, all voices off, reverb off. Used before each ADPCM scenario so
// the capture ring starts clean.
static void spu_reset_quiet(void) {
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
    unsigned char upload[128];
    spu_reset_quiet();
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

// Returns a malloc'd buffer (NULL with *size 0 when the file cannot be opened).
static unsigned char* load_expected(
    const char* name, const char* ext, const char* what, size_t* size) {
    char path[256];
    FILE* f;
    long n;
    unsigned char* buf;
    snprintf(path, sizeof(path), "expected/spu/%s%s", name, ext);
    *size = 0;
    f = fopen(path, "rb");
    if (!f) {
        zfail("cannot open %s: %s", what, path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = (unsigned char*)calloc(n > 0 ? (size_t)n : 1, 1);
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
        zfail("short read: %s", path);
    }
    fclose(f);
    *size = (size_t)n;
    return buf;
}

enum { SampleTolerance = 1500 };

static bool samples_close(
    const unsigned char* a, const unsigned char* b, unsigned bytes) {
    unsigned i;
    for (i = 0; i + 1 < bytes; i += 2) {
        short av = (short)(a[i] | (a[i + 1] << 8));
        short bv = (short)(b[i] | (b[i + 1] << 8));
        int d = av - bv;
        if (d < -SampleTolerance || d > SampleTolerance) {
            return false;
        }
    }
    return true;
}

typedef struct {
    uint32_t magic;  // 'PCMT' header magic code
    uint32_t length; // size of this header, for versioning or expandability
    uint32_t warmup; // ADPCM-decoded bytes with artifacts from previous run
    uint32_t period; // repeatable ADPCM-decoded bytes after warm-up
} PcmTestHeader;

// Same warmup-skipping period-locating algorithm as pcsx-redux's
// spu_compare_golden(): the warmup prefix is non-deterministic (depends on
// the pipeline's state at KEY_ON) so we slide the period over the capture
// looking for a tolerance-close match, then verify periodicity from there.
static int compare_golden(const char* name, const unsigned char* cap,
                          const unsigned char* gold, size_t gold_size) {
    PcmTestHeader h;
    const unsigned char* period_start;
    uint32_t found_at = 0xFFFFFFFFu;
    uint32_t off, i;
    if (gold_size < sizeof(PcmTestHeader)) {
        zprintf("%s: golden too small\n", name);
        return 0;
    }
    memcpy(&h, gold, sizeof(h));
    if (h.magic != 0x544D4350u) {
        zprintf("%s: bad magic 0x%x\n", name, h.magic);
        return 0;
    }
    if (h.period == 0 || h.period > 1024) {
        zprintf("%s: bad period %u\n", name, h.period);
        return 0;
    }
    period_start = gold + h.length + h.warmup;

    for (off = 0; off + h.period <= 1024; off += 2) {
        if (samples_close(cap + off, period_start, h.period)) {
            found_at = off;
            break;
        }
    }
    if (found_at == 0xFFFFFFFFu) {
        zprintf("%s: period not found in capture (tol=%d)\n", name,
                SampleTolerance);
        return 0;
    }
    for (i = found_at; i + 1 < 1024; i += 2) {
        uint32_t base = (i - found_at) % h.period;
        short av = (short)(cap[i] | (cap[i + 1] << 8));
        short ev = (short)(period_start[base] | (period_start[base + 1] << 8));
        int d = av - ev;
        if (d < -SampleTolerance || d > SampleTolerance) {
            zprintf("%s: periodicity broken at sample %u: got %d, want %d "
                    "(found_at=%u)\n",
                    name, i / 2, av, ev, found_at);
            return 0;
        }
    }
    return 1;
}

static int check_golden(const char* name, const unsigned char* cap) {
    size_t size;
    unsigned char* gold =
        load_expected(name, ".test.pcm", "expected PCM file", &size);
    int ok = compare_golden(name, cap, gold, size);
    free(gold);
    return ok;
}

#define SPU_EXPECT_GOLDEN(name, cap)                                           \
    zexpect_s32_eq(1, check_golden(#name, (cap)))

ZTEST(spu, adpcm_decode_silent) {
    // A silent ADPCM payload should produce a capture ring full of zeros
    unsigned char cap[1024];
    int i;
    run_voice1_with_sample(kAdpcmSilent, 0x1000, cap);
    for (i = 0; i < 1024; i++) {
        if (cap[i] != 0) {
            zprintf("byte %d\n", i);
        }
        zassert_u8_eq(0, cap[i]);
    }
}

ZTEST(spu, adpcm_decode_sinewave) {
    unsigned char cap[1024];
    run_voice1_with_sample(kAdpcmSine, 0x1000, cap);
    SPU_EXPECT_GOLDEN(sine, cap);
}

ZTEST(spu, adpcm_decode_sinewave_lowpitch) {
    unsigned char cap[1024];
    run_voice1_with_sample(kAdpcmSine394Hz, 0x1000, cap);
    SPU_EXPECT_GOLDEN(sine_low, cap);
}

ZTEST(spu, adpcm_decode_sinewave_highpitch) {
    unsigned char cap[1024];
    run_voice1_with_sample(kAdpcmSine5512Hz, 0x1000, cap);
    SPU_EXPECT_GOLDEN(sine_high, cap);
}

ZTEST(spu, adpcm_decode_trianglewave) {
    unsigned char cap[1024];
    run_voice1_with_sample(kAdpcmTriangle, 0x1000, cap);
    SPU_EXPECT_GOLDEN(triangle, cap);
}

ZTEST(spu, adpcm_decode_squarewave) {
    unsigned char cap[1024];
    run_voice1_with_sample(kAdpcmSquare, 0x1000, cap);
    SPU_EXPECT_GOLDEN(square, cap);
}

ZTEST(spu, adpcm_decode_with_loop) {
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
ZTEST(spu, ChangePitchWhileVoiceIsOn) {
    // self-looping ADPCM on block 0
    unsigned char payload[32];
    unsigned char zeros[1024] = {0};
    unsigned char cap_const[1024];
    unsigned char cap_changed[1024];
    const unsigned int base = 1u << 4; // voice 1 register base
    int i;
    memset(payload, 0, sizeof(payload));
    payload[1] = 0x04;  // block 0: loop-start
    payload[17] = 0x03; // block 1: loop-end + repeat
    for (i = 0; i < 14; i++)
        payload[18 + i] = 0x77;

    spu_reset_quiet();
    Psyz_SpuWrite(0x1AA, 0x8000 | 0x4000);
    Psyz_SpuWrite(0x180, 0x3FFF);
    Psyz_SpuWrite(0x182, 0x3FFF);
    Psyz_SpuMemWrite(kSampleAddr, payload, sizeof(payload));
    spu_voice1_keyon(kSampleAddr, 0x0800); // set ADPCM pitch at 50%
    pull_samples_nop(256);
    Psyz_SpuMemWrite(0x0800, zeros, sizeof(zeros));
    pull_samples_nop(512);
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
    Psyz_SpuMemRead(0x0800, cap_changed, sizeof(cap_changed));
    Psyz_SpuWrite(0x18C, 0xFFFF);
    Psyz_SpuWrite(0x18E, 0xFFFF);

    // if done correctly, the two captures will differ; it's very hard to test
    // byte-by-byte here due to how gauss interpolation works, a memcpy will do
    if (!zexpect_u8array_ne(cap_const, cap_changed, sizeof(cap_const))) {
        zprintf("mid-playback pitch write had no effect on voice output\n");
    }
}

// Peak absolute amplitude of the left and right channels over `nframes` of the
// final stereo mix that Psyz_SpuPullSamples produces (NOT the per-voice capture
// buffer, which is pre-volume). out_l/out_r receive the per-channel peaks.
static void mix_peak(int nframes, int* out_l, int* out_r) {
    short* buf = (short*)calloc((size_t)nframes * 2, sizeof(short));
    int pl = 0, pr = 0;
    int i;
    Psyz_SpuPullSamples(buf, nframes);
    for (i = 0; i < nframes; i++) {
        int l = abs((int)buf[i * 2]);
        int r = abs((int)buf[i * 2 + 1]);
        if (l > pl)
            pl = l;
        if (r > pr)
            pr = r;
    }
    free(buf);
    *out_l = pl;
    *out_r = pr;
}

// given a sine wave and L/R volume, get out peak volume out for L/R capture
static void voice1_volume_peak(
    unsigned short vol_l, unsigned short vol_r, int* peak_l, int* peak_r) {
    const unsigned int base = 1u << 4; // voice 1 register base
    spu_reset_quiet();
    Psyz_SpuMemWrite(kSampleAddr, kAdpcmSine, sizeof(kAdpcmSine));
    Psyz_SpuWrite(0x1AA, 0x8000 | 0x4000); // SPU enable + unmute
    Psyz_SpuWrite(0x180, 0x3FFF);          // main volume left = unity
    Psyz_SpuWrite(0x182, 0x3FFF);          // main volume right = unity
    Psyz_SpuWrite(base + 0x00, vol_l);     // voice 1 volume left
    Psyz_SpuWrite(base + 0x02, vol_r);     // voice 1 volume right
    spu_voice1_keyon(kSampleAddr, 0x1000);
    // Skip the key-on envelope delay, then measure a steady window.
    pull_samples_nop(32);
    mix_peak(256, peak_l, peak_r);
    Psyz_SpuWrite(0x18C, 0xFFFF);
    Psyz_SpuWrite(0x18E, 0xFFFF);
}

ZTEST(spu, VoiceVolumePansLeftAndRight) {
    int l, r;

    voice1_volume_peak(0x3FFF, 0x0000, &l, &r);
    if (!zexpect_s32_gt(1000, l)) {
        zprintf("hard-left voice produced no left output\n");
    }
    if (!zexpect_s32_eq(0, r)) {
        zprintf("hard-left voice leaked into the right channel\n");
    }

    voice1_volume_peak(0x0000, 0x3FFF, &l, &r);
    if (!zexpect_s32_gt(1000, r)) {
        zprintf("hard-right voice produced no right output\n");
    }
    if (!zexpect_s32_eq(0, l)) {
        zprintf("hard-right voice leaked into the left channel\n");
    }
}

ZTEST(spu, VoiceVolumeScalesAmplitude) {
    int full_l, full_r, half_l, half_r;
    double ratio;
    voice1_volume_peak(0x3FFF, 0x3FFF, &full_l, &full_r);
    voice1_volume_peak(0x2000, 0x2000, &half_l, &half_r);

    zassert_s32_gt(0, full_l);
    // 0x2000 / 0x3FFF ~= 0.5; allow a generous band for rounding and the
    // gauss-interpolated sample peak landing on different frames.
    ratio = (double)half_l / (double)full_l;
    if (!zexpect_s32_ne(0, ratio > 0.35)) {
        zprintf("half volume too quiet (ratio %f)\n", ratio);
    }
    if (!zexpect_s32_ne(0, ratio < 0.65)) {
        zprintf("half volume too loud (ratio %f)\n", ratio);
    }
}

ZTEST(spu, VoiceVolumeZeroIsSilent) {
    int l, r;
    voice1_volume_peak(0x0000, 0x0000, &l, &r);
    zexpect_s32_eq(0, l);
    zexpect_s32_eq(0, r);
}

ZTEST(spu, KeyOnLatchesStartAddrAndActivates) {
    short cap[28];
    short ring[512];
    int nonzero = 0;
    int i;
    Psyz_SpuMemWrite(kSampleAddr, kAdpcmSine, sizeof(kAdpcmSine));
    setup_voice1(kSampleAddr, 0x1000);
    // Before KEY_ON: voice 1 capture region stays zero through a pull.
    pull_samples_nop(28);
    Psyz_SpuMemRead(0x0800, cap, sizeof(cap));
    for (i = 0; i < 28; i++) {
        if (!zexpect_s16_eq(0, cap[i])) {
            zprintf("pre-keyon sample %d\n", i);
        }
    }
    // KEY_ON, then pull. The capture_pos has advanced 28*2=56 bytes from
    // the first pull, so new samples land at 0x0800+56. Read the whole
    // ring (512 shorts = 1024 bytes) and assert *some* sample is non-zero.
    Psyz_SpuWrite(0x188, 1u << 1);
    pull_samples_nop(28);
    Psyz_SpuMemRead(0x0800, ring, sizeof(ring));
    for (i = 0; i < 512; i++) {
        if (ring[i] != 0)
            nonzero++;
    }
    zexpect_s32_gt(0, nonzero);
}

ZTEST(spu, KeyOffSilencesVoice) {
    // TODO this test has the stange consequence where voice_envelope_step must
    // be called before the samples are captured, despite the capture itself
    // not being afected by the voice envelope. It seems to match the generated
    // samples, but it needs to be double-checked on real hardware.
    unsigned char zeros[128] = {0};
    short cap[64];
    int i;
    Psyz_SpuMemWrite(kSampleAddr, kAdpcmSine, sizeof(kAdpcmSine));
    setup_voice1(kSampleAddr, 0x1000);
    Psyz_SpuWrite(0x188, 1u << 1);
    pull_samples_nop(56);
    Psyz_SpuWrite(0x18C, 1u << 1); // KEY_OFF voice 1
    // Zero out the capture region so we observe only post-keyoff writes.
    Psyz_SpuMemWrite(0x0800, zeros, sizeof(zeros));
    pull_samples_nop(64);
    Psyz_SpuMemRead(0x0800, cap, sizeof(cap));
    for (i = 0; i < 64; i++) {
        if (!zexpect_s16_eq(0, cap[i])) {
            zprintf("post-keyoff sample %d\n", i);
        }
    }
}

ZTEST(spu, Bit11TogglesEveryHalfCaptureRing) {
    unsigned short s0, s1, s2;
    s0 = Psyz_SpuRead(0x1AE) & 0x800;
    pull_samples_nop(256);
    s1 = Psyz_SpuRead(0x1AE) & 0x800;
    zexpect_u16_ne(s0, s1);
    pull_samples_nop(256);
    s2 = Psyz_SpuRead(0x1AE) & 0x800;
    zexpect_u16_eq(s0, s2);
}

ZTEST(spu, AdpcmLoopRepeatJumpsToLoopAddr) {
    // Two blocks: block 0 = silent (flag=0x04 = loop-start marker);
    // block 1 = non-zero (flag=0x03 = loop-end + repeat). After exhausting
    // block 1 the voice must jump back to block 0 (not stop). With the
    // gauss pipeline the first ~3 samples are pre-fill; thereafter the
    // capture should contain non-zero samples (from block 1) and the voice
    // must remain active across multiple lap-equivalent durations.
    unsigned char payload[32];
    unsigned char zeros[1024] = {0};
    short ring[512];
    int nonzero = 0;
    int still_running = 0;
    int i;
    memset(payload, 0, sizeof(payload));
    payload[0] = 0x00;
    payload[1] = 0x04; // block 0: silent, loop-start
    payload[16] = 0x00;
    payload[17] = 0x03;
    for (i = 0; i < 14; i++)
        payload[16 + 2 + i] = 0x77;

    Psyz_SpuMemWrite(kSampleAddr, payload, sizeof(payload));
    setup_voice1(kSampleAddr, 0x1000);
    Psyz_SpuWrite(0x188, 1u << 1);

    pull_samples_nop(300); // pull as many samples necessary to trigger loop
    Psyz_SpuMemRead(0x0800, ring, sizeof(ring));
    for (i = 0; i < 300; i++)
        if (ring[i] != 0)
            nonzero++;
    if (!zexpect_s32_gt(14, nonzero)) {
        zprintf("block 1 decoded samples never reached capture\n");
    }

    // Verify voice keeps producing output
    Psyz_SpuMemWrite(0x0800, zeros, sizeof(zeros));
    pull_samples_nop(300);
    Psyz_SpuMemRead(0x0800, ring, sizeof(ring));
    for (i = 0; i < 512; i++)
        if (ring[i] != 0)
            still_running++;
    if (!zexpect_s32_gt(0, still_running)) {
        zprintf("voice stopped instead of looping\n");
    }
}

#define ADSR_ATTACK(step, shift, exp)                                          \
    ((((step) & 3) << 8) | (((shift) & 31) << 10) | (!!(exp) << 15))
#define ADSR_DECAY(shift) (((shift) & 15) << 4)
#define ADSR_SUSTAIN(step, shift, level, direction, exp)                       \
    ((((step) & 3) << 22) | (((shift) & 31) << 24) | (((level) & 15) << 0) |   \
     (!!(direction) << 30) | (!!(exp) << 31))
#define ADSR_RELEASE(shift, exp) ((((shift) & 31) << 16) | (!!(exp) << 21))

enum { kAdsrSampleAddr = 0x1080 };

// A 2-block looping ADPCM payload so voice 1 never stops while the envelope
// runs (block 1 loops back to block 0). The decoded samples are irrelevant;
// the tests only read the envelope level.
static void adsr_upload_loop_sample(void) {
    unsigned char payload[32];
    int i;
    memset(payload, 0, sizeof(payload));
    payload[1] = 0x04;  // block 0: loop-start
    payload[17] = 0x03; // block 1: loop-end + repeat
    for (i = 0; i < 14; i++)
        payload[18 + i] = 0x55;
    Psyz_SpuMemWrite(kAdsrSampleAddr, payload, sizeof(payload));
}

static unsigned short adsr_status(void) { return Psyz_SpuRead(0x1AE); }
static unsigned short adsr_envx1(void) { return Psyz_SpuRead(0x1C); }

static void adsr_tick(void) {
    short frame[2];
    Psyz_SpuPullSamples(frame, 1);
}

static void adsr_wait_bit11_flip(void) {
    int guard;
    for (guard = 0; !(adsr_status() & 0x0800); guard++) {
        adsr_tick();
        if (guard >= 4096) {
            zfail("bit11 never went high (guard %d)", guard);
            return;
        }
    }
    for (guard = 0; adsr_status() & 0x0800; guard++) {
        adsr_tick();
        if (guard >= 4096) {
            zfail("bit11 never went low (guard %d)", guard);
            return;
        }
    }
}

// Capture n_samples ENVX readings for `adsr`, one per bit-11 flip.
static void adsr_capture(uint32_t adsr, uint16_t* envx, unsigned n_samples) {
    int guard;
    unsigned i;
    // Drain any previous envelope back to zero.
    spu_reset_quiet();
    Psyz_SpuWrite(0x1AA, 0x8000 | 0x4000);
    Psyz_SpuWrite(0x180, 0);
    Psyz_SpuWrite(0x182, 0);
    Psyz_SpuWrite(0x18C, 0xFFFF);
    Psyz_SpuWrite(0x18E, 0xFFFF);
    adsr_wait_bit11_flip();
    guard = 0;
    while (adsr_envx1() != 0) {
        adsr_tick();
        if (++guard >= 200000) {
            zfail(
                "previous envelope never went back to zero (guard %d)", guard);
            return;
        }
    }

    // Prepare voice 1, wait for a flip, then fire it.
    adsr_upload_loop_sample();
    Psyz_SpuWrite(0x14, 0x1000);                                 // sampleRate
    Psyz_SpuWrite(0x16, (unsigned short)(kAdsrSampleAddr >> 3)); // startAddr
    Psyz_SpuWrite(0x1E, (unsigned short)(kAdsrSampleAddr >> 3)); // repeatAddr
    Psyz_SpuWrite(0x10, 0);                                      // volumeLeft
    Psyz_SpuWrite(0x12, 0);                                      // volumeRight
    adsr_wait_bit11_flip();
    Psyz_SpuWrite(0x18, (unsigned short)(adsr & 0xFFFF));
    Psyz_SpuWrite(0x1A, (unsigned short)(adsr >> 16));
    Psyz_SpuWrite(0x18C, 0);
    Psyz_SpuWrite(0x18E, 0);
    Psyz_SpuWrite(0x188, 1u << 1); // KEY_ON voice 1

    for (guard = 0; adsr_envx1() == 0; guard++) {
        adsr_tick();
        if (guard >= 100000) {
            zfail("envelope never started after key-on (guard %d)", guard);
            return;
        }
    }
    envx[0] = adsr_envx1();
    for (i = 1; i < n_samples; i++) {
        adsr_wait_bit11_flip();
        envx[i] = adsr_envx1();
    }
    Psyz_SpuWrite(0x18C, 0xFFFF);
    Psyz_SpuWrite(0x18E, 0xFFFF);
}

static void adsr_capture_with_keyoff(
    uint32_t adsr, uint16_t* envx, unsigned n_samples, unsigned keyoff_at) {
    unsigned i;
    adsr_capture(adsr, envx, keyoff_at + 1);
    for (i = keyoff_at + 1; i < n_samples; i++) {
        adsr_wait_bit11_flip();
        envx[i] = adsr_envx1();
    }
}

#define EXPECT_ENVX_NEAR(nominal, step, got)                                   \
    do {                                                                       \
        int ok_ = (got) >= (uint16_t)((nominal) - (step)) &&                   \
                  (got) <= (uint16_t)((nominal) + (step));                     \
        if (!zexpect_s32_ne(0, ok_)) {                                         \
            zprintf("envx 0x%x not within %d of 0x%x\n", (unsigned)(got),      \
                    (int)(step), (unsigned)(nominal));                         \
        }                                                                      \
    } while (0)

ZTEST(spu, adsr_attack_linear_step) {
    uint16_t envx[0x40];
    const uint32_t base =
        ADSR_DECAY(0) | ADSR_SUSTAIN(3, 0x1f, 15, 0, 0) | ADSR_RELEASE(0, 0);
    int i;

    adsr_capture(ADSR_ATTACK(2, 12, 0) | base, envx, 4);
    zexpect_u16_eq(0x0005, envx[0]);
    EXPECT_ENVX_NEAR(0x04f1, 5, envx[1]);
    EXPECT_ENVX_NEAR(0x09f1, 5, envx[2]);
    EXPECT_ENVX_NEAR(0x0ef1, 5, envx[3]);

    adsr_capture(ADSR_ATTACK(3, 12, 0) | base, envx, 4);
    zexpect_u16_eq(0x0004, envx[0]);
    EXPECT_ENVX_NEAR(0x03f4, 4, envx[1]);
    EXPECT_ENVX_NEAR(0x07f4, 4, envx[2]);
    EXPECT_ENVX_NEAR(0x0bf4, 4, envx[3]);

    adsr_capture(ADSR_ATTACK(2, 24, 0) | base, envx, 48);
    for (i = 0; i < 17; i++)
        zexpect_u16_eq(0x0005, envx[i]);
    for (i = 17; i < 33; i++)
        zexpect_u16_eq(0x000a, envx[i]);
    for (i = 33; i < 48; i++)
        zexpect_u16_eq(0x000f, envx[i]);

    adsr_capture(ADSR_ATTACK(3, 24, 0) | base, envx, 48);
    for (i = 0; i < 17; i++)
        zexpect_u16_eq(0x0004, envx[i]);
    for (i = 17; i < 33; i++)
        zexpect_u16_eq(0x0008, envx[i]);
    for (i = 33; i < 48; i++)
        zexpect_u16_eq(0x000c, envx[i]);
}

ZTEST(spu, adsr_attack_linear_shift) {
    uint16_t envx[0x40];
    const uint32_t base =
        ADSR_DECAY(0) | ADSR_SUSTAIN(3, 0x1f, 15, 0, 0) | ADSR_RELEASE(0, 0);
    static const uint16_t s012[] = {
        0x06e4, 0x0de4, 0x14e4, 0x1be4, 0x22e4, 0x29e4, 0x30e4, 0x37e4, 0x3ee4,
        0x45e4, 0x4ce4, 0x53e4, 0x5ae4, 0x61e4, 0x68e4, 0x6fe4, 0x76e4, 0x7de4};
    int i;

    adsr_capture(ADSR_ATTACK(0, 0, 0) | base, envx, 1);
    zexpect_u16_eq(0x3800, envx[0]);
    adsr_capture(ADSR_ATTACK(1, 0, 0) | base, envx, 1);
    zexpect_u16_eq(0x3000, envx[0]);

    adsr_capture(ADSR_ATTACK(0, 11, 0) | base, envx, 4);
    zexpect_u16_eq(0x0007, envx[0]);
    EXPECT_ENVX_NEAR(0x0dcf, 7, envx[1]);
    EXPECT_ENVX_NEAR(0x1bcf, 7, envx[2]);
    EXPECT_ENVX_NEAR(0x29cf, 7, envx[3]);

    adsr_capture(ADSR_ATTACK(1, 11, 0) | base, envx, 4);
    zexpect_u16_eq(0x0006, envx[0]);
    EXPECT_ENVX_NEAR(0x0bdc, 7, envx[1]);
    EXPECT_ENVX_NEAR(0x17d6, 7, envx[2]);
    EXPECT_ENVX_NEAR(0x23d6, 7, envx[3]);

    adsr_capture(ADSR_ATTACK(0, 12, 0) | base, envx, 32);
    zexpect_u16_eq(0x0007, envx[0]);
    for (i = 0; i < 18; i++)
        EXPECT_ENVX_NEAR(s012[i], 7, envx[i + 1]);

    adsr_capture(ADSR_ATTACK(1, 12, 0) | base, envx, 4);
    zexpect_u16_eq(0x0006, envx[0]);
    EXPECT_ENVX_NEAR(0x05ee, 6, envx[1]);
    EXPECT_ENVX_NEAR(0x0bee, 6, envx[2]);
    EXPECT_ENVX_NEAR(0x11ee, 6, envx[3]);

    adsr_capture(ADSR_ATTACK(0, 23, 0) | base, envx, 48);
    for (i = 0; i < 9; i++)
        zexpect_u16_eq(0x0007, envx[i]);
    for (i = 9; i < 17; i++)
        zexpect_u16_eq(0x000e, envx[i]);
    for (i = 17; i < 25; i++)
        zexpect_u16_eq(0x0015, envx[i]);
    for (i = 25; i < 33; i++)
        zexpect_u16_eq(0x001c, envx[i]);
    for (i = 33; i < 41; i++)
        zexpect_u16_eq(0x0023, envx[i]);
    for (i = 41; i < 48; i++)
        zexpect_u16_eq(0x002a, envx[i]);

    adsr_capture(ADSR_ATTACK(1, 23, 0) | base, envx, 48);
    for (i = 0; i < 9; i++)
        zexpect_u16_eq(0x0006, envx[i]);
    for (i = 9; i < 17; i++)
        zexpect_u16_eq(0x000c, envx[i]);
    for (i = 17; i < 25; i++)
        zexpect_u16_eq(0x0012, envx[i]);
    for (i = 25; i < 33; i++)
        zexpect_u16_eq(0x0018, envx[i]);
    for (i = 33; i < 41; i++)
        zexpect_u16_eq(0x001e, envx[i]);
    for (i = 41; i < 48; i++)
        zexpect_u16_eq(0x0024, envx[i]);

    adsr_capture(ADSR_ATTACK(0, 24, 0) | base, envx, 48);
    for (i = 0; i < 17; i++)
        zexpect_u16_eq(0x0007, envx[i]);
    for (i = 17; i < 33; i++)
        zexpect_u16_eq(0x000e, envx[i]);
    for (i = 33; i < 48; i++)
        zexpect_u16_eq(0x0015, envx[i]);

    adsr_capture(ADSR_ATTACK(1, 24, 0) | base, envx, 48);
    for (i = 0; i < 17; i++)
        zexpect_u16_eq(0x0006, envx[i]);
    for (i = 17; i < 33; i++)
        zexpect_u16_eq(0x000c, envx[i]);
    for (i = 33; i < 48; i++)
        zexpect_u16_eq(0x0012, envx[i]);
}

ZTEST(spu, adsr_attack_exponential) {
    uint16_t envx[0x40];
    const uint32_t base =
        ADSR_DECAY(0) | ADSR_SUSTAIN(3, 0x1f, 15, 0, 0) | ADSR_RELEASE(0, 0);
    static const uint16_t e[] = {
        0x06e4, 0x0de4, 0x14e4, 0x1be4, 0x22e4, 0x29e4, 0x30e4, 0x37e4,
        0x3ee4, 0x45e4, 0x4ce4, 0x53e4, 0x5ae4, 0x607f, 0x623f, 0x63ff,
        0x65bf, 0x677f, 0x693f, 0x6aff, 0x6cbf, 0x6e7f, 0x703f, 0x71ff,
        0x73bf, 0x757f, 0x773f, 0x78ff, 0x7abf, 0x7c7f, 0x7e3f};
    int i;

    adsr_capture(ADSR_ATTACK(0, 12, 1) | base, envx, 32);
    zexpect_u16_eq(0x0007, envx[0]);
    for (i = 0; i < 31; i++)
        EXPECT_ENVX_NEAR(e[i], 7, envx[i + 1]);
}

ZTEST(spu, adsr_decay_shift) {
    uint16_t envx[0x40];
    int i;

    adsr_capture(ADSR_ATTACK(0, 1, 0) | ADSR_DECAY(10) |
                     ADSR_SUSTAIN(0, 0, 0, 1, 0) | ADSR_RELEASE(0, 1),
                 envx, 16);
    zexpect_u16_eq(0x1c00, envx[0]);
    EXPECT_ENVX_NEAR(0x6370, 0x07, envx[1]);
    EXPECT_ENVX_NEAR(0x4c95, 0x05, envx[2]);
    EXPECT_ENVX_NEAR(0x3ac6, 0x04, envx[3]);
    EXPECT_ENVX_NEAR(0x2cef, 0x03, envx[4]);
    EXPECT_ENVX_NEAR(0x221c, 0x03, envx[5]);
    EXPECT_ENVX_NEAR(0x19b0, 0x02, envx[6]);
    EXPECT_ENVX_NEAR(0x1343, 0x02, envx[7]);
    EXPECT_ENVX_NEAR(0x0e2d, 0x01, envx[8]);
    EXPECT_ENVX_NEAR(0x0a2d, 0x01, envx[9]);
    for (i = 10; i < 16; i++)
        zexpect_u16_eq(0x0000, envx[i]);

    adsr_capture(ADSR_ATTACK(0, 1, 0) | ADSR_DECAY(14) |
                     ADSR_SUSTAIN(0, 0, 0, 1, 0) | ADSR_RELEASE(0, 1),
                 envx, 32);
    zexpect_u16_eq(0x1c00, envx[0]);
    EXPECT_ENVX_NEAR(0x7e05, 0x02, envx[1]);
    EXPECT_ENVX_NEAR(0x7c05, 0x02, envx[2]);
    EXPECT_ENVX_NEAR(0x7805, 0x02, envx[4]);
    EXPECT_ENVX_NEAR(0x7405, 0x02, envx[6]);
    EXPECT_ENVX_NEAR(0x7005, 0x02, envx[8]);
    EXPECT_ENVX_NEAR(0x6906, 0x02, envx[12]);
    EXPECT_ENVX_NEAR(0x6206, 0x02, envx[16]);
    EXPECT_ENVX_NEAR(0x5bba, 0x02, envx[20]);
    EXPECT_ENVX_NEAR(0x55ba, 0x02, envx[24]);
    EXPECT_ENVX_NEAR(0x4fc7, 0x02, envx[28]);
    EXPECT_ENVX_NEAR(0x4c05, 0x02, envx[31]);
}

typedef struct {
    int level;
    uint16_t v;
    int start;
} AdsrSustainCase;

ZTEST(spu, adsr_sustain_level) {
    uint16_t envx[16];
    static const AdsrSustainCase cases[] = {
        {0, 0x07ff, 10},
        {7, 0x3ffa, 3},
        {11, 0x5ff6, 2},
        {14, 0x77ff, 1},
        {15, 0x7fef, 1}};
    size_t n;
    int i;
    for (n = 0; n < sizeof(cases) / sizeof(cases[0]); n++) {
        const AdsrSustainCase* c = &cases[n];
        adsr_capture(
            ADSR_ATTACK(0, 1, 0) | ADSR_DECAY(10) |
                ADSR_SUSTAIN(3, 0x1f, c->level, 1, 0) | ADSR_RELEASE(0, 1),
            envx, 16);
        for (i = c->start; i < 16; i++) {
            if (!zexpect_u16_eq(c->v, envx[i])) {
                zprintf("level %d sample %d\n", c->level, i);
            }
        }
    }
}

ZTEST(spu, adsr_sustain_up_linear) {
    uint16_t envx[0x20];
    int i;

    adsr_capture(ADSR_ATTACK(0, 1, 0) | ADSR_DECAY(0) |
                     ADSR_SUSTAIN(0, 10, 15, 0, 0) | ADSR_RELEASE(0, 1),
                 envx, 8);
    zexpect_u16_eq(0x1c00, envx[0]);
    EXPECT_ENVX_NEAR(0x5b50, 0x07, envx[1]);
    EXPECT_ENVX_NEAR(0x7750, 0x07, envx[2]);
    for (i = 3; i < 8; i++)
        zexpect_u16_eq(0x7fff, envx[i]);

    adsr_capture(ADSR_ATTACK(0, 1, 0) | ADSR_DECAY(0) |
                     ADSR_SUSTAIN(0, 14, 15, 0, 0) | ADSR_RELEASE(0, 1),
                 envx, 32);
    zexpect_u16_eq(0x1c00, envx[0]);
    EXPECT_ENVX_NEAR(0x41b8, 0x02, envx[1]);
    EXPECT_ENVX_NEAR(0x46f8, 0x02, envx[4]);
    EXPECT_ENVX_NEAR(0x4df8, 0x02, envx[8]);
    EXPECT_ENVX_NEAR(0x54f8, 0x02, envx[12]);
    EXPECT_ENVX_NEAR(0x5bf8, 0x02, envx[16]);
    EXPECT_ENVX_NEAR(0x70f8, 0x02, envx[28]);
    EXPECT_ENVX_NEAR(0x7638, 0x02, envx[31]);
}

ZTEST(spu, adsr_sustain_down_linear) {
    uint16_t envx[0x20];
    int i;

    adsr_capture(ADSR_ATTACK(0, 1, 0) | ADSR_DECAY(0) |
                     ADSR_SUSTAIN(3, 10, 15, 1, 0) | ADSR_RELEASE(0, 1),
                 envx, 8);
    zexpect_u16_eq(0x1c00, envx[0]);
    EXPECT_ENVX_NEAR(0x2c7c, 0x05, envx[1]);
    EXPECT_ENVX_NEAR(0x187c, 0x05, envx[2]);
    EXPECT_ENVX_NEAR(0x047c, 0x05, envx[3]);
    for (i = 4; i < 8; i++)
        zexpect_u16_eq(0x0000, envx[i]);

    adsr_capture(ADSR_ATTACK(0, 1, 0) | ADSR_DECAY(0) |
                     ADSR_SUSTAIN(0, 14, 15, 1, 0) | ADSR_RELEASE(0, 1),
                 envx, 32);
    zexpect_u16_eq(0x1c00, envx[0]);
    EXPECT_ENVX_NEAR(0x3e05, 0x02, envx[1]);
    EXPECT_ENVX_NEAR(0x3805, 0x02, envx[4]);
    EXPECT_ENVX_NEAR(0x3005, 0x02, envx[8]);
    EXPECT_ENVX_NEAR(0x2005, 0x02, envx[16]);
    EXPECT_ENVX_NEAR(0x1005, 0x02, envx[24]);
    EXPECT_ENVX_NEAR(0x0205, 0x02, envx[31]);
}

ZTEST(spu, adsr_sustain_up_exponential) {
    uint16_t envx[0x20];

    adsr_capture(ADSR_ATTACK(0, 1, 0) | ADSR_DECAY(0) |
                     ADSR_SUSTAIN(0, 12, 15, 0, 1) | ADSR_RELEASE(0, 1),
                 envx, 24);
    zexpect_u16_eq(0x1c00, envx[0]);
    EXPECT_ENVX_NEAR(0x46d1, 0x04, envx[1]);
    EXPECT_ENVX_NEAR(0x5bd1, 0x04, envx[4]);
    EXPECT_ENVX_NEAR(0x60ba, 0x02, envx[5]);
    EXPECT_ENVX_NEAR(0x67ba, 0x02, envx[9]);
    EXPECT_ENVX_NEAR(0x73fa, 0x02, envx[16]);
    EXPECT_ENVX_NEAR(0x7afa, 0x02, envx[20]);
    zexpect_u16_eq(0x7fff, envx[23]);
}

ZTEST(spu, adsr_sustain_down_exponential) {
    uint16_t envx[0x20];

    adsr_capture(ADSR_ATTACK(0, 1, 0) | ADSR_DECAY(0) |
                     ADSR_SUSTAIN(0, 12, 15, 1, 1) | ADSR_RELEASE(0, 1),
                 envx, 32);
    zexpect_u16_eq(0x1c00, envx[0]);
    EXPECT_ENVX_NEAR(0x3c19, 0x02, envx[1]);
    EXPECT_ENVX_NEAR(0x3019, 0x02, envx[4]);
    EXPECT_ENVX_NEAR(0x2112, 0x02, envx[9]);
    EXPECT_ENVX_NEAR(0x1eb7, 0x01, envx[10]);
    EXPECT_ENVX_NEAR(0x12b7, 0x01, envx[16]);
    EXPECT_ENVX_NEAR(0x095b, 0x01, envx[24]);
    EXPECT_ENVX_NEAR(0x035b, 0x01, envx[30]);
}

ZTEST(spu, adsr_release_linear) {
    uint16_t envx[0x20];
    int i;

    adsr_capture_with_keyoff(
        ADSR_ATTACK(0, 1, 0) | ADSR_DECAY(15) |
            ADSR_SUSTAIN(3, 0x1f, 15, 0, 0) | ADSR_RELEASE(12, 0),
        envx, 24, 3);
    zexpect_u16_eq(0x1c00, envx[0]);
    zexpect_u16_eq(0x7ff7, envx[1]);
    zexpect_u16_eq(0x7ff7, envx[2]);
    zexpect_u16_eq(0x7ff7, envx[3]);
    EXPECT_ENVX_NEAR(0x77fb, 0x04, envx[4]);
    EXPECT_ENVX_NEAR(0x47fb, 0x04, envx[10]);
    EXPECT_ENVX_NEAR(0x1ffb, 0x04, envx[15]);
    EXPECT_ENVX_NEAR(0x07fb, 0x04, envx[18]);
    for (i = 19; i < 24; i++)
        zexpect_u16_eq(0x0000, envx[i]);
}

ZTEST(spu, adsr_release_exponential) {
    uint16_t envx[0x20];

    adsr_capture_with_keyoff(
        ADSR_ATTACK(0, 1, 0) | ADSR_DECAY(15) |
            ADSR_SUSTAIN(3, 0x1f, 15, 0, 0) | ADSR_RELEASE(12, 1),
        envx, 24, 2);
    zexpect_u16_eq(0x1c00, envx[0]);
    zexpect_u16_eq(0x7ff7, envx[1]);
    zexpect_u16_eq(0x7ff7, envx[2]);
    EXPECT_ENVX_NEAR(0x77fb, 0x04, envx[3]);
    EXPECT_ENVX_NEAR(0x61fb, 0x04, envx[6]);
    EXPECT_ENVX_NEAR(0x45bf, 0x03, envx[11]);
    EXPECT_ENVX_NEAR(0x3099, 0x02, envx[16]);
    EXPECT_ENVX_NEAR(0x2172, 0x02, envx[21]);
    EXPECT_ENVX_NEAR(0x1cf7, 0x01, envx[23]);
}

enum {
    kRoomWorkAddr = 0x7D940,
    kRoomWorkBytes = 0x26C0,
    kRoomLapFrames = kRoomWorkBytes,
};

static void reverb_room_setup(bool master_enable, bool voice1_reverb) {
    SpuReverbAttr attr;
    spu_reset_quiet();
    SpuInit();
    Psyz_SpuWrite(0x1AA, 0x8000 | 0x4000);
    Psyz_SpuWrite(0x180, 0x3FFF);
    Psyz_SpuWrite(0x182, 0x3FFF);

    memset(&attr, 0, sizeof(attr));
    attr.mask = SPU_REV_MODE;
    attr.mode = SPU_REV_MODE_ROOM | SPU_REV_MODE_CLEAR_WA;
    if (!zexpect_s32_eq(0, SpuSetReverbModeParam(&attr))) {
        return;
    }

    memset(&attr, 0, sizeof(attr));
    attr.mask = SPU_REV_DEPTHL | SPU_REV_DEPTHR;
    attr.depth.left = 0x3FFF;
    attr.depth.right = 0x3FFF;
    if (!zexpect_s32_eq(0, SpuSetReverbModeParam(&attr))) {
        return;
    }

    if (master_enable) {
        if (!zexpect_s32_eq(SPU_ON, SpuSetReverb(SPU_ON))) {
            return;
        }
    }
    Psyz_SpuWrite(0x198, voice1_reverb ? (1u << 1) : 0);

    Psyz_SpuWrite((1 << 4) + 0x00, 0x3FFF);
    Psyz_SpuWrite((1 << 4) + 0x02, 0x3FFF);
}

static void run_reverb_room(
    const unsigned char* sample64, int nframes, unsigned char* work_out) {
    unsigned char upload[128];
    memcpy(upload, sample64, 64);
    memset(upload + 64, 0xAA, 64);
    Psyz_SpuMemWrite(kSampleAddr, upload, sizeof(upload));

    spu_voice1_keyon(kSampleAddr, 0x1000);
    pull_samples_nop(nframes);
    Psyz_SpuMemRead(kRoomWorkAddr, work_out, kRoomWorkBytes);

    Psyz_SpuWrite(0x18C, 0xFFFF);
    Psyz_SpuWrite(0x18E, 0xFFFF);
}

static void dump_actual_bin(
    const char* name, const unsigned char* data, size_t size) {
    char path[256];
    FILE* f;
    snprintf(path, sizeof(path), "expected/spu/%s.actual.bin", name);
    f = fopen(path, "wb");
    if (f) {
        fwrite(data, 1, size, f);
        fclose(f);
    }
}

static int compare_work_area(const char* name, const unsigned char* got,
                             const unsigned char* want, size_t want_size) {
    unsigned i;
    if (want_size != kRoomWorkBytes) {
        dump_actual_bin(name, got, kRoomWorkBytes);
        zprintf("%s: golden is %zu bytes, want %u\n", name, want_size,
                (unsigned)kRoomWorkBytes);
        return 0;
    }
    for (i = 0; i < kRoomWorkBytes; i++) {
        if (got[i] != want[i]) {
            dump_actual_bin(name, got, kRoomWorkBytes);
            zprintf("%s: first mismatch at work area offset %u (SPU RAM "
                    "0x%05X): got 0x%02X, want 0x%02X\n",
                    name, i, kRoomWorkAddr + i, got[i], want[i]);
            return 0;
        }
    }
    return 1;
}

static int check_work_area(const char* name, const unsigned char* got) {
    size_t size;
    unsigned char* want =
        load_expected(name, ".test.bin", "expected file", &size);
    int ok = compare_work_area(name, got, want, size);
    free(want);
    return ok;
}

static bool all_zero(const unsigned char* p, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        if (p[i] != 0) {
            return false;
        }
    }
    return true;
}

ZTEST(spu, reverb_room_work_area) {
    unsigned char work[kRoomWorkBytes] = {0};
    reverb_room_setup(true, true);
    run_reverb_room(kAdpcmSine, kRoomLapFrames, work);

    if (!zexpect_s32_eq(0, all_zero(work, sizeof(work)))) {
        zprintf("reverb wrote nothing to its work area\n");
    }
    zexpect_s32_eq(1, check_work_area("reverb_room", work));
}

ZTEST(spu, reverb_master_disabled_leaves_work_area_clear) {
    unsigned char work[kRoomWorkBytes] = {0};
    reverb_room_setup(false, true);
    run_reverb_room(kAdpcmSine, kRoomLapFrames, work);

    if (!zexpect_s32_ne(0, all_zero(work, sizeof(work)))) {
        zprintf("reverb wrote to its work area with SPUCNT bit 7 clear\n");
    }
}

ZTEST(spu, reverb_unrouted_voice_leaves_work_area_clear) {
    unsigned char work[kRoomWorkBytes] = {0};
    reverb_room_setup(true, false);
    run_reverb_room(kAdpcmSine, kRoomLapFrames, work);

    if (!zexpect_s32_ne(0, all_zero(work, sizeof(work)))) {
        zprintf(
            "reverb work area is non-zero with no voice routed to reverb\n");
    }
}

typedef struct tagSpuMalloc {
    u32 addr;
    u32 size;
} SPU_MALLOC;
extern SPU_MALLOC* _spu_memList;
extern int _spu_mem_mode_plus;
extern int _spu_rev_reserve_wa;
extern int _spu_rev_offsetaddr;
extern int _SpuIsInAllocateArea_(unsigned);
long SpuInitMalloc(long num, char* top);
long SpuMallocWithStartAddr(unsigned long addr, long size);

static char heap[0x1000];

ZTEST_SETUP(spu_malloc) {
    spu_setup();
    memset(heap, 0, sizeof(heap));
    SpuInit();
}

ZTEST(spu_malloc, SpuSetReverbOffClearsSpucnt) {
    Psyz_SpuWrite(0x1AA, 0x0080);

    zexpect_s32_eq(0, SpuSetReverb(SPU_OFF));
    zexpect_u16_eq(0x0000, Psyz_SpuRead(0x1AA));
}

ZTEST(spu_malloc, SpuSetReverbOnSetsSpucnt) {
    Psyz_SpuWrite(0x1AA, 0);
    SpuInitMalloc(32, heap);

    zexpect_s32_eq(3, _spu_mem_mode_plus);
    zexpect_s32_eq(0, _spu_rev_reserve_wa);
    zexpect_s32_eq(0xFFFE, _spu_rev_offsetaddr);
    zexpect_u32_eq(0x40001010u, _spu_memList[0].addr);
    zexpect_u32_eq(520176u, _spu_memList[0].size);
    zexpect_s32_eq(0, _SpuIsInAllocateArea_(_spu_rev_offsetaddr));

    zexpect_s32_eq(SPU_ON, SpuSetReverb(SPU_ON));
    zexpect_u16_eq(0x0080, Psyz_SpuRead(0x1AA));
}

ZTEST(spu_malloc, SpuMallocWithStartAddrSplitsFreeBlock) {
    SpuInitMalloc(32, heap);
    SpuMallocWithStartAddr(0x00001010, 0x00010000);

    zexpect_u32_eq(0x00001010u, _spu_memList[0].addr);
    zexpect_u32_eq(65536u, _spu_memList[0].size);
    zexpect_u32_eq(0x40011010u, _spu_memList[1].addr);
    zexpect_u32_eq(454640u, _spu_memList[1].size);
    zexpect_u32_eq(0u, _spu_memList[2].addr);
    zexpect_u32_eq(0u, _spu_memList[2].size);
}
