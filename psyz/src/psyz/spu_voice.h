#ifndef PSYZ_SPU_VOICE_H
#define PSYZ_SPU_VOICE_H

#include <psyz.h>
#include "../../decomp/src/libspu/libspu_private.h"
#include "spu_gauss.h"

// One ADPCM block decodes to 28 samples
#define ADPCM_BLOCK_BYTES 16
#define ADPCM_BLOCK_SAMPLES 28
#define ENV_KEYON_DELAY_TICKS 6 // latency to simulate PS1 SPU latching a key-on

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
static inline void spu_adpcm_decode_block(
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
            int s = t * (1 << shift) + ((prev * f0) >> 6) + ((prev2 * f1) >> 6);
            short final = clamp16(s);
            out[i * 2 + n] = final;
            prev2 = prev;
            prev = final;
        }
    }
    *hist1 = prev;
    *hist2 = prev2;
}

typedef enum {
    ADSR_ATTACK = 0,
    ADSR_DECAY,
    ADSR_SUSTAIN,
    ADSR_RELEASE,
    ADSR_OFF,
} AdsrState;

typedef struct {
    unsigned cur_addr;    // current 16-byte block address in SPU RAM
    unsigned repeat_addr; // from voice loop_addr reg, or block flag bit 2
    short hist1, hist2;
    short samples[ADPCM_BLOCK_SAMPLES];
    u8 block_flags; // flags byte of the most-recently-decoded block
    u8 sample_idx;  // next sample to consume from samples[] (0..28)
    unsigned spos;  // 16.16 fixed-point counter (1.0 = 0x10000)
    unsigned sinc;  // pitch_reg << 4 is the per-output-tick increment
    short gwin[4];  // GAUSS interpolation window
    u8 gpos;        // index to next decoded sample in gwin
    u8 active;
    u8 needs_decode;
    int env_vol;          // mirrors SPU_VOICE_REG::volumex
    unsigned env_counter; // rate divider, increases by 1 for all voice duration
    AdsrState env_state;  // voice phase, works like a state machine
    u8 key_off;           // set by key-off; drives the release phase
    u8 delay_ticks;       // key-on -> envelope-start pipeline latency
    u16 adsr_lo;          // mirrors SPU_VOICE_REG::adsr[0]
    u16 adsr_hi;          // mirrors SPU_VOICE_REG::adsr[1]
} VoiceState;

static inline void spu_voice_key_on(
    VoiceState* vs, volatile SPU_VOICE_REG* reg) {
    vs->cur_addr = ((unsigned)reg->addr << 3) & (PSYZ_SPU_RAM_SIZE - 1);
    vs->repeat_addr = ((unsigned)reg->loop_addr << 3) & (PSYZ_SPU_RAM_SIZE - 1);
    vs->hist1 = vs->hist2 = 0;
    vs->sample_idx = ADPCM_BLOCK_SAMPLES; // force decode on first consume
    vs->block_flags = 0;
    vs->needs_decode = 1;
    // sinc = pitch << 4; pitch=0x1000 -> sinc=0x10000 (1.0 in 16.16)
    unsigned pitch = reg->pitch;
    vs->sinc = (pitch & 0x3FFF) << 4;
    if (vs->sinc == 0)
        vs->sinc = 1;

    // Start spos at 1.0 so the first voice_step consumes exactly one sample.
    // The gauss window stays mostly zero-padded for several ticks, producing
    // the silent warmup-then-ringing pattern observed in PS1 captures
    vs->spos = 0x10000;

    // We do not reset vs->gpos at key-on for accuracy. On real PS1 hardware,
    // a voice on will carry the residual state from prior voice activity
    vs->active = 1;

    // reset the envelope to a fresh attack from zero
    vs->adsr_lo = reg->adsr[0];
    vs->adsr_hi = reg->adsr[1];
    vs->env_vol = 0;
    vs->env_counter = 0;
    vs->env_state = ADSR_ATTACK;
    vs->key_off = 0;
    vs->delay_ticks = ENV_KEYON_DELAY_TICKS;
    reg->volumex = 0;
}

static inline int spu_voice_decode_one_sample(VoiceState* vs, const u8* ram) {
    if (vs->sample_idx >= ADPCM_BLOCK_SAMPLES) {
        // end of block, and handle loop/end flags
        if (!vs->needs_decode && (vs->block_flags & 0x01)) {
            if (!(vs->block_flags & 0x02)) {
                vs->active = 0;
                vs->gwin[vs->gpos] = 0;
                vs->gpos = (vs->gpos + 1) & 3;
                return 0;
            }
            vs->cur_addr = vs->repeat_addr;
        }
        vs->needs_decode = 1;
        vs->sample_idx = 0;
    }
    if (vs->needs_decode) {
        const u8* block = &ram[vs->cur_addr];
        u8 wrapped[ADPCM_BLOCK_BYTES];
        if (vs->cur_addr > PSYZ_SPU_RAM_SIZE - ADPCM_BLOCK_BYTES) {
            for (int i = 0; i < ADPCM_BLOCK_BYTES; i++)
                wrapped[i] = ram[(vs->cur_addr + i) & (PSYZ_SPU_RAM_SIZE - 1)];
            block = wrapped;
        }
        spu_adpcm_decode_block(
            block, &vs->hist1, &vs->hist2, vs->samples, &vs->block_flags);
        if (vs->block_flags & 0x04) {
            vs->repeat_addr = vs->cur_addr;
        }
        vs->cur_addr =
            (vs->cur_addr + ADPCM_BLOCK_BYTES) & (PSYZ_SPU_RAM_SIZE - 1);
        vs->needs_decode = 0;
    }
    short s = vs->samples[vs->sample_idx++];
    vs->gwin[vs->gpos] = s;
    vs->gpos = (vs->gpos + 1) & 3;
    return 1;
}

// Advance the voice by one output-rate tick (44.1 kHz) and return its
// pitch-resampled, gauss-interpolated short sample.
static inline short spu_voice_step(
    VoiceState* vs, unsigned pitch_reg, const u8* ram) {
    // for pitch changes during voice on, enable vibrato or bends
    unsigned pitch = pitch_reg & 0x3FFF;
    vs->sinc = pitch ? pitch << 4 : 1;

    // consume decoded samples until the pitch counter is below 1.0
    while (vs->spos >= 0x10000) {
        if (!spu_voice_decode_one_sample(vs, ram)) {
            // Voice stopped mid-decode; flush remaining ticks as zero.
            vs->spos = 0;
            break;
        }
        vs->spos -= 0x10000;
    }

    // GAUSS interpolation
    int vl = (vs->spos >> 6) & ~3;
    int g0 = vs->gwin[vs->gpos & 3];
    int g1 = vs->gwin[(vs->gpos + 1) & 3];
    int g2 = vs->gwin[(vs->gpos + 2) & 3];
    int g3 = vs->gwin[(vs->gpos + 3) & 3];
    int acc = (spu_gauss_tbl[vl + 0] * g0) & ~2047;
    acc += (spu_gauss_tbl[vl + 1] * g1) & ~2047;
    acc += (spu_gauss_tbl[vl + 2] * g2) & ~2047;
    acc += (spu_gauss_tbl[vl + 3] * g3) & ~2047;
    vs->spos += vs->sinc;
    // each tap keeps 11 fractional bits and the taps sum to 0x800: unity gain
    return clamp16(acc >> 11);
}

static inline unsigned adsr_denominator(int rate) {
    return rate < 48 ? 1u : (1u << ((rate >> 2) - 11));
}

static inline int adsr_num_increase(int rate) {
    return rate < 48 ? (7 - (rate & 3)) << (11 - (rate >> 2))
                     : (7 - (rate & 3));
}

static inline int adsr_num_decrease(int rate) {
    // (-8 + (rate & 3)) is always negative; shifting it left is undefined,
    // so scale by the equivalent power of two instead.
    return rate < 48 ? (-8 + (rate & 3)) * (1 << (11 - (rate >> 2)))
                     : (-8 + (rate & 3));
}

// The rate driving the current phase; it only changes when the phase fires.
static inline int spu_voice_envelope_rate(const VoiceState* vs) {
    switch (vs->env_state) {
    case ADSR_ATTACK: {
        int rate = (vs->adsr_lo >> 8) & 0x7F;
        if ((vs->adsr_lo & 0x8000) && vs->env_vol >= 0x6000)
            rate += 8;
        return rate;
    }
    case ADSR_DECAY:
        return ((vs->adsr_lo >> 4) & 0x0F) * 4;
    case ADSR_SUSTAIN: {
        int rate = (vs->adsr_hi >> 6) & 0x7F;
        if (!(vs->adsr_hi & 0x4000) && (vs->adsr_hi & 0x8000) &&
            vs->env_vol >= 0x6000)
            rate += 8;
        return rate;
    }
    case ADSR_RELEASE:
        return (vs->adsr_hi & 0x1F) * 4;
    default:
        return 0;
    }
}

// process voice ADSR envelope by one sample, calculate ADSR envelope volume
static inline void spu_voice_envelope_step(VoiceState* vs) {
    if (vs->delay_ticks) { // simulate PS1 SPU key on latency
        vs->delay_ticks--;
        return;
    }
    if (vs->key_off && vs->env_state != ADSR_OFF) {
        vs->env_state = ADSR_RELEASE;
    }

    // the PS1 SPU has an internal counter, shared across all states, that
    // allows to trigger a change based on the selected ADSR rate
    unsigned ctr = ++vs->env_counter;
#define ADSR_FIRES(rate) ((ctr % adsr_denominator(rate)) == 0)

    switch (vs->env_state) {
    case ADSR_ATTACK: {
        int attack_rate = (vs->adsr_lo >> 8) & 0x7F;
        int attack_exp = (vs->adsr_lo >> 15) & 1;
        int rate = attack_rate;
        if (attack_exp && vs->env_vol >= 0x6000) {
            rate += 8;
        }
        if (ADSR_FIRES(rate)) {
            vs->env_vol += adsr_num_increase(rate);
            if (vs->env_vol >= 0x7FFF) {
                vs->env_vol = 0x7FFF;
                vs->env_state = ADSR_DECAY;
            }
        }
        break;
    }
    case ADSR_DECAY: {
        int decay_rate = (vs->adsr_lo >> 4) & 0x0F;
        int rate = decay_rate * 4; // decay is always exponential decrease
        if (ADSR_FIRES(rate)) {
            vs->env_vol += (adsr_num_decrease(rate) * vs->env_vol) >> 15;
            if (vs->env_vol < 0) {
                vs->env_vol = 0;
            }
            int sustain_level = vs->adsr_lo & 0x0F;
            if (((vs->env_vol >> 11) & 0xF) <= sustain_level) {
                vs->env_state = ADSR_SUSTAIN;
            }
        }
        break;
    }
    case ADSR_SUSTAIN: {
        int sustain_rate = (vs->adsr_hi >> 6) & 0x7F;
        int sustain_dec = (vs->adsr_hi >> 14) & 1;
        int sustain_exp = (vs->adsr_hi >> 15) & 1;
        int rate = sustain_rate;
        if (!sustain_dec) {
            if (sustain_exp && vs->env_vol >= 0x6000) {
                rate += 8;
            }
            if (ADSR_FIRES(rate)) {
                vs->env_vol += adsr_num_increase(rate);
                if (vs->env_vol > 0x7FFF) {
                    vs->env_vol = 0x7FFF;
                }
            }
        } else {
            if (ADSR_FIRES(rate)) {
                if (sustain_exp) {
                    vs->env_vol +=
                        (adsr_num_decrease(rate) * vs->env_vol) >> 15;
                } else {
                    vs->env_vol += adsr_num_decrease(rate);
                }
                if (vs->env_vol < 0) {
                    vs->env_vol = 0;
                }
            }
        }
        break;
    }
    case ADSR_RELEASE: {
        int release_rate = vs->adsr_hi & 0x1F;
        int rate = release_rate * 4;
        if (ADSR_FIRES(rate)) {
            int release_exp = (vs->adsr_hi >> 5) & 1;
            if (release_exp) {
                vs->env_vol += (adsr_num_decrease(rate) * vs->env_vol) >> 15;
            } else {
                vs->env_vol += adsr_num_decrease(rate);
            }
            if (vs->env_vol <= 0) {
                vs->env_vol = 0;
                vs->env_state = ADSR_OFF;
                vs->active = 0;
            }
        }
        break;
    }
    default:
        break;
    }
#undef ADSR_FIRES
}

// Up to n exponential firings of env -= ceil(q * env / 32768), stopping below
// floor. Runs of firings with the same step are applied at once.
static inline unsigned adsr_exp_fire(
    int* env_io, int q, unsigned n, int floor) {
    int env = *env_io;
    unsigned fired = 0;
    while (fired < n) {
        int c = (q * env + 32767) >> 15;
        unsigned steps = 1;
        if (env >= floor && c > 0) {
            int bottom = ((c - 1) << 15) / q;
            steps = (unsigned)(env - bottom - 1) / c + 1;
            unsigned below = (unsigned)(env - floor) / c + 1;
            if (below < steps)
                steps = below;
        }
        if (steps > n - fired)
            steps = n - fired;
        env -= (int)steps * c;
        fired += steps;
        if (env < floor)
            break;
    }
    *env_io = env;
    return fired;
}

// Applies up to n linear firings of step towards limit, stopping on reaching
// it.
static inline unsigned adsr_linear_fire(
    int* env_io, int step, int limit, unsigned n) {
    int dist = step > 0 ? limit - *env_io : *env_io - limit;
    int mag = step > 0 ? step : -step;
    unsigned need = dist > 0 ? (unsigned)(dist + mag - 1) / mag : 1;
    unsigned fired = need < n ? need : n;
    *env_io += (int)fired * step;
    return fired;
}

// Runs up to n firings of the current phase, returning how many it took
// before the phase or its rate changed.
static inline unsigned spu_voice_envelope_fire(
    VoiceState* vs, int rate, unsigned n) {
    unsigned fired;
    switch (vs->env_state) {
    case ADSR_ATTACK: {
        int limit =
            (vs->adsr_lo & 0x8000) && vs->env_vol < 0x6000 ? 0x6000 : 0x7FFF;
        fired =
            adsr_linear_fire(&vs->env_vol, adsr_num_increase(rate), limit, n);
        if (vs->env_vol >= 0x7FFF) {
            vs->env_vol = 0x7FFF;
            vs->env_state = ADSR_DECAY;
        }
        return fired;
    }
    case ADSR_DECAY: {
        int level = ((vs->adsr_lo & 0x0F) + 1) << 11;
        fired = adsr_exp_fire(&vs->env_vol, -adsr_num_decrease(rate), n, level);
        if (vs->env_vol < level) {
            vs->env_state = ADSR_SUSTAIN;
        }
        return fired;
    }
    case ADSR_SUSTAIN:
        if (!(vs->adsr_hi & 0x4000)) {
            int limit = (vs->adsr_hi & 0x8000) && vs->env_vol < 0x6000
                            ? 0x6000
                            : 0x7FFF;
            fired = adsr_linear_fire(
                &vs->env_vol, adsr_num_increase(rate), limit, n);
            if (vs->env_vol > 0x7FFF) {
                vs->env_vol = 0x7FFF;
            }
        } else if (vs->adsr_hi & 0x8000) {
            fired = adsr_exp_fire(&vs->env_vol, -adsr_num_decrease(rate), n, 1);
        } else {
            fired =
                adsr_linear_fire(&vs->env_vol, adsr_num_decrease(rate), 0, n);
            if (vs->env_vol < 0) {
                vs->env_vol = 0;
            }
        }
        return fired;
    case ADSR_RELEASE:
        if (vs->adsr_hi & 0x20) {
            fired = adsr_exp_fire(&vs->env_vol, -adsr_num_decrease(rate), n, 1);
        } else {
            fired =
                adsr_linear_fire(&vs->env_vol, adsr_num_decrease(rate), 0, n);
        }
        if (vs->env_vol <= 0) {
            vs->env_vol = 0;
            vs->env_state = ADSR_OFF;
            vs->active = 0;
        }
        return fired;
    default:
        return n;
    }
}

// Same result as `ticks` calls to spu_voice_envelope_step, in firings rather
// than ticks.
static inline void spu_voice_envelope_advance(VoiceState* vs, unsigned ticks) {
    while (ticks && vs->active) {
        if (vs->delay_ticks) {
            unsigned n = vs->delay_ticks < ticks ? vs->delay_ticks : ticks;
            vs->delay_ticks -= n;
            ticks -= n;
            continue;
        }
        if (vs->key_off && vs->env_state != ADSR_OFF) {
            vs->env_state = ADSR_RELEASE;
        }
        int held = vs->env_state == ADSR_OFF ||
                   (vs->env_state == ADSR_SUSTAIN &&
                    ((vs->adsr_hi & 0x4000) ? vs->env_vol == 0
                                            : vs->env_vol == 0x7FFF));
        if (held) {
            vs->env_counter += ticks;
            return;
        }
        int rate = spu_voice_envelope_rate(vs);
        unsigned den = adsr_denominator(rate);
        unsigned first = den - (vs->env_counter & (den - 1));
        if (first > ticks) {
            vs->env_counter += ticks;
            return;
        }
        unsigned fired =
            spu_voice_envelope_fire(vs, rate, 1 + (ticks - first) / den);
        unsigned used = first + (fired - 1) * den;
        vs->env_counter += used;
        ticks -= used;
    }
}

#endif
