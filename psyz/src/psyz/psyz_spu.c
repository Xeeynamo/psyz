#include <psyz.h>
#include <psyz/log.h>
#include <assert.h>
#include <string.h>
#include "spu_voice.h"

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(SPU_RXX) == 0x200, "SPU_RXX must be 0x200 bytes");
_Static_assert(
    sizeof(union SpuUnion) == sizeof(SPU_RXX), "SpuUnion must alias SPU_RXX");
#endif

#define N_CHANNELS 2                      // stereo interleaved
#define CD_RING_FRAMES 4096               // must be power of 2
#define CD_RING_MASK (CD_RING_FRAMES - 1) // ring wrapper
#define CD_RING_LOW_WATER 1024            // refill when short of N frames

// Full SPU state
static struct {
    u8 ram[PSYZ_SPU_RAM_SIZE];

    // Transfer address (byte address into SPU RAM)
    unsigned transfer_addr;

    // Capture buffer position
    u16 capture_pos;

    unsigned reverb_cur;
    int reverb_phase;
    short reverb_input[2];
    short reverb_previous[2];

    VoiceState voice[PSYZ_SPU_NUM_VOICES];

    // CD audio ring buffer, refilled by Psyz_CdPullSamples
    short cd_ring[CD_RING_FRAMES * N_CHANNELS];
    unsigned cd_ring_read;
    unsigned cd_ring_count; // number of valid frames in ring

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
    for (int v = 0; v < PSYZ_SPU_NUM_VOICES; v++) {
        _spu_RXX->rxx.voice[v].volumex = 0;
    }
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

static void spu_key_on_voice(int v) {
    spu_voice_key_on(&spu.voice[v], &_spu_RXX->rxx.voice[v]);
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
    // MSVC's C-mode offsetof is not an integer constant expression, so it
    // cannot appear in a case label. Use an if-chain instead.
    if (reg_offset == offsetof(SPU_RXX, key_on[0])) { // voices 0..15
        for (int v = 0; v < 16; v++) {
            if (value & (1u << v))
                spu_key_on_voice(v);
        }
    } else if (reg_offset == offsetof(SPU_RXX, key_on[1])) { // voices 16..23
        for (int v = 0; v < 8; v++) {
            if (value & (1u << v))
                spu_key_on_voice(16 + v);
        }
    } else if (reg_offset == offsetof(SPU_RXX, key_off[0])) { // voices 0..15
        for (int v = 0; v < 16; v++) {
            if (value & (1u << v))
                spu.voice[v].key_off = 1;
        }
    } else if (reg_offset == offsetof(SPU_RXX, key_off[1])) { // voices 16..23
        for (int v = 0; v < 8; v++) {
            if (value & (1u << v))
                spu.voice[16 + v].key_off = 1;
        }
    } else if (reg_offset == offsetof(SPU_RXX, trans_addr)) {
        Psyz_SpuSetTransferAddr((unsigned)value << 3);
    } else if (reg_offset == offsetof(SPU_RXX, rev_work_addr)) {
        spu.reverb_cur = ((unsigned)value << 3) & 0x7FFFF;
    } else if (reg_offset == offsetof(SPU_RXX, trans_fifo)) {
        Psyz_SpuFifoWrite(value);
    }
}

unsigned short Psyz_SpuRead(unsigned int reg_offset) {
    if (reg_offset >= sizeof(SPU_RXX) || (reg_offset & 1)) {
        WARNF("Psyz_SpuRead: bad offset 0x%X", reg_offset);
        return 0;
    }
    // offsetof is not an ICE under MSVC C mode, so it cannot be a case label.
    if (reg_offset == offsetof(SPU_RXX, trans_addr)) {
        return (unsigned short)((spu.transfer_addr >> 3) & 0xFFFF);
    }
    if (reg_offset == offsetof(SPU_RXX, spustat)) {
        // lower 6 bits mirror SPUCNT's low bits
        // bit 11, capture-buffer half-pointer, flipped by spu_tick.
        return (_spu_RXX->rxx.spucnt & 0x3F) |
               (_spu_RXX->rxx.spustat & (1u << 11));
    }
    return _spu_RXX->raw[reg_offset >> 1];
}

static void write_capture(unsigned int idx, short val) {
    unsigned int addr = (idx * 0x400) | spu.capture_pos;
    if (addr < PSYZ_SPU_RAM_SIZE) {
        *(short*)&spu.ram[addr] = val;
    }
}

static inline int voice_vol(unsigned short reg) {
    if (reg & 0x8000) {
        // https://problemkaputt.de/psxspx-spu-volume-and-adsr-generator.htm
        LOG_ONCE("voice volume bit15 not implemented");
        return 0;
    }
    return (short)((reg & 0x7FFF) << 1);
}

static inline int spu_s16(u16 value) { return (short)value; }

static unsigned reverb_address(u16 raw_offset, int extra_bytes) {
    unsigned base = ((unsigned)_spu_RXX->rxx.rev_work_addr << 3) & 0x7FFFF;
    unsigned addr = spu.reverb_cur + ((unsigned)raw_offset << 3) + extra_bytes;
    addr &= 0x7FFFF;
    if (addr < base)
        addr = base + addr;
    return addr & 0x7FFFF;
}

static short reverb_read(u16 offset, int extra_bytes) {
    unsigned addr = reverb_address(offset, extra_bytes);
    return (short)(spu.ram[addr] | (spu.ram[(addr + 1) & 0x7FFFF] << 8));
}

static void reverb_write(u16 offset, int value) {
    unsigned addr;
    short sample;
    if (!(_spu_RXX->rxx.spucnt & SPU_CTRL_MASK_REVERB_MASTER_ENABLE))
        return;
    addr = reverb_address(offset, 0);
    sample = clamp16(value);
    spu.ram[addr] = (u8)sample;
    spu.ram[(addr + 1) & 0x7FFFF] = (u8)((u16)sample >> 8);
}

static int reverb_mul(int sample, u16 coefficient) {
    return (sample * spu_s16(coefficient)) >> 15;
}

static void reverb_process_22050(
    int input_left, int input_right, short output[2]) {
    SPU_RXX* r = (SPU_RXX*)&_spu_RXX->rxx;
    const int input[2] = {input_left, input_right};
    const u16 same_src[2] = {r->dLSAME, r->dRSAME};
    const u16 diff_src[2] = {r->dRDIFF, r->dLDIFF};
    const u16 same_dst[2] = {r->mLSAME, r->mRSAME};
    const u16 diff_dst[2] = {r->mLDIFF, r->mRDIFF};
    const u16 comb1[2] = {r->mLCOMB1, r->mRCOMB1};
    const u16 comb2[2] = {r->mLCOMB2, r->mRCOMB2};
    const u16 comb3[2] = {r->mLCOMB3, r->mRCOMB3};
    const u16 comb4[2] = {r->mLCOMB4, r->mRCOMB4};
    const u16 apf1[2] = {r->mLAPF1, r->mRAPF1};
    const u16 apf2[2] = {r->mLAPF2, r->mRAPF2};
    const u16 in_coef[2] = {r->vLIN, r->vRIN};
    int channel;

    for (channel = 0; channel < 2; channel++) {
        int iir_input_a =
            clamp16(reverb_mul(reverb_read(same_src[channel], 0), r->vWALL) +
                    reverb_mul(input[channel], in_coef[channel]));
        int iir_input_b =
            clamp16(reverb_mul(reverb_read(diff_src[channel], 0), r->vWALL) +
                    reverb_mul(input[channel], in_coef[channel]));
        int previous_a = reverb_read(same_dst[channel], -2);
        int previous_b = reverb_read(diff_dst[channel], -2);
        int inverse_iir = 0x8000 - spu_s16(r->vIIR);
        int iir_a = clamp16(reverb_mul(iir_input_a, r->vIIR) +
                            ((previous_a * inverse_iir) >> 15));
        int iir_b = clamp16(reverb_mul(iir_input_b, r->vIIR) +
                            ((previous_b * inverse_iir) >> 15));
        int accumulator;
        int feedback_a;
        int feedback_b;
        int mix_a;
        int mix_b;

        reverb_write(same_dst[channel], iir_a);
        reverb_write(diff_dst[channel], iir_b);
        accumulator = reverb_mul(reverb_read(comb1[channel], 0), r->vCOMB1) +
                      reverb_mul(reverb_read(comb2[channel], 0), r->vCOMB2) +
                      reverb_mul(reverb_read(comb3[channel], 0), r->vCOMB3) +
                      reverb_mul(reverb_read(comb4[channel], 0), r->vCOMB4);
        feedback_a = reverb_read((u16)(apf1[channel] - r->dAPF1), 0);
        feedback_b = reverb_read((u16)(apf2[channel] - r->dAPF2), 0);
        mix_a = clamp16(accumulator - reverb_mul(feedback_a, r->vAPF1));
        mix_b = clamp16(feedback_a + reverb_mul(mix_a, r->vAPF1) -
                        reverb_mul(feedback_b, r->vAPF2));
        output[channel] = clamp16(feedback_b + reverb_mul(mix_b, r->vAPF2));
        reverb_write(apf1[channel], mix_a);
        reverb_write(apf2[channel], mix_b);
    }

    spu.reverb_cur = (spu.reverb_cur + 2) & 0x7FFFF;
    if (spu.reverb_cur < ((unsigned)r->rev_work_addr << 3))
        spu.reverb_cur = (unsigned)r->rev_work_addr << 3;
}

static void reverb_tick(int input_left, int input_right, int output[2]) {
    if (!spu.reverb_phase) {
        spu.reverb_input[0] = clamp16(input_left);
        spu.reverb_input[1] = clamp16(input_right);
        output[0] = spu.reverb_previous[0];
        output[1] = spu.reverb_previous[1];
    } else {
        short next[2];
        reverb_process_22050((spu.reverb_input[0] + input_left) / 2,
                             (spu.reverb_input[1] + input_right) / 2, next);
        output[0] = (spu.reverb_previous[0] + next[0]) / 2;
        output[1] = (spu.reverb_previous[1] + next[1]) / 2;
        spu.reverb_previous[0] = next[0];
        spu.reverb_previous[1] = next[1];
    }
    spu.reverb_phase ^= 1;
}

// generate one frame at 44100hz with voices mix, cd playback and volume control
static void spu_tick(short* out) {
    SPU_RXX* rxx = (SPU_RXX*)&_spu_RXX->rxx;
    unsigned short spucnt = rxx->spucnt;

    // accumulate frame to each separate channels
    int left_sum = 0, right_sum = 0;
    int reverb_input_left = 0, reverb_input_right = 0;
    int reverb_output[2];

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

    // decode+resample, scale volume by ADSR envelope, then mix voices
    short v1_sample = 0, v3_sample = 0;
    for (int v = 0; v < PSYZ_SPU_NUM_VOICES; v++) {
        if (!spu.voice[v].active)
            continue;
        short s = spu_voice_step(&spu.voice[v], rxx->voice[v].pitch, spu.ram);
        spu_voice_envelope_step(&spu.voice[v]);
        if (spu.voice[v].env_state == ADSR_OFF) {
            s = 0;
        }
        rxx->voice[v].volumex = (unsigned short)spu.voice[v].env_vol;
        s = (short)(((int)s * spu.voice[v].env_vol) >> 15);
        if (v == 1) {
            v1_sample = s;
        } else if (v == 3) {
            v3_sample = s;
        }
        {
            int voice_left = (s * voice_vol(rxx->voice[v].volume.left)) >> 15;
            int voice_right = (s * voice_vol(rxx->voice[v].volume.right)) >> 15;
            left_sum += voice_left;
            right_sum += voice_right;
            if ((v < 16 && (rxx->rev_mode[0] & (1u << v))) ||
                (v >= 16 && (rxx->rev_mode[1] & (1u << (v - 16))))) {
                reverb_input_left += voice_left;
                reverb_input_right += voice_right;
            }
        }
    }

    // Mute all voices. CD audio is mixed after, and not affected by mute.
    if (!(spucnt & SPU_CTRL_MASK_MUTE_SPU)) {
        left_sum = right_sum = 0;
        reverb_input_left = reverb_input_right = 0;
    }

    reverb_tick(
        clamp16(reverb_input_left), clamp16(reverb_input_right), reverb_output);
    left_sum += reverb_mul(reverb_output[0], rxx->rev_vol.left);
    right_sum += reverb_mul(reverb_output[1], rxx->rev_vol.right);

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
    Psyz_RcntAdd(num_frames);
    for (int i = 0; i < num_frames; i++) {
        spu_tick(&out[i * 2]);
    }
}
