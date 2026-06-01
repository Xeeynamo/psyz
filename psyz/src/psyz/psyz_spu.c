#include <psyz.h>
#include <psyz/log.h>
#include <assert.h>
#include <string.h>
#include "../../decomp/src/libspu/libspu_private.h"

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(SPU_RXX) == 0x200, "SPU_RXX must be 0x200 bytes");
_Static_assert(
    sizeof(union SpuUnion) == sizeof(SPU_RXX), "SpuUnion must alias SPU_RXX");
#endif

// One ADPCM block decodes to 28 samples
#define ADPCM_BLOCK_BYTES 16
#define ADPCM_BLOCK_SAMPLES 28

static inline short clamp16(int v) {
    if (v < -32768)
        return -32768;
    if (v > 32767)
        return 32767;
    return v;
}

static inline short clamp15(int v) {
    if (v < -16384)
        return -16384;
    if (v > 16383)
        return 16383;
    return v;
}

static inline int sign4(int n) { return (n & 0x8) ? (n - 0x10) : n; }

// Decode one 16-byte SPU ADPCM block into 28 short samples. PS1 SPU ADPCM:
//   block[0]    = shift_filter: low nibble = shift (12-shift_in, or 9 if >12)
//                               high nibble = filter index (clamped 0..4)
//   block[1]    = flags: bit0 = loop-end, bit1 = repeat, bit2 = loop-start
//   block[2..15]= 14 bytes of 4-bit nibbles (low nibble first)
static void spu_adpcm_decode_block(
    const unsigned char block[ADPCM_BLOCK_BYTES], short* hist1, short* hist2,
    short out[ADPCM_BLOCK_SAMPLES], u8* flags_out) {
    static const int pos[5] = {0, 60, 115, 98, 122};
    static const int neg[5] = {0, 0, -52, -55, -60};

    int shift_in = block[0] & 0x0F;
    int shift = (shift_in > 12) ? 9 : (12 - shift_in);
    int filter = (block[0] >> 4) & 0x07;
    if (filter > 4)
        filter = 4;
    int f0 = pos[filter];
    int f1 = neg[filter];
    if (flags_out)
        *flags_out = block[1];

    short prev = *hist1;
    short prev2 = *hist2;
    for (int i = 0; i < 14; i++) {
        unsigned short byte = block[2 + i];
        for (int n = 0; n < 2; n++) {
            int t = sign4((byte >> (n * 4)) & 0x0F);
            int s = (t << shift) + ((prev * f0) >> 6) + ((prev2 * f1) >> 6);
            short final = clamp16(s);
            out[i * 2 + n] = final;
            prev2 = prev;
            prev = final;
        }
    }
    *hist1 = prev;
    *hist2 = prev2;
}

#define N_CHANNELS 2                      // stereo interleaved
#define CD_RING_FRAMES 4096               // must be power of 2
#define CD_RING_MASK (CD_RING_FRAMES - 1) // ring wrapper
#define CD_RING_LOW_WATER 1024            // refill when short of N frames

// Full SPU state
static struct {
    u8 ram[PSYZ_SPU_RAM_SIZE];

    // Transfer address (byte address into SPU RAM)
    u32 transfer_addr;

    // Capture buffer position
    u16 capture_pos;

    // CD audio ring buffer + pull callback
    s16 cd_ring[CD_RING_FRAMES * N_CHANNELS];
    u32 cd_ring_read;
    u32 cd_ring_count; // number of valid frames in ring
    void* cd_audio_userdata;

    u8 initialized;
} spu;

u8* Psyz_SpuGetRam(void) { return spu.ram; }

void Psyz_SpuInit(void) {
    if (spu.initialized)
        return;
    memset(&spu, 0, sizeof(spu));
    spu.initialized = 1;
    INFOF("SPU emulation initialized");
}

static void spu_reset(void) {
    _spu_RXX->rxx.spustat = 0;
    memset(&spu, 0, sizeof(spu));
}

static void spu_reset_hot(void) {
    u8 saved_ram[PSYZ_SPU_RAM_SIZE];
    memcpy(saved_ram, spu.ram, PSYZ_SPU_RAM_SIZE);
    spu_reset();
    memcpy(spu.ram, saved_ram, PSYZ_SPU_RAM_SIZE);
}

void Psyz_SpuReset(int hot) {
    if (hot) {
        spu_reset_hot();
    } else {
        spu_reset();
    }
    spu.initialized = 1;
}

void Psyz_SpuSetTransferAddr(unsigned int addr) {
    spu.transfer_addr = addr & (PSYZ_SPU_RAM_SIZE - 1);
}

unsigned int Psyz_SpuGetTransferAddr(void) { return spu.transfer_addr; }

void Psyz_SpuFifoWrite(unsigned short word) {
#ifdef PLATFORM_LE
    *(unsigned short*)&spu.ram[spu.transfer_addr] = word;
#else
    spu.ram[spu.transfer_addr] = (unsigned char)(word & 0xFF);
    spu.ram[spu.transfer_addr + 1] = (unsigned char)((word >> 8) & 0xFF);
#endif
    spu.transfer_addr = (spu.transfer_addr + 2) & (PSYZ_SPU_RAM_SIZE - 1);
}

void Psyz_SpuFifoWriteBulk(const unsigned char* src, unsigned int size) {
    Psyz_SpuMemWrite(spu.transfer_addr, src, size);
    spu.transfer_addr = (spu.transfer_addr + size) & (PSYZ_SPU_RAM_SIZE - 1);
}

void Psyz_SpuMemRead(unsigned int offset, void* dst, unsigned int size) {
    unsigned int start = offset & (PSYZ_SPU_RAM_SIZE - 1);
    unsigned int head = PSYZ_SPU_RAM_SIZE - start;
    if (size <= head) {
        memcpy(dst, &spu.ram[start], size);
    } else {
        memcpy(dst, &spu.ram[start], head);
        memcpy((unsigned char*)dst + head, &spu.ram[0], size - head);
    }
}

void Psyz_SpuMemWrite(unsigned int offset, const void* src, unsigned int size) {
    unsigned int start = offset & (PSYZ_SPU_RAM_SIZE - 1);
    unsigned int head = PSYZ_SPU_RAM_SIZE - start;
    if (size <= head) {
        memcpy(&spu.ram[start], src, size);
    } else {
        memcpy(&spu.ram[start], src, head);
        memcpy(&spu.ram[0], (const unsigned char*)src + head, size - head);
    }
}

void Psyz_SpuWrite(unsigned int reg_offset, unsigned short value) {
    if (reg_offset >= sizeof(SPU_RXX) || (reg_offset & 1)) {
        WARNF("Psyz_SpuWrite: bad offset 0x%X", reg_offset);
        return;
    }
    if (reg_offset == offsetof(SPU_RXX, spustat)) { // read-only
        return;
    }
    _spu_RXX->raw[reg_offset >> 1] = value;
    switch (reg_offset) {
    case 0x1A6: // xfer_addr
        Psyz_SpuSetTransferAddr((unsigned int)value << 3);
        break;
    case 0x1A8: // xfer_fifo
        Psyz_SpuFifoWrite(value);
        break;
    default:
        break;
    }
}

unsigned short Psyz_SpuRead(unsigned int reg_offset) {
    if (reg_offset >= 0x200 || (reg_offset & 1)) {
        WARNF("Psyz_SpuRead: bad offset 0x%X", reg_offset);
        return 0;
    }
    switch (reg_offset) {
    case offsetof(SPU_RXX, trans_addr):
        return (unsigned short)((spu.transfer_addr >> 3) & 0xFFFF);
    case offsetof(SPU_RXX, spustat):
        // lower 6 bits mirror SPUCNT's low bits
        // bit 11, capture-buffer half-pointer, flipped by spu_tick.
        return (_spu_RXX->rxx.spucnt & 0x3F) |
               (_spu_RXX->rxx.spustat & (1u << 11));
    default:
        return _spu_RXX->raw[reg_offset >> 1];
    }
}

static void write_capture(unsigned int idx, short val) {
    unsigned int addr = (idx * 0x400) | spu.capture_pos;
    if (addr < PSYZ_SPU_RAM_SIZE) {
        memcpy(&spu.ram[addr], &val, sizeof(val));
    }
}

static short voice_step(int v) { return 0; }

// generate one frame at 44100hz with voices mix, cd playback and volume control
static void spu_tick(short* out) {
    SPU_RXX* rxx = (SPU_RXX*)&_spu_RXX->rxx;
    unsigned short spucnt = rxx->spucnt;

    // accumulate frame to each separate channels
    int left_sum = 0, right_sum = 0;

    // Pre-fill CD ring buffer if running low
    if (spu.cd_ring_count < CD_RING_LOW_WATER) {
        size_t space = CD_RING_FRAMES - spu.cd_ring_count;
        size_t write_pos =
            (spu.cd_ring_read + spu.cd_ring_count) & CD_RING_MASK;
        // Fill into contiguous chunk up to end of array
        size_t chunk = CD_RING_FRAMES - write_pos;
        if (chunk > space)
            chunk = space;
        size_t got = Psyz_CdPullSamples(&spu.cd_ring[write_pos * 2], chunk);
        spu.cd_ring_count += got;
    }

    // Read one stereo frame from CD ring buffer
    int cd_left = 0, cd_right = 0;
    if (spu.cd_ring_count > 0) {
        cd_left = spu.cd_ring[spu.cd_ring_read * 2];
        cd_right = spu.cd_ring[spu.cd_ring_read * 2 + 1];
        spu.cd_ring_read = (spu.cd_ring_read + 1) & CD_RING_MASK;
        spu.cd_ring_count--;
    }

    // bare-bone voice mixer -- no envelope, no volume, no resampling
    short v1_sample = 0, v3_sample = 0;
    for (int v = 0; v < PSYZ_SPU_NUM_VOICES; v++) {
        if (!spu.voice[v].active)
            continue;
        short s = voice_step(v);
        if (v == 1)
            v1_sample = s;
        else if (v == 3)
            v3_sample = s;
        left_sum += s;
        right_sum += s;
    }

    // Mute all voices. CD audio is mixed after, and not affected by mute.
    if (!(spucnt & SPU_CTRL_MASK_MUTE_SPU)) {
        left_sum = right_sum = 0;
    }

    // Mix CD audio per SPUCNT and cd_vol registers.
    if (spucnt & SPU_CTRL_MASK_CD_AUDIO_ENABLE) {
        left_sum += (cd_left * rxx->cd_vol.left) >> 15;
        right_sum += (cd_right * rxx->cd_vol.right) >> 15;
    }

    out[0] = clamp16((left_sum * clamp15(rxx->main_vol.left)) >> 14);
    out[1] = clamp16((right_sum * clamp15(rxx->main_vol.right)) >> 14);

    // Capture buffers back to SPU RAM, as per real hardware
    write_capture(0, (short)cd_left);
    write_capture(1, (short)cd_right);
    write_capture(2, v1_sample);
    write_capture(3, v3_sample);

    // SPUSTAT bit 11 flips when capture_pos crosses 0x200
    unsigned prev_pos = spu.capture_pos;
    spu.capture_pos = (prev_pos + 2) & 0x3FF;
    if ((prev_pos ^ spu.capture_pos) & 0x200) {
        rxx->spustat ^= 1u << 11;
    }
}

void Psyz_SpuPullSamples(short* out, int num_frames) {
    if (!spu.initialized) {
        memset(out, 0, num_frames * 2 * sizeof(short));
        return;
    }
    for (int i = 0; i < num_frames; i++) {
        spu_tick(&out[i * 2]);
    }
}
