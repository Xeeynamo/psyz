#include "mdec.h"
#include <string.h>
#include "../internal.h"

#define HALFWORD(p) ((unsigned)(p)[0] | (unsigned)(p)[1] << 8)
#define SIGNED10(v) ((int)((v) & 0x3FF) - (int)((v) & 0x200) * 2)
#define WRAP(v, bits)                                                          \
    ((int32_t)(((uint32_t)(v) + (1u << ((bits) - 1))) &                        \
               ((1u << (bits)) - 1)) -                                         \
     (1 << ((bits) - 1)))
#define ODD_COEFFICIENT(magnitude)                                             \
    ((magnitude) > 1024 ? 2047 : 2 * (int32_t)(magnitude) - 1)
#define COEFFICIENT(level, magnitude)                                          \
    ((magnitude) == 0 ? 0                                                      \
     : (level) < 0    ? -ODD_COEFFICIENT(magnitude)                            \
                      : ODD_COEFFICIENT(magnitude))
#define COLOR(luma, chroma) WRAP((luma) * 256 + (chroma), 17)

typedef struct {
    uint8_t luma[64], chroma[64];
    int16_t vertical[64], horizontal[64];
} MdecTables;

static MdecTables tables, transfer_tables;
static uint8_t input[UINT16_MAX * 4];
static size_t input_pos, input_size;

static const uint8_t scan[64] = {
    0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

static uint32_t command;
static int decode_error;
static uint8_t macroblock[768];
static size_t pixel_pos, pixel_size;

static void idct(int8_t out[64], const uint8_t positions[64],
                 const int16_t values[64], unsigned count) {
    int32_t columns[64] = {0};
    unsigned used_columns = 0;
    for (unsigned i = 0; i < count; ++i) {
        unsigned v = positions[i] >> 3, u = positions[i] & 7;
        const int16_t* scale = transfer_tables.vertical + v * 8;
        int32_t* column = columns + u * 8;
        for (unsigned y = 0; y < 8; ++y)
            column[y] += values[i] * scale[y] >> 7;
        used_columns |= 1u << u;
    }
    unsigned column_count = 0;
    uint8_t column_list[8];
    for (unsigned u = 0; u < 8; ++u) {
        if (!(used_columns >> u & 1))
            continue;
        column_list[column_count++] = u;
        for (unsigned y = 0; y < 8; ++y)
            columns[u * 8 + y] = WRAP(columns[u * 8 + y] >> 4, 13);
    }
    for (unsigned y = 0; y < 8; ++y) {
        int32_t row[8] = {0};
        for (unsigned i = 0; i < column_count; ++i) {
            unsigned u = column_list[i];
            int32_t t = columns[u * 8 + y];
            const int16_t* scale = transfer_tables.horizontal + u * 8;
            for (unsigned x = 0; x < 8; ++x)
                row[x] += t * scale[x] >> 7;
        }
        for (unsigned x = 0; x < 8; ++x)
            out[y * 8 + x] = CLAMP((WRAP(row[x], 17) + 128) >> 8, -128, 127);
    }
}

static int decode_block(int8_t out[64], const uint8_t quant[64]) {
    uint8_t positions[64];
    int16_t values[64];
    unsigned count = 0, halfword;
    do {
        if (input_pos == input_size)
            return -1;
        halfword = HALFWORD(input + input_pos);
        input_pos += 2;
    } while (halfword == 0xfe00);
    unsigned q_scale = halfword >> 10;
    int level = SIGNED10(halfword);
    unsigned magnitude = (unsigned)(level < 0 ? -level : level);
    positions[count] = 0;
    values[count++] =
        q_scale ? COEFFICIENT(level, magnitude * quant[0]) : level * 4;
    for (unsigned index = 0; index < 63;) {
        if (input_pos == input_size)
            return -1;
        halfword = HALFWORD(input + input_pos);
        index += (halfword >> 10) + 1;
        if (index >= 64)
            break;
        input_pos += 2;
        level = SIGNED10(halfword);
        magnitude = (unsigned)(level < 0 ? -level : level);
        magnitude =
            (magnitude * quant[index] * q_scale + (level < 0 ? 7 : 4)) >> 3;
        positions[count] = q_scale ? scan[index] : index;
        values[count++] = q_scale ? COEFFICIENT(level, magnitude) : level * 4;
    }
    idct(out, positions, values, count);
    return 0;
}

static size_t write_mono(
    const int8_t samples[64], unsigned depth, int signed_output) {
    if (depth == 1) {
        for (unsigned i = 0; i < 64; ++i)
            macroblock[i] = (uint8_t)samples[i] ^ (signed_output ? 0 : 0x80);
        return 64;
    }
    for (unsigned i = 0; i < 32; ++i) {
        unsigned lo = CLAMP((samples[i * 2] + 8) >> 4, -8, 7) & 15;
        unsigned hi = CLAMP((samples[i * 2 + 1] + 8) >> 4, -8, 7) & 15;
        macroblock[i] = (lo | hi << 4) ^ (signed_output ? 0 : 0x88);
    }
    return 32;
}

static size_t write_color(
    int8_t blocks[6][64], unsigned depth, int signed_output, int bit15) {
    unsigned byte_flip = signed_output ? 0 : 0x80,
             rgb555_flip = signed_output ? 0 : 0x4210;
    unsigned rgb555_bit15 = bit15 ? 0x8000 : 0;
    for (unsigned cy = 0; cy < 8; ++cy) {
        for (unsigned cx = 0; cx < 8; ++cx) {
            int cr = blocks[0][cy * 8 + cx], cb = blocks[1][cy * 8 + cx];
            int red = 359 * cr, blue = 454 * cb;
            int green = -88 * cb + (-183 * cr & ~31);
            for (unsigned dy = 0; dy < 2; ++dy) {
                for (unsigned dx = 0; dx < 2; ++dx) {
                    unsigned y = cy * 2 + dy, x = cx * 2 + dx;
                    const int8_t* luma = blocks[2 + (y >> 3) * 2 + (x >> 3)];
                    int sample = luma[(y & 7) * 8 + (x & 7)];
                    int32_t r = COLOR(sample, red), g = COLOR(sample, green);
                    int32_t b = COLOR(sample, blue);
                    unsigned p = y * 16 + x;
                    if (depth == 2) {
                        macroblock[p * 3] =
                            (uint8_t)CLAMP((r + 128) >> 8, -128, 127) ^
                            byte_flip;
                        macroblock[p * 3 + 1] =
                            (uint8_t)CLAMP((g + 128) >> 8, -128, 127) ^
                            byte_flip;
                        macroblock[p * 3 + 2] =
                            (uint8_t)CLAMP((b + 128) >> 8, -128, 127) ^
                            byte_flip;
                    } else {
                        unsigned packed =
                            (CLAMP((r + 1024) >> 11, -16, 15) & 31) |
                            (CLAMP((g + 1024) >> 11, -16, 15) & 31) << 5 |
                            (CLAMP((b + 1024) >> 11, -16, 15) & 31) << 10;
                        packed = (packed ^ rgb555_flip) | rgb555_bit15;
                        macroblock[p * 2] = (uint8_t)packed;
                        macroblock[p * 2 + 1] = (uint8_t)(packed >> 8);
                    }
                }
            }
        }
    }
    return depth == 2 ? 768 : 512;
}

static int next_macroblock(void) {
    int8_t blocks[6][64];
    unsigned depth = (command >> 27) & 3;
    int signed_output = (command >> 26) & 1;
    pixel_pos = 0;
    if (depth < 2) {
        if (decode_block(blocks[0], transfer_tables.luma))
            return -1;
        pixel_size = write_mono(blocks[0], depth, signed_output);
        return 0;
    }
    for (unsigned i = 0; i < 6; ++i)
        if (decode_block(blocks[i],
                         i < 2 ? transfer_tables.chroma : transfer_tables.luma))
            return -1;
    pixel_size = write_color(blocks, depth, signed_output, (command >> 25) & 1);
    return 0;
}

void Psyz_MdecReset(void) {
    input_pos = input_size = pixel_pos = pixel_size = 0;
    command = 0;
    decode_error = 0;
}

int Psyz_MdecCommand(uint32_t value, const void* data, size_t words) {
    const uint8_t* bytes = data;
    unsigned opcode = value >> 29;
    size_t expected = opcode == 2   ? ((value & 1) ? 32 : 16)
                      : opcode == 3 ? 32
                                    : value & UINT16_MAX;
    if (opcode < 1 || opcode > 3 || words != expected || (!data && words)) {
        input_pos = input_size = pixel_pos = pixel_size = 0;
        decode_error = 1;
        return -1;
    }
    if (opcode == 2) {
        memcpy(tables.luma, bytes, 64);
        if (value & 1)
            memcpy(tables.chroma, bytes + 64, 64);
        return 0;
    }
    if (opcode == 3) {
        for (unsigned i = 0; i < 64; ++i) {
            int scale = (int16_t)HALFWORD(bytes + i * 2);
            tables.vertical[i] = (int16_t)(scale >> 3);
            tables.horizontal[i] = (int16_t)(scale >> 4);
        }
        return 0;
    }
    if (words) {
        memcpy(input, bytes, words * 4);
    }
    input_pos = 0;
    input_size = words * 4;
    transfer_tables = tables;
    command = value;
    pixel_pos = pixel_size = 0;
    decode_error = 0;
    return 0;
}

int Psyz_MdecRead(void* data, size_t words) {
    if ((!data && words) || words > SIZE_MAX / 4) {
        decode_error = 1;
        return -1;
    }
    uint8_t* dst = data;
    size_t left = words * 4;
    while (left) {
        if (decode_error || (pixel_pos == pixel_size && next_macroblock())) {
            memset(dst, 0, left);
            decode_error = 1;
            return -1;
        }
        size_t count = pixel_size - pixel_pos;
        if (count > left)
            count = left;
        memcpy(dst, macroblock + pixel_pos, count);
        pixel_pos += count;
        dst += count;
        left -= count;
    }
    return 0;
}
