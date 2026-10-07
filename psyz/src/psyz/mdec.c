#include "mdec.h"
#include <string.h>

static struct MdecTables {
    uint8_t luma[64], chroma[64];
    int16_t scale[64];
} tables, transfer_tables;
static uint16_t input_transfer[UINT16_MAX * 2];
static const uint16_t* input;
static size_t input_left;
static uint32_t command;
static int decode_error;
static uint8_t macroblock[768];
static size_t pixel_pos, pixel_size;

static const uint8_t scan[64] = {
    0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};
static int clamp(int v, int lo, int hi) {
    return v < lo ? lo : v > hi ? hi : v;
}
static int signed10(unsigned v) {
    return (int)(v & 1023) - ((v & 512) ? 1024 : 0);
}
static int signed9(int v) { return ((v + 256) & 511) - 256; }

static int64_t floor_shift(int64_t v, int n) {
    return v >= 0 ? v / ((int64_t)1 << n)
                  : -((-v + (((int64_t)1 << n) - 1)) / ((int64_t)1 << n));
}
static int decode_block(int8_t out[64], const uint8_t quant[64]) {
    int32_t coefficients[64] = {0}, scratch[64];
    unsigned word;
    do {
        if (!input_left)
            return -1;
        word = *input++;
        --input_left;
    } while (word == 0xfe00);
    int scale = word >> 10;
    unsigned index = 0;
    for (;;) {
        int level = signed10(word);
        int q = index ? quant[index] * scale : quant[0];
        int value =
            scale == 0 || q == 0
                ? level * 32
                : (int)(index ? floor_shift(level * q, 3) : level * q) * 16 +
                      (level > 0   ? -8
                       : level < 0 ? 8
                                   : 0);
        coefficients[scale ? scan[index] : index] = clamp(value, -16384, 16383);
        if (index == 63)
            break;
        if (!input_left)
            return -1;
        word = *input++;
        --input_left;
        if (word == 0xfe00)
            break;
        index += (word >> 10) + 1;
        if (index >= 64)
            return -1;
    }
    for (int pass = 0; pass < 2; ++pass) {
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                int64_t sum = 0;
                for (int freq = 0; freq < 8; ++freq)
                    sum +=
                        (int64_t)coefficients[row + freq * 8] *
                        floor_shift(transfer_tables.scale[col + freq * 8], 3);
                int value = (int)floor_shift(sum + 16384, 15);
                scratch[row * 8 + col] = value;
            }
        }
        memcpy(coefficients, scratch, sizeof(scratch));
    }
    for (int i = 0; i < 64; ++i)
        out[i] = clamp(signed9(scratch[i]), -128, 127);
    return 0;
}
static int next_macroblock(void) {
    int8_t samples[6][64];
    unsigned depth = (command >> 27) & 3;
    unsigned signed_output = (command >> 26) & 1;
    if (depth < 2) {
        if (decode_block(samples[0], transfer_tables.luma))
            return -1;
        for (int i = 0; i < 64; ++i) {
            if (depth == 1) {
                macroblock[i] =
                    (uint8_t)samples[0][i] ^ (signed_output ? 0 : 0x80);
            } else {
                uint8_t value = (uint8_t)clamp(samples[0][i] + 8, -128, 127);
                value = (value >> 4) ^ (signed_output ? 0 : 8);
                if (i & 1)
                    macroblock[i / 2] |= value << 4;
                else
                    macroblock[i / 2] = value;
            }
        }
        pixel_pos = 0;
        pixel_size = depth == 1 ? 64 : 32;
        return 0;
    }
    for (int i = 0; i < 6; ++i)
        if (decode_block(samples[i],
                         i < 2 ? transfer_tables.chroma : transfer_tables.luma))
            return -1;
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            int luma = samples[2 + (y / 8) * 2 + x / 8][(y % 8) * 8 + x % 8];
            int cr = samples[0][(y / 2) * 8 + x / 2];
            int cb = samples[1][(y / 2) * 8 + x / 2];
            int r = clamp(signed9(luma + (int)floor_shift(cr * 359 + 128, 8)),
                          -128, 127) +
                    128;
            int g =
                clamp(
                    signed9(luma + (int)floor_shift(
                                       floor_shift(-88 * cb, 5) * 32 +
                                           floor_shift(-183 * cr, 3) * 8 + 128,
                                       8)),
                    -128, 127) +
                128;
            int b = clamp(signed9(luma + (int)floor_shift(cb * 454 + 128, 8)),
                          -128, 127) +
                    128;
            int p = y * 16 + x;
            if (depth == 2) {
                macroblock[p * 3] = r ^ (signed_output ? 0x80 : 0);
                macroblock[p * 3 + 1] = g ^ (signed_output ? 0x80 : 0);
                macroblock[p * 3 + 2] = b ^ (signed_output ? 0x80 : 0);
            } else {
                unsigned packed =
                    clamp((r + 4) / 8, 0, 31) |
                    (clamp((g + 4) / 8, 0, 31) << 5) |
                    (clamp((b + 4) / 8, 0, 31) << 10) |
                    ((command & (1u << 25)) ? 0x8000 : 0);
                packed ^= signed_output ? 0x4210 : 0;
                macroblock[p * 2] = packed;
                macroblock[p * 2 + 1] = packed >> 8;
            }
        }
    }
    pixel_pos = 0;
    pixel_size = depth == 2 ? 768 : 512;
    return 0;
}
void Psyz_MdecReset(void) {
    input = NULL;
    input_left = pixel_pos = pixel_size = 0;
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
        input_left = pixel_pos = pixel_size = 0;
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
            unsigned v = bytes[i * 2] | ((unsigned)bytes[i * 2 + 1] << 8);
            tables.scale[i] = (int)(v & 32767) - ((v & 32768) ? 32768 : 0);
        }
        return 0;
    }
    input_left = words * 2;
    for (size_t i = 0; i < input_left; ++i)
        input_transfer[i] = bytes[i * 2] | ((unsigned)bytes[i * 2 + 1] << 8);
    input = input_transfer;
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
