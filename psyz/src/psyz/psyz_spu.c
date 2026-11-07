#include <psyz.h>
#include <psyz/log.h>
#include <assert.h>
#include <string.h>
#include "spu_voice.h"
#include "../internal.h"

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

    // Frames generated since the reset
    unsigned ticks;

    unsigned reverb_cur;
    int reverb_in[64][2];    // reverb input per frame
    short reverb_out[2][32]; // network output per channel, one every 2 frames
    unsigned reverb_ticks[2];

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
    VoiceState* vs = &spu.voice[v];
    unsigned wait = spu.ticks & 1;
    spu_voice_key_on(vs, &_spu_RXX->rxx.voice[v], spu.ram);
    vs->hold_ticks += wait;
    vs->delay_ticks += wait;
    vs->env_counter = spu.ticks + ENV_CLOCK_PHASE;
}

static void spu_key_off_voice(int v) {
    spu.voice[v].key_off_wait = 1 + (spu.ticks & 1);
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
                spu_key_off_voice(v);
        }
    } else if (reg_offset == offsetof(SPU_RXX, key_off[1])) { // voices 16..23
        for (int v = 0; v < 8; v++) {
            if (value & (1u << v))
                spu_key_off_voice(16 + v);
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

static int rmul(int x, int coefficient) { return (x * coefficient) >> 14; }

static int reverb_tap(int ch, u16 left, u16 right, u16 coefficient) {
    return rmul(reverb_read(ch ? right : left, 0), spu_s16(coefficient));
}

// One channel of the reverb, run at 22.05 kHz
static short reverb_network(int ch, int in) {
    SPU_RXX* r = (SPU_RXX*)&_spu_RXX->rxx;
    u16 same = ch ? r->mRSAME : r->mLSAME;
    u16 diff = ch ? r->mRDIFF : r->mLDIFF;
    u16 apf1 = ch ? r->mRAPF1 : r->mLAPF1;
    u16 apf2 = ch ? r->mRAPF2 : r->mLAPF2;
    int vin = rmul(in, spu_s16(ch ? r->vRIN : r->vLIN));
    int iir = spu_s16(r->vIIR);
    int iir_prev = (short)((0x8000 - r->vIIR) & 0xFFFE);
    int vapf1 = spu_s16(r->vAPF1);
    int vapf2 = spu_s16(r->vAPF2);
    int a, b, fa, fb, ma, mb;

    a = clamp16((vin + reverb_tap(ch, r->dLSAME, r->dRSAME, r->vWALL)) >> 1);
    reverb_write(
        same, (rmul(a, iir) + rmul(reverb_read(same, -2), iir_prev)) >> 1);
    b = clamp16((vin + reverb_tap(ch, r->dRDIFF, r->dLDIFF, r->vWALL)) >> 1);
    reverb_write(
        diff, (rmul(b, iir) + rmul(reverb_read(diff, -2), iir_prev)) >> 1);
    fa = reverb_read((u16)(apf1 - r->dAPF1), 0);
    ma = clamp16((reverb_tap(ch, r->mLCOMB1, r->mRCOMB1, r->vCOMB1) +
                  reverb_tap(ch, r->mLCOMB2, r->mRCOMB2, r->vCOMB2) +
                  reverb_tap(ch, r->mLCOMB3, r->mRCOMB3, r->vCOMB3) +
                  reverb_tap(ch, r->mLCOMB4, r->mRCOMB4, r->vCOMB4) +
                  rmul(fa, -vapf1)) >>
                 1);
    reverb_write(apf1, ma);
    fb = reverb_read((u16)(apf2 - r->dAPF2), 0);
    mb = clamp16((2 * fa + rmul(ma, vapf1) + rmul(fb, -vapf2)) >> 1);
    reverb_write(apf2, mb);
    return clamp16((2 * fb + rmul(mb, vapf2)) >> 1);
}

static const short reverb_taps[10] = {
    20492, -5920, 2664, -1232, 532, -206, 70, -20, 4, -2};

static int reverb_downsample(int ch, unsigned centre) {
    int acc = spu.reverb_in[centre & 63][ch];
    for (int k = 0; k < 10; k++) {
        unsigned d = 2 * k + 1;
        acc += (reverb_taps[k] * spu.reverb_in[(centre - d) & 63][ch]) >> 15;
        acc += (reverb_taps[k] * spu.reverb_in[(centre + d) & 63][ch]) >> 15;
    }
    return clamp16(acc >> 1);
}

static int reverb_upsample(int ch, int on_output_frame) {
    unsigned t = spu.reverb_ticks[ch];
    int acc = 0;
    if (on_output_frame) {
        return spu.reverb_out[ch][(t - 10) & 31];
    }
    for (int j = 0; j < 20; j++) {
        int d = j < 10 ? 9 - j : j - 10;
        acc += (spu.reverb_out[ch][(t - 1 - j) & 31] * reverb_taps[d]) >> 14;
    }
    return clamp16(acc >> 1);
}

static void reverb_tick(int in_left, int in_right, int out[2]) {
    SPU_RXX* r = (SPU_RXX*)&_spu_RXX->rxx;
    unsigned n = spu.ticks;
    int ch = n & 1;
    spu.reverb_in[n & 63][0] = in_left;
    spu.reverb_in[n & 63][1] = in_right;
    spu.reverb_out[ch][spu.reverb_ticks[ch]++ & 31] =
        reverb_network(ch, reverb_downsample(ch, n - 19));
    if (ch == 1) {
        spu.reverb_cur = (spu.reverb_cur + 2) & 0x7FFFF;
        if (spu.reverb_cur < ((unsigned)r->rev_work_addr << 3))
            spu.reverb_cur = (unsigned)r->rev_work_addr << 3;
    }
    out[0] = reverb_upsample(0, n & 1);
    out[1] = reverb_upsample(1, !(n & 1));
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
        if (spu.voice[v].key_off_wait && !--spu.voice[v].key_off_wait) {
            spu.voice[v].key_off = 1;
        }
        short s = spu_voice_step(&spu.voice[v], rxx->voice[v].pitch, spu.ram);
        int env = spu_voice_envelope_step(&spu.voice[v]);
        rxx->voice[v].volumex = (unsigned short)spu.voice[v].env_vol;
        s = (short)(((int)s * env) >> 15);
        if (v == 1) {
            v1_sample = s;
        } else if (v == 3) {
            v3_sample = s;
        }
        {
            int voice_left =
                (s * spu_volume(rxx->voice[v].volume.left, 0)) >> 14;
            int voice_right =
                (s * spu_volume(rxx->voice[v].volume.right, 0)) >> 14;
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

    reverb_tick(clamp16(reverb_input_left >> 1),
                clamp16(reverb_input_right >> 1), reverb_output);
    left_sum += (reverb_output[0] * spu_s16(rxx->rev_vol.left)) >> 14;
    right_sum += (reverb_output[1] * spu_s16(rxx->rev_vol.right)) >> 14;

    // Mix CD audio per SPUCNT and cd_vol registers.
    if (spucnt & SPU_CTRL_MASK_CD_AUDIO_ENABLE) {
        left_sum += (cd_left * rxx->cd_vol.left) >> 14;
        right_sum += (cd_right * rxx->cd_vol.right) >> 14;
    }

    out[0] = clamp16(
        (clamp16(left_sum >> 1) * spu_volume(rxx->main_vol.left, 0x7FFE)) >>
        15);
    out[1] = clamp16(
        (clamp16(right_sum >> 1) * spu_volume(rxx->main_vol.right, 0x7FFE)) >>
        15);

    // Capture buffers back to SPU RAM, as per real hardware
    write_capture(0, (short)cd_left);
    write_capture(1, (short)cd_right);
    write_capture(2, v1_sample);
    write_capture(3, v3_sample);

    // SPUSTAT bit 11 flips when capture_pos crosses 0x200
    unsigned prev_pos = spu.capture_pos;
    spu.capture_pos = (prev_pos + 2) & 0x3FF;
    spu.ticks++;
    if ((prev_pos ^ spu.capture_pos) & 0x200) {
        rxx->spustat ^= 1u << 11;
    }
}

void Psyz_SpuPullSamples(short* out, int num_frames) {
    Psyz_KernelSpuSamples(num_frames);
    if (!spu.initialized) {
        memset(out, 0, num_frames * 2 * sizeof(short));
        return;
    }
    for (int i = 0; i < num_frames; i++) {
        spu_tick(&out[i * 2]);
    }
}
