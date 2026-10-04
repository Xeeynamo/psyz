#include <pspkernel.h>
#include <pspsascore.h>
#include <psputility.h>
#include <psyz.h>
#include <psyz/log.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "../psyz/spu_voice.h"
#include "psp.h"

#define N_CHANNELS 2
#define CD_RING_FRAMES 4096
#define CD_RING_MASK (CD_RING_FRAMES - 1)
#define CD_RING_LOW_WATER 1024
#define GRAIN 512
#define BLOCKS (PSYZ_SPU_RAM_SIZE / ADPCM_BLOCK_BYTES)
#define CAPTURE_END 0x1000
#define CAPTURE_WINDOW 44100
#define CAPTURED_VOICES ((1u << 1) | (1u << 3))
#define SAS_HEIGHT_MAX 0x40000000
#define UPLOAD_CHUNK 0x1000
#define UTILITY_ALREADY_LOADED 0x80111102
#define UNCACHED(p) ((void*)((uintptr_t)(p) | 0x40000000))

typedef struct {
    u32 start;
    u16 adsr_lo, adsr_hi;
    u16 sent_adsr_lo, sent_adsr_hi;
    short vol[4];
    u16 pitch;
    u8 kon;
    u8 koff;
    u8 on;
    u8 adsr_valid;
    u8 params_valid;
} SasVoice;

enum { GRAIN_EMPTY, GRAIN_RENDERING, GRAIN_READY };

static struct {
    unsigned transfer_addr;
    u16 capture_pos;
    VoiceState voice[PSYZ_SPU_NUM_VOICES];
    short cd_ring[CD_RING_FRAMES * N_CHANNELS];
    unsigned cd_ring_read;
    unsigned cd_ring_count;
    unsigned capture_left;
    u32 tracked;
    u8 initialized;
} spu;

// Two copies of the SPU RAM, where the SAS one has ADPCM samples adapted.
static u8 __attribute__((aligned(64))) ram_storage[PSYZ_SPU_RAM_SIZE];
static u8 __attribute__((aligned(64))) sas_ram_storage[PSYZ_SPU_RAM_SIZE];
static SceSasCore __attribute__((aligned(64))) ctx_storage;
static short __attribute__((aligned(64))) grain_storage[GRAIN * N_CHANNELS];
static u32 end_blocks[BLOCKS / 32];
static SasVoice sas_voice[PSYZ_SPU_NUM_VOICES];
static volatile int grain_state;
static unsigned grain_pos;
static unsigned grain_epoch, spu_epoch;
static volatile u32 sas_ended;
static u8 sas_ok, sas_reset_pending;
static u8 rev_dirty, rev_settling;
static int rev_type, rev_delay, rev_feedback;
static short rev_evol[2];
static int rev_wet;

u8* Psyz_SpuGetRam(void) { return ram_storage; }

static void mirror_clear(void) {
    memset16_vfpu(sas_ram_storage, 0, PSYZ_SPU_RAM_SIZE);
    memset(end_blocks, 0, sizeof(end_blocks));
    psp_dcache_writeback(sas_ram_storage, PSYZ_SPU_RAM_SIZE);
    psp_dcache_writeback(ram_storage, PSYZ_SPU_RAM_SIZE);
}

// SAS ADPCM flags comparer:
// - bit3: jumps back after decoding
// - bit6: marks the loop start
// - bit7: stops before decoding
static inline u8 sas_flag(u8 flags) {
    // nibble i is the SAS flag for (flags & 7) == i
    return (0x76663000u >> ((flags & 7) * 4)) & 0xF;
}

// Sync specified memory range from Main CPU to SAS in Media Engine CPU
static void mirror_sync(unsigned start, unsigned size) {
    unsigned first = start / ADPCM_BLOCK_BYTES;
    unsigned last = (start + size - 1) / ADPCM_BLOCK_BYTES;
    unsigned bytes = (last - first + 1) * ADPCM_BLOCK_BYTES;
    memcpy16_vfpu(&sas_ram_storage[first * ADPCM_BLOCK_BYTES],
                  &ram_storage[first * ADPCM_BLOCK_BYTES], bytes);
    const u8* src = &ram_storage[first * ADPCM_BLOCK_BYTES + 1];
    u8* dst = &sas_ram_storage[first * ADPCM_BLOCK_BYTES + 1];
    for (unsigned w = first / 32; w <= last / 32; w++) {
        unsigned lo = w * 32 > first ? 0 : first & 31;
        unsigned hi = w * 32 + 31 < last ? 31 : last & 31;
        u32 ends = 0;
#pragma GCC unroll 4
        for (unsigned i = lo; i <= hi; i++) {
            u8 flags = *src;
            *dst = sas_flag(flags);
            ends |= (u32)(flags & 1) << i;
            src += ADPCM_BLOCK_BYTES;
            dst += ADPCM_BLOCK_BYTES;
        }
        u32 mask = ((2u << hi) - 1) & ~((1u << lo) - 1);
        end_blocks[w] = (end_blocks[w] & ~mask) | ends;
    }
    psp_dcache_writeback(&sas_ram_storage[first * ADPCM_BLOCK_BYTES], bytes);
    psp_dcache_writeback(&ram_storage[first * ADPCM_BLOCK_BYTES], bytes);
}

// Chunked so the mirror copy and the flag pass still find the chunk in cache
static void ram_write(unsigned start, const void* src, unsigned size) {
    const unsigned char* s = (const unsigned char*)src;
    while (size) {
        unsigned n = UPLOAD_CHUNK - (start & (UPLOAD_CHUNK - 1));
        if (n > size)
            n = size;
        memcpy_vfpu(&ram_storage[start], s, n);
        mirror_sync(start, n);
        start += n;
        s += n;
        size -= n;
    }
}

static void spu_reset_state(int hot) {
    _spu_RXX->rxx.spustat = 0;
    for (int v = 0; v < PSYZ_SPU_NUM_VOICES; v++) {
        _spu_RXX->rxx.voice[v].volumex = 0;
    }
    if (!hot) {
        memset16_vfpu(ram_storage, 0, PSYZ_SPU_RAM_SIZE);
        mirror_clear();
    }
    spu.transfer_addr = 0;
    spu.capture_pos = 0;
    memset(spu.voice, 0, sizeof(spu.voice));
    memset(spu.cd_ring, 0, sizeof(spu.cd_ring));
    spu.cd_ring_read = spu.cd_ring_count = 0;
    spu.capture_left = CAPTURE_WINDOW;
    spu.tracked = 0;
    for (int v = 0; v < PSYZ_SPU_NUM_VOICES; v++) {
        sas_voice[v].kon = sas_voice[v].koff = 0;
    }
    sas_reset_pending = 1;
    spu_epoch++;
}

static int sas_init(void) {
    int ret = sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
    if (ret < 0 && ret != UTILITY_ALREADY_LOADED) {
        ERRORF("sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC) failed: %08x", ret);
        return 0;
    }
    ret = sceUtilityLoadModule(PSP_MODULE_AV_SASCORE);
    if (ret < 0 && ret != UTILITY_ALREADY_LOADED) {
        ERRORF("sceUtilityLoadModule(PSP_MODULE_AV_SASCORE) failed: %08x", ret);
        return 0;
    }
    ret = __sceSasInit(&ctx_storage, GRAIN, PSYZ_SPU_NUM_VOICES,
                       PSP_SAS_OUTPUTMODE_STEREO, PSP_SAS_SAMPLE_RATE);
    if (ret < 0) {
        ERRORF("__sceSasInit failed: %08x", ret);
        return 0;
    }
    return 1;
}

void Psyz_SpuInit(void) {
    if (spu.initialized)
        return;
    sas_ok = sas_init();
    spu_reset_state(0);
    spu.initialized = 1;
    INFOF("SPU emulation initialized on sceSasCore");
}

void Psyz_SpuReset(int hot) {
    Psyz_AudioLock();
    spu_reset_state(hot);
    spu.initialized = 1;
    Psyz_AudioUnlock();
}

void Psyz_SpuSetTransferAddr(unsigned int addr) {
    spu.transfer_addr = addr & (PSYZ_SPU_RAM_SIZE - 1);
}

unsigned int Psyz_SpuGetTransferAddr(void) { return spu.transfer_addr; }

void Psyz_SpuFifoWrite(unsigned short word) {
    unsigned addr = spu.transfer_addr;
    ram_storage[addr] = (unsigned char)(word & 0xFF);
    ram_storage[addr + 1] = (unsigned char)(word >> 8);
    mirror_sync(addr, 2);
    spu.transfer_addr = (addr + 2) & (PSYZ_SPU_RAM_SIZE - 1);
}

void Psyz_SpuFifoWriteBulk(const unsigned char* src, unsigned int size) {
    Psyz_SpuMemWrite(spu.transfer_addr, src, size);
    spu.transfer_addr = (spu.transfer_addr + size) & (PSYZ_SPU_RAM_SIZE - 1);
}

void Psyz_SpuMemRead(unsigned int offset, void* dst, unsigned int size) {
    unsigned int start = offset & (PSYZ_SPU_RAM_SIZE - 1);
    unsigned int head = PSYZ_SPU_RAM_SIZE - start;
    if (size <= head) {
        memcpy_vfpu(dst, &ram_storage[start], size);
    } else {
        memcpy_vfpu(dst, &ram_storage[start], head);
        memcpy_vfpu((unsigned char*)dst + head, &ram_storage[0], size - head);
    }
    if (size && (start < CAPTURE_END || size > head)) {
        spu.capture_left = CAPTURE_WINDOW;
    }
}

void Psyz_SpuMemWrite(unsigned int offset, const void* src, unsigned int size) {
    unsigned int start = offset & (PSYZ_SPU_RAM_SIZE - 1);
    unsigned int head = PSYZ_SPU_RAM_SIZE - start;
    if (!size)
        return;
    Psyz_AudioLock();
    if (size <= head) {
        ram_write(start, src, size);
    } else {
        ram_write(start, src, head);
        ram_write(0, (const unsigned char*)src + head, size - head);
    }
    Psyz_AudioUnlock();
}

static void spu_key_on_voice(int v) {
    VoiceState* vs = &spu.voice[v];
    SasVoice* sv = &sas_voice[v];
    spu_voice_key_on(vs, &_spu_RXX->rxx.voice[v]);
    sv->start = vs->cur_addr;
    sv->adsr_lo = vs->adsr_lo;
    sv->adsr_hi = vs->adsr_hi;
    sv->kon = 1;
    sv->koff = 0;
    if (CAPTURED_VOICES & (1u << v)) {
        if (spu.capture_left) {
            spu.tracked |= 1u << v;
        } else {
            spu.tracked &= ~(1u << v);
        }
    }
}

static void spu_key_on_mask(u32 mask) {
    Psyz_AudioLock();
    for (int v = 0; v < PSYZ_SPU_NUM_VOICES; v++) {
        if (mask & (1u << v))
            spu_key_on_voice(v);
    }
    Psyz_AudioUnlock();
}

static void spu_key_off_mask(u32 mask) {
    Psyz_AudioLock();
    for (int v = 0; v < PSYZ_SPU_NUM_VOICES; v++) {
        if (mask & (1u << v)) {
            spu.voice[v].key_off = 1;
            sas_voice[v].koff = 1;
        }
    }
    Psyz_AudioUnlock();
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
    case offsetof(SPU_RXX, key_on[0]):
        spu_key_on_mask(value);
        break;
    case offsetof(SPU_RXX, key_on[1]):
        spu_key_on_mask((u32)(value & 0xFF) << 16);
        break;
    case offsetof(SPU_RXX, key_off[0]):
        spu_key_off_mask(value);
        break;
    case offsetof(SPU_RXX, key_off[1]):
        spu_key_off_mask((u32)(value & 0xFF) << 16);
        break;
    case offsetof(SPU_RXX, trans_addr):
        Psyz_SpuSetTransferAddr((unsigned)value << 3);
        break;
    case offsetof(SPU_RXX, trans_fifo):
        Psyz_SpuFifoWrite(value);
        break;
    case offsetof(SPU_RXX, rev_work_addr):
        rev_dirty = 1;
        break;
    default:
        if (reg_offset >= offsetof(SPU_RXX, dAPF1)) {
            rev_dirty = 1;
        }
        break;
    }
}

unsigned short Psyz_SpuRead(unsigned int reg_offset) {
    if (reg_offset >= sizeof(SPU_RXX) || (reg_offset & 1)) {
        WARNF("Psyz_SpuRead: bad offset 0x%X", reg_offset);
        return 0;
    }
    switch (reg_offset) {
    case offsetof(SPU_RXX, trans_addr):
        return (unsigned short)((spu.transfer_addr >> 3) & 0xFFFF);
    case offsetof(SPU_RXX, spustat):
        return (_spu_RXX->rxx.spucnt & 0x3F) |
               (_spu_RXX->rxx.spustat & (1u << 11));
    }
    return _spu_RXX->raw[reg_offset >> 1];
}

// PS1 envelope steps are per 44.1kHz tick over 0..0x7FFF, SAS rates per
// sample over 0..0x40000000, so a PS1 step scales by 2^15.
static u32 sas_rate(u32 step, int shift) {
    if (shift >= 0) {
        unsigned long long rate = (unsigned long long)step << shift;
        return rate > 0x7FFFFFFF ? 0x7FFFFFFF : (u32)rate;
    }
    return shift <= -32 ? 0 : step >> -shift;
}

static u32 sas_rate_increase(int rate) {
    return sas_rate(7 - (rate & 3), 26 - (rate >> 2));
}

static u32 sas_rate_decrease(int rate) {
    return sas_rate(8 - (rate & 3), 26 - (rate >> 2));
}

// SAS exponential decrease removes height * rate / 2^32 per sample.
static u32 sas_rate_exponential(int rate) {
    return sas_rate(8 - (rate & 3), 28 - (rate >> 2));
}

static void sas_set_envelope(int v, u16 lo, u16 hi) {
    int attack = (lo >> 8) & 0x7F;
    int decay = ((lo >> 4) & 0xF) * 4;
    int level = lo & 0xF;
    int sustain = (hi >> 6) & 0x7F;
    int release = (hi & 0x1F) * 4;
    int attack_mode = (lo & 0x8000) ? PSP_SAS_ADSR_CURVE_MODE_LINEAR_BENT
                                    : PSP_SAS_ADSR_CURVE_MODE_LINEAR_INCREASE;
    int sustain_mode, release_mode;
    u32 sustain_rate, release_rate;
    if (hi & 0x4000) {
        sustain_mode = (hi & 0x8000) ? PSP_SAS_ADSR_CURVE_MODE_EXPONENT_REV
                                     : PSP_SAS_ADSR_CURVE_MODE_LINEAR_DECREASE;
        sustain_rate = (hi & 0x8000) ? sas_rate_exponential(sustain)
                                     : sas_rate_decrease(sustain);
    } else {
        sustain_mode = (hi & 0x8000) ? PSP_SAS_ADSR_CURVE_MODE_LINEAR_BENT
                                     : PSP_SAS_ADSR_CURVE_MODE_LINEAR_INCREASE;
        sustain_rate = sas_rate_increase(sustain);
    }
    if (hi & 0x20) {
        release_mode = PSP_SAS_ADSR_CURVE_MODE_EXPONENT_REV;
        release_rate = sas_rate_exponential(release);
    } else {
        release_mode = PSP_SAS_ADSR_CURVE_MODE_LINEAR_DECREASE;
        release_rate = sas_rate_decrease(release);
    }

    // The PS1 checks the sustain level only after a decay step. Put the SAS
    // level halfway into its first step so both land on the same height.
    int threshold = ((level + 1) << 11) - 1;
    u32 sustain_level = (u32)threshold << 15;
    if (0x7FFF + ((adsr_num_decrease(decay) * 0x7FFF) >> 15) < threshold) {
        u32 drop = (u32)(((unsigned long long)SAS_HEIGHT_MAX *
                          sas_rate_exponential(decay)) >>
                         32);
        sustain_level = SAS_HEIGHT_MAX - drop / 2;
    }

    int ret = __sceSasSetADSRmode(
        &ctx_storage, v, PSP_SAS_ADSR_EVERYTHING, attack_mode,
        PSP_SAS_ADSR_CURVE_MODE_EXPONENT_REV, sustain_mode, release_mode);
    if (ret >= 0) {
        ret = __sceSasSetADSR(
            &ctx_storage, v, PSP_SAS_ADSR_EVERYTHING, sas_rate_increase(attack),
            sas_rate_exponential(decay), sustain_rate, release_rate);
    }
    if (ret >= 0) {
        ret = __sceSasSetSL(&ctx_storage, v, (int)sustain_level);
    }
    if (ret < 0) {
        WARNF(
            "SPU voice %d: envelope %04X/%04X rejected: %08x", v, lo, hi, ret);
    }
}

// Convert ranges from PS1 -0x4000..0x4000 to SAS -0x1000..0x1000
static short sas_level(u16 reg) {
    if (reg & 0x8000) {
        LOG_ONCE("voice volume bit15 not implemented");
        return 0;
    }
    int level = ((short)((reg & 0x7FFF) << 1) + 4) >> 3;
    return level > 0x1000 ? 0x1000 : (short)level;
}

static void sas_voice_params(int v) {
    volatile SPU_RXX* rxx = &_spu_RXX->rxx;
    SasVoice* sv = &sas_voice[v];
    short vol[4] = {0, 0, 0, 0};
    if (rxx->spucnt & SPU_CTRL_MASK_MUTE_SPU) {
        vol[0] = sas_level(rxx->voice[v].volume.left);
        vol[1] = sas_level(rxx->voice[v].volume.right);
        if (rxx->rev_mode[v >> 4] & (1u << (v & 15))) {
            vol[2] = vol[0];
            vol[3] = vol[1];
        }
    }
    if (!sv->params_valid || memcmp(vol, sv->vol, sizeof(vol))) {
        __sceSasSetVolume(&ctx_storage, v, vol[0], vol[1], vol[2], vol[3]);
        memcpy(sv->vol, vol, sizeof(vol));
    }
    u16 pitch = rxx->voice[v].pitch & 0x3FFF;
    if (!pitch)
        pitch = 1;
    if (!sv->params_valid || pitch != sv->pitch) {
        __sceSasSetPitch(&ctx_storage, v, pitch);
        sv->pitch = pitch;
    }
    sv->params_valid = 1;
}

static u32 sas_sample_size(u32 start, int* loop) {
    if (start & 8) {
        for (u32 addr = start; addr + ADPCM_BLOCK_BYTES <= PSYZ_SPU_RAM_SIZE;
             addr += ADPCM_BLOCK_BYTES) {
            u8 flags = ram_storage[addr + 1];
            if (flags & 1) {
                *loop = (flags & 3) == 3;
                return addr + ADPCM_BLOCK_BYTES - start;
            }
        }
    } else {
        for (u32 b = start / ADPCM_BLOCK_BYTES; b < BLOCKS;) {
            u32 bits = end_blocks[b / 32] >> (b % 32);
            if (bits) {
                b += __builtin_ctz(bits);
                *loop = (ram_storage[b * ADPCM_BLOCK_BYTES + 1] & 3) == 3;
                return (b + 1) * ADPCM_BLOCK_BYTES - start;
            }
            b = (b | 31) + 1;
        }
    }
    *loop = 0;
    return (PSYZ_SPU_RAM_SIZE - start) & ~(ADPCM_BLOCK_BYTES - 1);
}

static void sas_key_on(int v) {
    SasVoice* sv = &sas_voice[v];
    int loop;
    u32 size = sas_sample_size(sv->start, &loop);
    void* data = UNCACHED((sv->start & 8) ? &ram_storage[sv->start]
                                          : &sas_ram_storage[sv->start]);
    __sceSasSetKeyOff(&ctx_storage, v);
    sv->on = 0;
    int ret = __sceSasSetVoice(&ctx_storage, v, data, (int)size, loop);
    if (ret < 0) {
        WARNF("SPU voice %d: sample at %05X rejected: %08x", v, sv->start, ret);
        return;
    }
    if (!sv->adsr_valid || sv->sent_adsr_lo != sv->adsr_lo ||
        sv->sent_adsr_hi != sv->adsr_hi) {
        sas_set_envelope(v, sv->adsr_lo, sv->adsr_hi);
        sv->sent_adsr_lo = sv->adsr_lo;
        sv->sent_adsr_hi = sv->adsr_hi;
        sv->adsr_valid = 1;
    }
    sas_voice_params(v);
    ret = __sceSasSetKeyOn(&ctx_storage, v);
    if (ret < 0) {
        WARNF("SPU voice %d: key on failed: %08x", v, ret);
        return;
    }
    sv->on = 1;
}

static void sas_silence(int v) {
    SasVoice* sv = &sas_voice[v];
    __sceSasSetVolume(&ctx_storage, v, 0, 0, 0, 0);
    __sceSasSetKeyOff(&ctx_storage, v);
    memset(sv->vol, 0, sizeof(sv->vol));
    sv->on = 0;
}

// r->dAPF1 is the first value from the reverb table.
// https://problemkaputt.de/psx-spx.htm#spureverbexamples
static int sas_reverb_match(const volatile SPU_RXX* r) {
    switch (r->dAPF1) {
    case 0x0000:
        return PSP_SAS_EFFECT_TYPE_OFF;
    case 0x007D:
        return PSP_SAS_EFFECT_TYPE_ROOM;
    case 0x0033:
        return PSP_SAS_EFFECT_TYPE_SMALL;
    case 0x00B1:
        return PSP_SAS_EFFECT_TYPE_MEDIUM;
    case 0x00E3:
        return PSP_SAS_EFFECT_TYPE_LARGE;
    case 0x01A5:
        return PSP_SAS_EFFECT_TYPE_HALL;
    case 0x033D:
        return PSP_SAS_EFFECT_TYPE_SPACE;
    case 0x0017:
        return PSP_SAS_EFFECT_TYPE_PIPE;
    case 0x0001:
        // A SAS delay step is a quarter of the PS1 one, measured on hardware.
        rev_delay = 4 * (((r->mLSAME + r->dAPF1) * 127 + 8191) >> 13);
        rev_feedback = (r->vWALL * 127 + 0x80FF) / 0x8100;
        if (rev_delay > 127)
            rev_delay = 127;
        if (rev_feedback > 127)
            rev_feedback = 127;
        return r->vWALL ? PSP_SAS_EFFECT_TYPE_ECHO : PSP_SAS_EFFECT_TYPE_DELAY;
    default:
        LOG_ONCE("custom reverb registers are not supported");
        return PSP_SAS_EFFECT_TYPE_OFF;
    }
}

static short sas_effect_level(u16 reg) {
    int level = (short)reg >> 3;
    return level < 0 ? 0 : (short)level;
}

static void sas_reverb_sync(void) {
    volatile SPU_RXX* r = &_spu_RXX->rxx;
    if (rev_dirty) {
        // _spu_setReverbAttr spans many register writes; wait for them to end
        rev_dirty = 0;
        rev_settling = 1;
    } else if (rev_settling) {
        rev_settling = 0;
        int delay = rev_delay, feedback = rev_feedback;
        int type = sas_reverb_match(r);
        if (type != rev_type) {
            __sceSasRevType(&ctx_storage, type);
            rev_type = type;
            delay = feedback = -1;
        }
        if (delay != rev_delay || feedback != rev_feedback) {
            __sceSasRevParam(&ctx_storage, rev_delay, rev_feedback);
        }
    }
    short evol[2] = {sas_effect_level(r->rev_vol.left),
                     sas_effect_level(r->rev_vol.right)};
    if (evol[0] != rev_evol[0] || evol[1] != rev_evol[1]) {
        __sceSasRevEVOL(&ctx_storage, evol[0], evol[1]);
        rev_evol[0] = evol[0];
        rev_evol[1] = evol[1];
    }
    // SAS passes the wet bus through untouched when no effect is selected
    int wet = (r->spucnt & SPU_CTRL_MASK_REVERB_MASTER_ENABLE) &&
              rev_type != PSP_SAS_EFFECT_TYPE_OFF;
    if (wet != rev_wet) {
        __sceSasRevVON(&ctx_storage, 1, wet);
        rev_wet = wet;
    }
}

static void sas_reset(void) {
    for (int v = 0; v < PSYZ_SPU_NUM_VOICES; v++) {
        sas_silence(v);
        sas_voice[v].adsr_valid = 0;
        sas_voice[v].params_valid = 0;
    }
    __sceSasRevType(&ctx_storage, PSP_SAS_EFFECT_TYPE_OFF);
    __sceSasRevVON(&ctx_storage, 1, 0);
    __sceSasRevEVOL(&ctx_storage, 0, 0);
    rev_type = PSP_SAS_EFFECT_TYPE_OFF;
    rev_delay = rev_feedback = 0;
    rev_evol[0] = rev_evol[1] = 0;
    rev_wet = 0;
    rev_dirty = 1;
    sas_ended = 0;
    sas_reset_pending = 0;
}

static void sas_voice_ended(int v) {
    VoiceState* vs = &spu.voice[v];
    if (!vs->active || vs->key_off || (spu.tracked & (1u << v)))
        return;
    if (vs->env_state == ADSR_SUSTAIN && (vs->adsr_hi & 0x4000))
        return;
    vs->active = 0;
}

static void sas_dispatch(void) {
    if (!sas_ok)
        return;
    if (sas_reset_pending)
        sas_reset();
    u32 ended = sas_ended;
    sas_ended = 0;
    for (int v = 0; v < PSYZ_SPU_NUM_VOICES; v++) {
        SasVoice* sv = &sas_voice[v];
        if (sv->on && !sv->kon && (ended & (1u << v))) {
            sv->on = 0;
            sas_voice_ended(v);
        }
    }
    sas_reverb_sync();
    for (int v = 0; v < PSYZ_SPU_NUM_VOICES; v++) {
        SasVoice* sv = &sas_voice[v];
        if (sv->kon) {
            sv->kon = 0;
            sas_key_on(v);
            continue;
        }
        if (!sv->on)
            continue;
        if (!spu.voice[v].active) {
            sas_silence(v);
            continue;
        }
        if (sv->koff) {
            sv->koff = 0;
            __sceSasSetKeyOff(&ctx_storage, v);
        }
        sas_voice_params(v);
    }
}

static void sas_render(void) {
    if (sas_ok) {
        int ret = __sceSasCore(&ctx_storage, grain_storage);
        if (ret < 0) {
            WARNF("__sceSasCore failed: %08x", ret);
            memset(grain_storage, 0, GRAIN * N_CHANNELS * sizeof(short));
        }
        sas_ended = __sceSasGetEndFlag(&ctx_storage);
    } else {
        memset(grain_storage, 0, GRAIN * N_CHANNELS * sizeof(short));
    }
}

void Psyz_SpuSasPrerender(void) {
    Psyz_AudioLock();
    int render = spu.initialized && grain_state == GRAIN_EMPTY;
    unsigned epoch = spu_epoch;
    if (render) {
        sas_dispatch();
        grain_state = GRAIN_RENDERING;
    }
    Psyz_AudioUnlock();
    if (render) {
        sas_render();
        grain_epoch = epoch;
        grain_pos = 0;
        grain_state = GRAIN_READY;
    }
}

static void grain_acquire(void) {
    while (grain_state == GRAIN_RENDERING) {
        sceKernelDelayThread(100);
    }
    if (grain_state == GRAIN_READY && grain_epoch == spu_epoch) {
        return;
    }
    sas_dispatch();
    sas_render();
    grain_epoch = spu_epoch;
    grain_pos = 0;
    grain_state = GRAIN_READY;
}

typedef struct {
    // The SPU has a flag to know if it's enabled to pull samples from libcd
    int cd_on;

    // Don't sync with libcd output if there's nothing to pull from it.
    int cd_idle;

    int cd_vol_l, cd_vol_r;
    int main_l, main_r;

} Mixer;

static void mixer_init(Mixer* m) {
    volatile SPU_RXX* rxx = &_spu_RXX->rxx;
    m->cd_on = rxx->spucnt & SPU_CTRL_MASK_CD_AUDIO_ENABLE;
    m->cd_vol_l = rxx->cd_vol.left;
    m->cd_vol_r = rxx->cd_vol.right;
    m->main_l = clamp15(rxx->main_vol.left);
    m->main_r = clamp15(rxx->main_vol.right);
    m->cd_idle = 0;
}

static inline void cd_ring_pop(Mixer* m, int* left, int* right) {
    if (spu.cd_ring_count < CD_RING_LOW_WATER && !m->cd_idle) {
        size_t space = CD_RING_FRAMES - spu.cd_ring_count;
        size_t write_pos =
            (spu.cd_ring_read + spu.cd_ring_count) & CD_RING_MASK;
        size_t chunk = CD_RING_FRAMES - write_pos;
        if (chunk > space)
            chunk = space;
        size_t got = Psyz_CdPullSamples(&spu.cd_ring[write_pos * 2], chunk);
        spu.cd_ring_count += got;
        m->cd_idle = got == 0;
    }
    if (spu.cd_ring_count > 0) {
        *left = spu.cd_ring[spu.cd_ring_read * 2];
        *right = spu.cd_ring[spu.cd_ring_read * 2 + 1];
        spu.cd_ring_read = (spu.cd_ring_read + 1) & CD_RING_MASK;
        spu.cd_ring_count--;
    } else {
        *left = *right = 0;
    }
}

static inline void mix_frame(
    const Mixer* m, short* out, const short* sas, int cd_l, int cd_r) {
    int left = sas[0], right = sas[1];
    if (m->cd_on) {
        left += (cd_l * m->cd_vol_l) >> 15;
        right += (cd_r * m->cd_vol_r) >> 15;
    }
    out[0] = clamp16((left * m->main_l) >> 14);
    out[1] = clamp16((right * m->main_r) >> 14);
}

static void capture_advance(unsigned frames) {
    unsigned end = spu.capture_pos + 2 * frames;
    if (((end >> 9) - (spu.capture_pos >> 9)) & 1) {
        _spu_RXX->rxx.spustat ^= 1u << 11;
    }
    spu.capture_pos = end & 0x3FF;
}

static void write_capture(unsigned int idx, short val) {
    *(short*)&ram_storage[(idx * 0x400) | spu.capture_pos] = val;
}

static short tracked_step(int v) {
    VoiceState* vs = &spu.voice[v];
    if (!vs->active)
        return 0;
    short s = spu_voice_step(vs, _spu_RXX->rxx.voice[v].pitch, ram_storage);
    spu_voice_envelope_step(vs);
    if (vs->env_state == ADSR_OFF)
        return 0;
    return (short)((s * vs->env_vol) >> 15);
}

static void mix_with_capture(Mixer* m, short* out, const short* sas, int n) {
    for (int i = 0; i < n; i++) {
        int cd_l, cd_r;
        cd_ring_pop(m, &cd_l, &cd_r);
        mix_frame(m, &out[i * 2], &sas[i * 2], cd_l, cd_r);
        short v1 = (spu.tracked & (1u << 1)) ? tracked_step(1) : 0;
        short v3 = (spu.tracked & (1u << 3)) ? tracked_step(3) : 0;
        write_capture(0, (short)cd_l);
        write_capture(1, (short)cd_r);
        write_capture(2, v1);
        write_capture(3, v3);
        capture_advance(1);
    }
}

static void mix(Mixer* m, short* out, const short* sas, int n) {
    int i = 0;
    for (; i < n && (spu.cd_ring_count || !m->cd_idle); i++) {
        int cd_l, cd_r;
        cd_ring_pop(m, &cd_l, &cd_r);
        mix_frame(m, &out[i * 2], &sas[i * 2], cd_l, cd_r);
    }
    for (; i < n; i++) {
        mix_frame(m, &out[i * 2], &sas[i * 2], 0, 0);
    }
    capture_advance(n);
}

static void spu_consume(short* out, const short* sas, int frames) {
    int capture = spu.capture_left > 0;
    u32 was_active = 0;
    for (int v = 0; v < PSYZ_SPU_NUM_VOICES; v++) {
        if (spu.voice[v].active)
            was_active |= 1u << v;
    }
    Mixer m;
    mixer_init(&m);
    if (capture) {
        mix_with_capture(&m, out, sas, frames);
    } else {
        mix(&m, out, sas, frames);
    }
    for (int v = 0; v < PSYZ_SPU_NUM_VOICES; v++) {
        if (!(was_active & (1u << v)))
            continue;
        if (!(capture && (spu.tracked & (1u << v)))) {
            spu_voice_envelope_advance(&spu.voice[v], frames);
        }
        _spu_RXX->rxx.voice[v].volumex = (unsigned short)spu.voice[v].env_vol;
    }
    if (capture) {
        if (spu.capture_left > (unsigned)frames) {
            spu.capture_left -= frames;
        } else {
            spu.capture_left = 0;
            spu.tracked = 0;
        }
    }
}

void Psyz_SpuPullSamples(short* out, int num_frames) {
    if (!spu.initialized) {
        memset(out, 0, num_frames * N_CHANNELS * sizeof(short));
        return;
    }
    int done = 0;
    while (done < num_frames) {
        grain_acquire();
        int frames = num_frames - done;
        if (frames > GRAIN - (int)grain_pos)
            frames = GRAIN - grain_pos;
        spu_consume(&out[done * N_CHANNELS],
                    &grain_storage[grain_pos * N_CHANNELS], frames);
        grain_pos += frames;
        done += frames;
        if (grain_pos == GRAIN) {
            grain_state = GRAIN_EMPTY;
        }
    }
}
