#include "ztest.h"
#include <string.h>

#ifdef __psx__
static const uint32_t mdec_nonflat[] = {
    0x3800000F, 0x000413E8, 0x080207FD, 0x1020FE00, 0x040403FB, 0xFE000803,
    0x00091010, 0x080407FE, 0x13F0FE00, 0x040503F9, 0xFE000805, 0x00031020,
    0x080607FA, 0x13E0FE00, 0x040703FE, 0xFE000807,
};

static const uint8_t mdec_quant[64] = {
    2,  16, 16, 19, 16, 19, 22, 22, 22, 22, 22, 22, 26, 24, 26, 27,
    27, 27, 26, 26, 26, 26, 27, 27, 27, 29, 29, 29, 34, 34, 34, 29,
    29, 29, 27, 27, 29, 29, 32, 32, 34, 34, 37, 38, 37, 35, 35, 34,
    35, 38, 38, 40, 40, 40, 48, 48, 46, 46, 56, 56, 58, 69, 69, 83};

static const uint16_t mdec_scale[64] = {
    0x5a82, 0x5a82, 0x5a82, 0x5a82, 0x5a82, 0x5a82, 0x5a82, 0x5a82,
    0x7d8a, 0x6a6d, 0x471c, 0x18f8, 0xe707, 0xb8e3, 0x9592, 0x8275,
    0x7641, 0x30fb, 0xcf04, 0x89be, 0x89be, 0xcf04, 0x30fb, 0x7641,
    0x6a6d, 0xe707, 0x8275, 0xb8e3, 0x471c, 0x7d8a, 0x18f8, 0x9592,
    0x5a82, 0xa57d, 0xa57d, 0x5a82, 0x5a82, 0xa57d, 0xa57d, 0x5a82,
    0x471c, 0x8275, 0x18f8, 0x6a6d, 0x9592, 0xe707, 0x7d8a, 0xb8e3,
    0x30fb, 0x89be, 0x7641, 0xcf04, 0xcf04, 0x7641, 0x89be, 0x30fb,
    0x18f8, 0xb8e3, 0x6a6d, 0x8275, 0x7d8a, 0x9592, 0x471c, 0xe707};

static uint32_t psx_mdec_read(unsigned offset) {
    return *(volatile uint32_t*)(0xBF801820u + offset);
}

static void psx_mdec_write(unsigned offset, uint32_t value) {
    *(volatile uint32_t*)(0xBF801820u + offset) = value;
}

static int hardware_transfer(
    uint32_t command, const uint32_t* data, unsigned count, uint32_t* output,
    unsigned output_words) {
    unsigned sent = 0, received = 0;
    for (unsigned poll = 0; poll < 0x100000; ++poll) {
        uint32_t status = psx_mdec_read(4);
        if (sent <= count && !(status & (sent ? 0x40000000u : 0x20000000u))) {
            psx_mdec_write(0, sent ? data[sent - 1] : command);
            ++sent;
        }
        if (received < output_words && !(status & 0x80000000u))
            output[received++] = psx_mdec_read(0);
        if (sent == count + 1 && received == output_words &&
            !(psx_mdec_read(4) & 0x20000000u))
            return 0;
    }
    zprintf("MDEC timeout: command=%08X status=%08X input=%u/%u output=%u/%u\n",
            command, psx_mdec_read(4), sent, count + 1, received, output_words);
    psx_mdec_write(4, 0x80000000u);
    return -1;
}

static void print_words(
    const char* name, const uint32_t* data, unsigned count) {
    zprintf("MDEC %s words=%u\n", name, count);
    for (unsigned i = 0; i < count; i += 8) {
        zprintf("%04X:", i);
        for (unsigned j = i; j < count && j < i + 8; ++j)
            zprintf(" %08X", data[j]);
        zprintf("\n");
    }
}

static void capture_hardware(
    uint32_t flags, const uint32_t* data, unsigned count,
    const uint8_t quant[128], const uint16_t scale[64]) {
    uint32_t padded[192];
    unsigned padded_count = (count + 31u) & ~31u;
    memcpy(padded, data, count * sizeof(*padded));
    for (unsigned i = count; i < padded_count; ++i)
        padded[i] = 0xFE00FE00u;
    data = padded;
    count = padded_count;
    uint32_t quant_words[32], scale_words[32];
    memcpy(quant_words, quant, sizeof(quant_words));
    memcpy(scale_words, scale, sizeof(scale_words));
    unsigned depth = (flags >> 27) & 3;
    const unsigned sizes[] = {8, 16, 192, 128};
    unsigned words = sizes[depth];
    uint32_t output[193], repeated[193];
    memset(output, 0xA5, sizeof(output));
    memset(repeated, 0xA5, sizeof(repeated));
    uint32_t command = 0x20000000 | flags | count;
    for (unsigned pass = 0; pass < 2; ++pass) {
        psx_mdec_write(4, 0x80000000u);
        zassert_s32_eq(
            0, hardware_transfer(0x40000001, quant_words, 32, NULL, 0));
        zassert_s32_eq(
            0, hardware_transfer(0x60000000, scale_words, 32, NULL, 0));
        zassert_s32_eq(0, hardware_transfer(command, data, count,
                                            pass ? repeated : output, words));
        psx_mdec_write(4, 0x80000000u);
        if (!pass) {
            zprintf("MDEC command=%08X output_order=pio\n", command);
            print_words("quant", quant_words, 32);
            print_words("scale", scale_words, 32);
            print_words("input", data, count);
            print_words("output", output, words);
        }
    }
    zexpect_u32_eq(0xA5A5A5A5, output[words]);
    zexpect_u32_eq(0xA5A5A5A5, repeated[words]);
    for (unsigned i = 0; i < words; ++i) {
        if (output[i] != repeated[i])
            zprintf("MDEC command=%08X word=%u first=%08X repeated=%08X\n",
                    command, i, output[i], repeated[i]);
        zassert_u32_eq(output[i], repeated[i]);
    }
}

static void hardware_tables(uint8_t quant[128], uint16_t scale[64]) {
    memcpy(quant, mdec_quant, 64);
    memcpy(quant + 64, mdec_quant, 64);
    memcpy(scale, mdec_scale, 128);
}

static unsigned generated_runlevels(
    uint32_t data[192], unsigned blocks, unsigned scale, int dense) {
    uint16_t coefficients[384];
    unsigned count = 0;
    for (unsigned block = 0; block < blocks; ++block) {
        unsigned first = (scale << 10) | (block & 1 ? 511 : 512);
        if (first == 0xFE00)
            ++first;
        coefficients[count++] = first;
        unsigned index = 0;
        for (unsigned i = 0; i < (dense ? 63 : 8); ++i) {
            unsigned run = dense ? 0 : (i + block) % 7;
            index += run + 1;
            if (index >= 64)
                break;
            unsigned level = (i * 173 + block * 97 + 513) & 1023;
            coefficients[count++] = (run << 10) | level;
        }
        if (!dense)
            coefficients[count++] = 0xFE00;
    }
    if (count & 1)
        coefficients[count++] = 0xFE00;
    for (unsigned i = 0; i < count / 2; ++i)
        data[i] =
            coefficients[i * 2] | ((uint32_t)coefficients[i * 2 + 1] << 16);
    return count / 2;
}

ZTEST(mdec, hardware_rgb24) {
    uint8_t quant[128];
    uint16_t scale[64];
    hardware_tables(quant, scale);
    capture_hardware(2u << 27, mdec_nonflat + 1, 15, quant, scale);
    capture_hardware(
        (2u << 27) | (1u << 26), mdec_nonflat + 1, 15, quant, scale);
}

ZTEST(mdec, hardware_rgb555_flags) {
    uint8_t quant[128];
    uint16_t scale[64];
    hardware_tables(quant, scale);
    for (unsigned flags = 0; flags < 4; ++flags)
        capture_hardware(
            (3u << 27) | (flags << 25), mdec_nonflat + 1, 15, quant, scale);
}

ZTEST(mdec, hardware_monochrome) {
    const uint32_t data[] = {0x00091010, 0x080407FE, 0xFE0013F0};
    uint8_t quant[128];
    uint16_t scale[64];
    hardware_tables(quant, scale);
    for (unsigned depth = 0; depth < 2; ++depth)
        for (unsigned sign = 0; sign < 2; ++sign)
            capture_hardware(
                (depth << 27) | (sign << 26), data, 3, quant, scale);
}

ZTEST(mdec, hardware_custom_quantization) {
    uint8_t quant[128];
    uint16_t scale[64];
    hardware_tables(quant, scale);
    for (unsigned i = 0; i < 128; ++i)
        quant[i] = (i * 17 + 3) & 255;
    capture_hardware(2u << 27, mdec_nonflat + 1, 15, quant, scale);
    capture_hardware(3u << 27, mdec_nonflat + 1, 15, quant, scale);
}

ZTEST(mdec, hardware_custom_scale) {
    uint8_t quant[128];
    uint16_t scale[64];
    hardware_tables(quant, scale);
    for (unsigned i = 0; i < 64; ++i)
        scale[i] ^= (i * 13 + 9) & 255;
    capture_hardware(2u << 27, mdec_nonflat + 1, 15, quant, scale);
    capture_hardware(3u << 27, mdec_nonflat + 1, 15, quant, scale);
}

ZTEST(mdec, hardware_generated_runlevels) {
    const unsigned scales[] = {1, 4, 63};
    uint8_t quant[128];
    uint16_t matrix[64];
    uint32_t data[192];
    hardware_tables(quant, matrix);
    for (unsigned s = 0; s < 3; ++s) {
        for (unsigned depth = 0; depth < 4; ++depth) {
            unsigned count =
                generated_runlevels(data, depth < 2 ? 1 : 6, scales[s], 0);
            for (unsigned sign = 0; sign < 2; ++sign)
                capture_hardware(
                    (depth << 27) | (sign << 26), data, count, quant, matrix);
        }
    }
}

ZTEST(mdec, hardware_zero_scale) {
    uint8_t quant[128];
    uint16_t matrix[64];
    uint32_t data[192];
    hardware_tables(quant, matrix);
    for (unsigned depth = 0; depth < 4; ++depth) {
        unsigned count = generated_runlevels(data, depth < 2 ? 1 : 6, 0, 0);
        for (unsigned sign = 0; sign < 2; ++sign)
            capture_hardware(
                (depth << 27) | (sign << 26), data, count, quant, matrix);
    }
}

ZTEST(mdec, hardware_full_blocks_without_end_codes) {
    uint8_t quant[128];
    uint16_t matrix[64];
    uint32_t data[192];
    hardware_tables(quant, matrix);
    for (unsigned depth = 0; depth < 4; ++depth) {
        unsigned count = generated_runlevels(data, depth < 2 ? 1 : 6, 4, 1);
        capture_hardware(depth << 27, data, count, quant, matrix);
    }
}
#endif
