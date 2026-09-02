#include <common.h>
#include <libspu.h>

inline u32 _spu_2pitch(u32 center, u32 note) {
    u32 p0 = center << 12;
    u32 factor = 0x103B;
    u32 p1 = center * factor;
    s32 s = note / 32;
    s32 f = note % 32;
    s32 i;

    for (i = 0; i < s; i++) {
        p0 = center * factor;
        factor *= 0x103B;
        factor >>= 12;
        p1 = center * factor;
    }

    return (p0 + ((p1 - p0) >> 5) * f) >> 12;
}

u16 _spu_note2pitch(u16 center_note, u16 center_fine, u16 note, u16 fine) {
    s32 diff;
    s32 abs_diff;
    s32 octave;
    s32 note_rem;
    u16 base;
    u32 pitch;
    s32 center;
    s32 val;

    center = (center_note << 7) + center_fine;
    val = (note << 7) + fine;
    diff = val - center;
    abs_diff = diff < 0 ? -diff : diff;

    octave = abs_diff / 1536;
    note_rem = abs_diff % 1536;

    if (diff >= 0) {
        base = 0x1000 << octave;
    } else {
        if (note_rem != 0) {
            octave += 1;
            note_rem = 0x600 - note_rem;
        }
        base = 0x1000 >> octave;
    }

    pitch = _spu_2pitch(base, note_rem < 0 ? -note_rem : note_rem);

    if (pitch >= 0x4000) {
        pitch = 0x3FFF;
    }

    return pitch;
}

s32 _spu_pitch2note(u16 center_note, u16 center_fine, u16 pitch) {
    s32 lsb;
    s32 i;
    u16 inv;
    s32 octave;
    u32 mask;
    u32 t0;
    u32 val1, val2;
    u32 step;
    s32 j;
    u32 base;
    u32 acc, acc_step;
    s32 val;
    s32 s, f;
    s32 note, fine;

    inv = ~pitch;
    lsb = 0;
    for (i = 15; i >= 0; i--) {
        if (!((inv >> i) & 1)) {
            lsb = i;
            break;
        }
    }

    octave = lsb - 12;
    mask = 1 << lsb;
    t0 = 0x1000;
    for (i = 0; i < 48; i++) {
        val1 = mask * t0;
        t0 *= 0x103B;
        t0 >>= 12;
        val2 = mask * t0;
        j = 0;
        base = i << 5;
        acc = 0;
        step = (val2 - val1) >> 5;
        acc_step = step;
        for (; j < 32; j++) {
            u32 p0 = val1 + acc;
            u32 p1 = val1 + acc_step;
            p0 >>= 12;
            p1 >>= 12;
            if (pitch >= p0 && pitch < p1) {
                val = base + j;
                goto out;
            }
            acc_step += step;
            acc += step;
        }
    }
    val = 0x600;

out:
    s = val / 128;
    f = val % 128;
    note = center_note + s + octave * 12;
    fine = center_fine + f;
    return (note << 8) | fine;
}
