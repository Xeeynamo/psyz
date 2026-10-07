#include "ztest.h"
#include <string.h>
#ifndef __psx__
#include "../src/psyz/mdec.h"
#include <stdlib.h>
#endif

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

#ifdef __psx__
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
#else
static const uint32_t mdec_rgb24_expected[] = {
    0x87A99191, 0x83769F87, 0x8B82758C, 0x728C8974, 0x7D678A87, 0x7E735D88,
    0x6CA48374, 0x74679C7B, 0xA77B6EA0, 0x7DB08E76, 0x9673B795, 0xA18D6AAA,
    0x89AA9292, 0x8578A189, 0x8D84778E, 0x748D8A75, 0x7F698C89, 0x80755F8A,
    0x67A07F70, 0x6F629776, 0xA3776A9B, 0x79AB8971, 0x916EB391, 0x9D8966A5,
    0x8FA69599, 0x877F9C8B, 0x88857D8A, 0x7B898C7C, 0x806F888B, 0x7D766587,
    0x6592776D, 0x68608A6F, 0x9570688D, 0x779E836F, 0x8B6BA68B, 0x9182629A,
    0x91A8979B, 0x88809E8D, 0x8A877F8B, 0x7D8B8E7E, 0x82718A8D, 0x7F786789,
    0x618E7369, 0x645C866B, 0x906B6389, 0x729A7F6B, 0x8666A186, 0x8D7E5E95,
    0x979F95A1, 0x8786958B, 0x81868582, 0x81828E82, 0x8177818D, 0x76776D80,
    0x6785726F, 0x63627D6A, 0x886A6981, 0x78917E71, 0x856C9885, 0x827D648A,
    0x959D939F, 0x86859389, 0x7F848381, 0x7F808C80, 0x7F757F8B, 0x74756B7E,
    0x6B897673, 0x6766816E, 0x8D6F6E85, 0x7D958275, 0x8A719D8A, 0x8681688F,
    0x9695909F, 0x81868C87, 0x78808579, 0x80768881, 0x7C777587, 0x6B726D75,
    0x75897C7E, 0x6D6F8073, 0x8C757784, 0x8796867F, 0x8F7B9E8E, 0x88877390,
    0x94948F9E, 0x7F848A85, 0x767E8377, 0x7E758780, 0x7A757385, 0x69706B73,
    0x7A8D8082, 0x72748578, 0x90797B89, 0x8B9B8B84, 0x9480A292, 0x8C8B7795,
    0x8A8C8796, 0x757A807B, 0x70787D6D, 0x85768881, 0x807B7A8C, 0x6C736E79,
    0x7E96898B, 0x7173897C, 0x8F787A88, 0x8B9B8B84, 0x8E7AA292, 0x81806C8F,
    0x8F928D9C, 0x7A7F8580, 0x757D8272, 0x8A7C8E87, 0x85807F91, 0x7178737E,
    0x78908385, 0x6B6D8376, 0x88717382, 0x8495857E, 0x87739B8B, 0x7B7A6688,
    0x949E94A0, 0x84839288, 0x8287867F, 0x8E8A968A, 0x8E848E9A, 0x8081778D,
    0x6B8E7B78, 0x6261816E, 0x87696880, 0x77917E71, 0x80679784, 0x77725985,
    0x99A49AA6, 0x8988978D, 0x878C8B84, 0x93909C90, 0x9389939F, 0x85867C92,
    0x64887572, 0x5C5B7A67, 0x8062617A, 0x718A776A, 0x7960917E, 0x716C537E,
    0x93AD9CA0, 0x8A82A08F, 0x908D858D, 0x8F999C8C, 0x94839C9F, 0x8E87769B,
    0x5E91766C, 0x5D558368, 0x88635B82, 0x6B937864, 0x7A5A9A7F, 0x7C6D4D89,
    0x8EA7969A, 0x857D9B8A, 0x8B888088, 0x8A939686, 0x8F7E979A, 0x89827196,
    0x65977C72, 0x635B8A6F, 0x8F6A6288, 0x719A7F6B, 0x8161A085, 0x82735390,
    0x82A78F8F, 0x7E719A82, 0x8A817487, 0x7E93907B, 0x88729693, 0x867B6593,
    0x6AA78677, 0x6D609A79, 0x9F736699, 0x76AA8870, 0x8966B08E, 0x907C599D,
    0x7DA18989, 0x796C957D, 0x857C6F82, 0x798D8A75, 0x836D918E, 0x8176608E,
    0x70AD8C7D, 0x7366A07F, 0xA67A6D9F, 0x7DB08E76, 0x906DB795, 0x96825FA4,
};

static const uint32_t mdec_rgb555_expected[] = {
    0x52315652, 0x460F4A0F, 0x462E4A2F, 0x41CC460D, 0x51EE560F, 0x55EE51ED,
    0x5E705A4F, 0x524D566E, 0x52315652, 0x4A2F4A2F, 0x4A2F4A2F, 0x41EC460D,
    0x4DED520E, 0x51ED4DCC, 0x5A4F562E, 0x522D564E, 0x52325673, 0x46304630,
    0x462F4650, 0x41ED460E, 0x45CD49EE, 0x4DCD49AC, 0x562F520E, 0x4A0C4E2D,
    0x52525673, 0x46304630, 0x46504650, 0x41ED460E, 0x45AC49CD, 0x49AC45AC,
    0x522E4E0D, 0x4A0C4E2D, 0x4E335274, 0x42314231, 0x42504250, 0x3DEE420F,
    0x41AD45CE, 0x45AD418C, 0x4E2F4A0E, 0x420D462E, 0x4A335254, 0x42304231,
    0x42304250, 0x3DED420F, 0x41CD45EE, 0x49CE45AD, 0x52304E0F, 0x460D4A2E,
    0x4A334E54, 0x3E113E11, 0x3E303E30, 0x35CE3E0F, 0x41CF4610, 0x49EF45CE,
    0x52514E30, 0x462E4A4F, 0x46334E54, 0x3E103E11, 0x3A303E30, 0x35CD39EF,
    0x45EF4A10, 0x49EF45CF, 0x52514E31, 0x4A2F4E70, 0x41F14A33, 0x39F039EF,
    0x3E513E30, 0x39CE3E0F, 0x46104E31, 0x49EF45CE, 0x52514E31, 0x420E4A4F,
    0x46124A54, 0x3E1039F0, 0x42514251, 0x39EE4230, 0x41EF4A11, 0x45CE41AE,
    0x4E314E30, 0x3DED462E, 0x4A335274, 0x42314230, 0x4A724671, 0x420F4A51,
    0x41CD49EF, 0x45AD418C, 0x4E2F4A0E, 0x3DCB460D, 0x4E535675, 0x46514631,
    0x4A924A92, 0x46304A51, 0x3DAD45EE, 0x418C3D8B, 0x4A0E45ED, 0x39CA41EC,
    0x52525A94, 0x4A514A30, 0x52924E92, 0x4A2F4E70, 0x41AC49EE, 0x458B418B,
    0x4E0D49ED, 0x41CA45EB, 0x4E325673, 0x46304630, 0x4E714A71, 0x460E4E50,
    0x45CD4E0E, 0x49AC458B, 0x522E4E0D, 0x41CA4A0C, 0x4E105652, 0x460F460E,
    0x4E504A4F, 0x45ED4A2E, 0x4DED562F, 0x51CD4DCC, 0x5A4F562E, 0x4A0B522D,
    0x4E105231, 0x460E41EE, 0x4A4F4A2F, 0x41EC4A0E, 0x520E5A50, 0x55EE51CD,
    0x5E705A4F, 0x4E0C564E,
};

static const uint32_t mdec_custom_quant_expected[] = {
    0x819F8C87, 0x847A9986, 0x8D83798E, 0x768E8776, 0x80708E87, 0x887B6B8D,
    0x6F988074, 0x786D937B, 0x987C7194, 0x799F8775, 0x8B74A38B, 0x96866F9B,
    0x82A08D88, 0x857B9A87, 0x8E847A8F, 0x768F8877, 0x81718E87, 0x897C6C8E,
    0x6D967E72, 0x756A9179, 0x967A6F91, 0x779D8573, 0x8871A189, 0x94846D98,
    0x879D8E8C, 0x867D9889, 0x8D867D8D, 0x7B8D8A7C, 0x83768C89, 0x867D708C,
    0x6D8E7971, 0x72688A75, 0x91766C8D, 0x76958071, 0x856F9A85, 0x90816B94,
    0x879E8F8D, 0x877E9889, 0x8E877E8E, 0x7C8D8A7C, 0x83768D8A, 0x877E718C,
    0x6A8C776F, 0x70668772, 0x8F746A8B, 0x73937E6F, 0x836D9782, 0x8D7E6892,
    0x8A998E90, 0x86819388, 0x8A86818A, 0x7F88897F, 0x82798889, 0x827D7487,
    0x6C877771, 0x6F698272, 0x89736D85, 0x76907D72, 0x82709481, 0x877D6B8C,
    0x8A988D8F, 0x85809388, 0x89858089, 0x7E88897F, 0x82798788, 0x817C7387,
    0x6F897973, 0x716B8575, 0x8B756F87, 0x79927F74, 0x84729784, 0x8A806E8E,
    0x89938C8F, 0x83828D86, 0x84828185, 0x7D82877E, 0x80788186, 0x7D7B7382,
    0x74897D79, 0x74718478, 0x8C797687, 0x7E92837A, 0x87789687, 0x8A83748E,
    0x88928B8E, 0x82818C85, 0x83818084, 0x7D81867D, 0x7F778186, 0x7C7A7281,
    0x768B7F7B, 0x7774867A, 0x8E7B788A, 0x8094857C, 0x8A7B9889, 0x8C857691,
    0x858F888B, 0x7E7D8982, 0x82807F80, 0x82848980, 0x847C868B, 0x807E7686,
    0x778E827E, 0x7572877B, 0x8B787588, 0x7E92837A, 0x84759687, 0x847D6E8B,
    0x88928B8E, 0x81808C85, 0x85838283, 0x85878C83, 0x877F898E, 0x83817989,
    0x748B7F7B, 0x716E8478, 0x88757284, 0x7A8F8077, 0x81729283, 0x817A6B88,
    0x8B9A8F91, 0x86819489, 0x8C88838A, 0x888F9086, 0x8B829192, 0x89847B90,
    0x6D8A7A74, 0x6D678373, 0x86706A83, 0x748E7B70, 0x7C6A927F, 0x7F756386,
    0x8D9D9294, 0x8984968B, 0x8F8B868D, 0x8B929389, 0x8E859495, 0x8C877E93,
    0x6A877771, 0x69638070, 0x836D677F, 0x708B786D, 0x79678E7B, 0x7C726083,
    0x8AA29391, 0x8A819B8C, 0x938C8391, 0x88979486, 0x8F829996, 0x91887B98,
    0x688C776F, 0x6A608570, 0x896E6485, 0x6D8E796A, 0x7A64917C, 0x82735D89,
    0x889F908E, 0x877E998A, 0x9089808E, 0x85949183, 0x8C7F9693, 0x8E857895,
    0x6B8F7A72, 0x6E648873, 0x8C716789, 0x71917C6D, 0x7D679580, 0x8576608C,
    0x819F8C87, 0x83799986, 0x8F857B8D, 0x7E948D7C, 0x8878968F, 0x8F827295,
    0x6D988074, 0x72679179, 0x92766B8E, 0x739A8270, 0x826B9D85, 0x8B7B6492,
    0x7E9C8984, 0x80769683, 0x8C82788A, 0x7B918A79, 0x8575938C, 0x8C7F6F92,
    0x709B8377, 0x766B947C, 0x95796E92, 0x779D8573, 0x856EA189, 0x8E7E6795,
};

static const uint32_t mdec_depth0_signed0_expected[] = {
    0x99998777, 0x6788889A, 0x56789ABC, 0x678899AB,
    0x99998888, 0xAAA98777, 0x88988889, 0x567889BC,
};

static const uint32_t mdec_depth0_signed1_expected[] = {
    0x11110FFF, 0xEF000012, 0xDEF01234, 0xEF001123,
    0x11110000, 0x22210FFF, 0x00100001, 0xDEF00134,
};

static const uint32_t mdec_depth1_signed0_expected[] = {
    0x786C6C70, 0x9296968A, 0x8287939E, 0x67727E83, 0x8C9DB2C2, 0x48586E7F,
    0x8A94A4B2, 0x5C6A7A84, 0x80797C82, 0x8C92958E, 0x7B6D6A6E, 0x9DA09E8F,
    0x7F7D858D, 0x78818886, 0x8695A8B8, 0x4A5A6D7C,
};

static const uint32_t mdec_depth1_signed1_expected[] = {
    0xF8ECECF0, 0x1216160A, 0x0207131E, 0xE7F2FE03, 0x0C1D3242, 0xC8D8EEFF,
    0x0A142432, 0xDCEAFA04, 0x00F9FC02, 0x0C12150E, 0xFBEDEAEE, 0x1D201E0F,
    0xFFFD050D, 0xF8010806, 0x06152838, 0xCADAEDFC,
};

static const uint32_t mdec_depth2_signed1_expected[] = {
    0x07291111, 0x03F61F07, 0x0B02F50C, 0xF20C09F4, 0xFDE70A07, 0xFEF3DD08,
    0xEC2403F4, 0xF4E71CFB, 0x27FBEE20, 0xFD300EF6, 0x16F33715, 0x210DEA2A,
    0x092A1212, 0x05F82109, 0x0D04F70E, 0xF40D0AF5, 0xFFE90C09, 0x00F5DF0A,
    0xE720FFF0, 0xEFE217F6, 0x23F7EA1B, 0xF92B09F1, 0x11EE3311, 0x1D09E625,
    0x0F261519, 0x07FF1C0B, 0x0805FD0A, 0xFB090CFC, 0x00EF080B, 0xFDF6E507,
    0xE512F7ED, 0xE8E00AEF, 0x15F0E80D, 0xF71E03EF, 0x0BEB260B, 0x1102E21A,
    0x1128171B, 0x08001E0D, 0x0A07FF0B, 0xFD0B0EFE, 0x02F10A0D, 0xFFF8E709,
    0xE10EF3E9, 0xE4DC06EB, 0x10EBE309, 0xF21AFFEB, 0x06E62106, 0x0DFEDE15,
    0x171F1521, 0x0706150B, 0x01060502, 0x01020E02, 0x01F7010D, 0xF6F7ED00,
    0xE705F2EF, 0xE3E2FDEA, 0x08EAE901, 0xF811FEF1, 0x05EC1805, 0x02FDE40A,
    0x151D131F, 0x06051309, 0xFF040301, 0xFF000C00, 0xFFF5FF0B, 0xF4F5EBFE,
    0xEB09F6F3, 0xE7E601EE, 0x0DEFEE05, 0xFD1502F5, 0x0AF11D0A, 0x0601E80F,
    0x1615101F, 0x01060C07, 0xF80005F9, 0x00F60801, 0xFCF7F507, 0xEBF2EDF5,
    0xF509FCFE, 0xEDEF00F3, 0x0CF5F704, 0x071606FF, 0x0FFB1E0E, 0x0807F310,
    0x14140F1E, 0xFF040A05, 0xF6FE03F7, 0xFEF50700, 0xFAF5F305, 0xE9F0EBF3,
    0xFA0D0002, 0xF2F405F8, 0x10F9FB09, 0x0B1B0B04, 0x14002212, 0x0C0BF715,
    0x0A0C0716, 0xF5FA00FB, 0xF0F8FDED, 0x05F60801, 0x00FBFA0C, 0xECF3EEF9,
    0xFE16090B, 0xF1F309FC, 0x0FF8FA08, 0x0B1B0B04, 0x0EFA2212, 0x0100EC0F,
    0x0F120D1C, 0xFAFF0500, 0xF5FD02F2, 0x0AFC0E07, 0x0500FF11, 0xF1F8F3FE,
    0xF8100305, 0xEBED03F6, 0x08F1F302, 0x041505FE, 0x07F31B0B, 0xFBFAE608,
    0x141E1420, 0x04031208, 0x020706FF, 0x0E0A160A, 0x0E040E1A, 0x0001F70D,
    0xEB0EFBF8, 0xE2E101EE, 0x07E9E800, 0xF711FEF1, 0x00E71704, 0xF7F2D905,
    0x19241A26, 0x0908170D, 0x070C0B04, 0x13101C10, 0x1309131F, 0x0506FC12,
    0xE408F5F2, 0xDCDBFAE7, 0x00E2E1FA, 0xF10AF7EA, 0xF9E011FE, 0xF1ECD3FE,
    0x132D1C20, 0x0A02200F, 0x100D050D, 0x0F191C0C, 0x14031C1F, 0x0E07F61B,
    0xDE11F6EC, 0xDDD503E8, 0x08E3DB02, 0xEB13F8E4, 0xFADA1AFF, 0xFCEDCD09,
    0x0E27161A, 0x05FD1B0A, 0x0B080008, 0x0A131606, 0x0FFE171A, 0x0902F116,
    0xE517FCF2, 0xE3DB0AEF, 0x0FEAE208, 0xF11AFFEB, 0x01E12005, 0x02F3D310,
    0x02270F0F, 0xFEF11A02, 0x0A01F407, 0xFE1310FB, 0x08F21613, 0x06FBE513,
    0xEA2706F7, 0xEDE01AF9, 0x1FF3E619, 0xF62A08F0, 0x09E6300E, 0x10FCD91D,
    0xFD210909, 0xF9EC15FD, 0x05FCEF02, 0xF90D0AF5, 0x03ED110E, 0x01F6E00E,
    0xF02D0CFD, 0xF3E620FF, 0x26FAED1F, 0xFD300EF6, 0x10ED3715, 0x1602DF24,
};

static const uint32_t mdec_depth3_signed1_expected[] = {
    0x10211442, 0x041F081F, 0x043E083F, 0x03DC041D, 0x13FE141F, 0x17FE13FD,
    0x1C60185F, 0x105D147E, 0x10211442, 0x083F083F, 0x083F083F, 0x03FC041D,
    0x0FFD101E, 0x13FD0FDC, 0x185F143E, 0x103D145E, 0x10221463, 0x04200420,
    0x043F0440, 0x03FD041E, 0x07DD0BFE, 0x0FDD0BBC, 0x143F101E, 0x081C0C3D,
    0x10421463, 0x04200420, 0x04400440, 0x03FD041E, 0x07BC0BDD, 0x0BBC07BC,
    0x103E0C1D, 0x081C0C3D, 0x0C231064, 0x00210021, 0x00400040, 0x7FFE001F,
    0x03BD07DE, 0x07BD039C, 0x0C3F081E, 0x001D043E, 0x08231044, 0x00200021,
    0x00200040, 0x7FFD001F, 0x03DD07FE, 0x0BDE07BD, 0x10200C1F, 0x041D083E,
    0x08230C44, 0x7C017C01, 0x7C207C20, 0x77DE7C1F, 0x03DF0400, 0x0BFF07DE,
    0x10410C20, 0x043E085F, 0x04230C44, 0x7C007C01, 0x78207C20, 0x77DD7BFF,
    0x07FF0800, 0x0BFF07DF, 0x10410C21, 0x083F0C60, 0x03E10823, 0x7BE07BFF,
    0x7C417C20, 0x7BDE7C1F, 0x04000C21, 0x0BFF07DE, 0x10410C21, 0x001E085F,
    0x04020844, 0x7C007BE0, 0x00410041, 0x7BFE0020, 0x03FF0801, 0x07DE03BE,
    0x0C210C20, 0x7FFD043E, 0x08231064, 0x00210020, 0x08620461, 0x001F0841,
    0x03DD0BFF, 0x07BD039C, 0x0C3F081E, 0x7FDB041D, 0x0C431465, 0x04410421,
    0x08820882, 0x04200841, 0x7FBD07FE, 0x039C7F9B, 0x081E07FD, 0x7BDA03FC,
    0x10421884, 0x08410820, 0x10820C82, 0x083F0C60, 0x03BC0BFE, 0x079B039B,
    0x0C1D0BFD, 0x03DA07FB, 0x0C221463, 0x04200420, 0x0C610861, 0x041E0C40,
    0x07DD0C1E, 0x0BBC079B, 0x103E0C1D, 0x03DA081C, 0x0C001442, 0x041F041E,
    0x0C40085F, 0x07FD083E, 0x0FFD143F, 0x13DD0FDC, 0x185F143E, 0x081B103D,
    0x0C001021, 0x041E03FE, 0x085F083F, 0x03FC081E, 0x101E1840, 0x17FE13DD,
    0x1C60185F, 0x0C1C145E,
};

static void load_quant(int custom) {
    uint8_t quant[128];
    for (unsigned i = 0; i < 64; ++i) {
        quant[i] = quant[i + 64] =
            custom ? mdec_quant[i] / 2 + 1 : mdec_quant[i];
    }
    zassert_s32_eq(0, Psyz_MdecCommand(0x40000001, quant, 32));
}

static void load_scale(void) {
    uint8_t scale[128];
    for (unsigned i = 0; i < 64; ++i) {
        scale[i * 2] = mdec_scale[i];
        scale[i * 2 + 1] = mdec_scale[i] >> 8;
    }
    zassert_s32_eq(0, Psyz_MdecCommand(0x60000000, scale, 32));
}

static void make_flat(uint32_t* data, unsigned macroblocks) {
    for (unsigned i = 0; i < macroblocks * 6; ++i)
        data[i] = 0xFE000400;
}

static void check_reference(uint32_t flags, const uint32_t* data, size_t count,
                            const uint32_t* expected, size_t words) {
    uint32_t pixels[193];
    memset(pixels, 0xA5, sizeof(pixels));
    zassert_s32_eq(
        0, Psyz_MdecCommand(0x20000000 | flags | count, data, count));
    zassert_s32_eq(0, Psyz_MdecRead(pixels, words));
    zexpect_u8array_eq(expected, pixels, words * 4);
    zexpect_u32_eq(0xA5A5A5A5, pixels[words]);
}

ZTEST_SETUP(mdec) {
    Psyz_MdecReset();
    load_quant(0);
    load_scale();
}

ZTEST(mdec, rgb24_reference) {
    check_reference(2u << 27, mdec_nonflat + 1, 15, mdec_rgb24_expected, 192);
}

ZTEST(mdec, rgb555_reference) {
    check_reference(3u << 27, mdec_nonflat + 1, 15, mdec_rgb555_expected, 128);
}

ZTEST(mdec, signed_color_reference) {
    check_reference((2u << 27) | (1u << 26), mdec_nonflat + 1, 15,
                    mdec_depth2_signed1_expected, 192);
    check_reference((3u << 27) | (1u << 26), mdec_nonflat + 1, 15,
                    mdec_depth3_signed1_expected, 128);
}

ZTEST(mdec, monochrome_reference) {
    const uint32_t data[] = {0x00091010, 0x080407FE, 0xFE0013F0};
    check_reference(0, data, 3, mdec_depth0_signed0_expected, 8);
    check_reference(1u << 27, data, 3, mdec_depth1_signed0_expected, 16);
    check_reference(1u << 26, data, 3, mdec_depth0_signed1_expected, 8);
    check_reference(
        (1u << 27) | (1u << 26), data, 3, mdec_depth1_signed1_expected, 16);
}

ZTEST(mdec, custom_quantization_reference) {
    load_quant(1);
    check_reference(
        2u << 27, mdec_nonflat + 1, 15, mdec_custom_quant_expected, 192);
}

ZTEST(mdec, luma_only_table_command_preserves_chroma) {
    uint8_t quant[128];
    uint32_t expected[192], actual[192];
    for (unsigned i = 0; i < 64; ++i) {
        quant[i] = mdec_quant[i] / 2 + 1;
        quant[i + 64] = mdec_quant[i];
    }
    zassert_s32_eq(0, Psyz_MdecCommand(0x40000001, quant, 32));
    zassert_s32_eq(0, Psyz_MdecCommand(0x3000000F, mdec_nonflat + 1, 15));
    zassert_s32_eq(0, Psyz_MdecRead(expected, 192));
    load_quant(0);
    zassert_s32_eq(0, Psyz_MdecCommand(0x40000000, quant, 16));
    zassert_s32_eq(0, Psyz_MdecCommand(0x3000000F, mdec_nonflat + 1, 15));
    zassert_s32_eq(0, Psyz_MdecRead(actual, 192));
    zexpect_u8array_eq(expected, actual, sizeof(actual));
}

ZTEST(mdec, scale_table_is_loaded_by_command) {
    uint8_t scale[128] = {0};
    uint32_t pixels[192];
    zassert_s32_eq(0, Psyz_MdecCommand(0x60000000, scale, 32));
    zassert_s32_eq(0, Psyz_MdecCommand(0x3000000F, mdec_nonflat + 1, 15));
    zassert_s32_eq(0, Psyz_MdecRead(pixels, 192));
    for (unsigned i = 0; i < 192; ++i)
        zassert_u32_eq(0x80808080, pixels[i]);
}

ZTEST(mdec, all_flat_output_formats) {
    uint32_t data[6], pixels[193];
    const unsigned counts[] = {8, 16, 192, 128};
    const uint32_t values[] = {0x88888888, 0x80808080, 0x80808080, 0x42104210};
    make_flat(data, 1);
    for (unsigned depth = 0; depth < 4; ++depth) {
        memset(pixels, 0xA5, sizeof(pixels));
        zassert_s32_eq(
            0, Psyz_MdecCommand(0x20000006 | (depth << 27), data, 6));
        zassert_s32_eq(0, Psyz_MdecRead(pixels, counts[depth]));
        for (unsigned i = 0; i < counts[depth]; ++i)
            zassert_u32_eq(values[depth], pixels[i]);
        zexpect_u32_eq(0xA5A5A5A5, pixels[counts[depth]]);
    }
}

ZTEST(mdec, rgb555_bit15) {
    uint32_t data[6], pixels[128];
    make_flat(data, 1);
    zassert_s32_eq(0, Psyz_MdecCommand(0x3A000006, data, 6));
    zassert_s32_eq(0, Psyz_MdecRead(pixels, 128));
    for (unsigned i = 0; i < 128; ++i)
        zassert_u32_eq(0xC210C210, pixels[i]);
}

ZTEST(mdec, input_is_copied_and_not_modified) {
    uint32_t data[6], pixels[192];
    make_flat(data, 1);
    zassert_s32_eq(0, Psyz_MdecCommand(0x30000006, data, 6));
    for (unsigned i = 0; i < 6; ++i)
        zassert_u32_eq(0xFE000400, data[i]);
    memset(data, 0, sizeof(data));
    zassert_s32_eq(0, Psyz_MdecRead(pixels, 192));
    for (unsigned i = 0; i < 192; ++i)
        zassert_u32_eq(0x80808080, pixels[i]);
}

ZTEST(mdec, sliced_output_crosses_macroblock_boundaries) {
    uint32_t data[12], expected[384], actual[385];
    make_flat(data, 2);
    data[8] = 0xFE000420;
    data[9] = 0xFE0007E0;
    zassert_s32_eq(0, Psyz_MdecCommand(0x3000000C, data, 12));
    zassert_s32_eq(0, Psyz_MdecRead(expected, 384));
    zassert_s32_eq(0, Psyz_MdecCommand(0x3000000C, data, 12));
    memset(actual, 0xA5, sizeof(actual));
    zassert_s32_eq(0, Psyz_MdecRead(actual, 65));
    zassert_s32_eq(0, Psyz_MdecRead(actual + 65, 192));
    zassert_s32_eq(0, Psyz_MdecRead(actual + 257, 127));
    zexpect_u8array_eq(expected, actual, sizeof(expected));
    zexpect_u32_eq(0xA5A5A5A5, actual[384]);
}

ZTEST(mdec, table_changes_apply_to_next_decode_command) {
    uint32_t pixels[192];
    zassert_s32_eq(0, Psyz_MdecCommand(0x3000000F, mdec_nonflat + 1, 15));
    load_quant(1);
    zassert_s32_eq(0, Psyz_MdecRead(pixels, 192));
    zexpect_u8array_eq(mdec_rgb24_expected, pixels, sizeof(pixels));
    check_reference(
        2u << 27, mdec_nonflat + 1, 15, mdec_custom_quant_expected, 192);
}

ZTEST(mdec, reset_discards_pixels_and_preserves_tables) {
    uint32_t pixels[192];
    load_quant(1);
    zassert_s32_eq(0, Psyz_MdecCommand(0x3000000F, mdec_nonflat + 1, 15));
    zassert_s32_eq(0, Psyz_MdecRead(pixels, 32));
    Psyz_MdecReset();
    zexpect_s32_eq(-1, Psyz_MdecRead(pixels, 192));
    for (unsigned i = 0; i < 192; ++i)
        zassert_u32_eq(0, pixels[i]);
    check_reference(
        2u << 27, mdec_nonflat + 1, 15, mdec_custom_quant_expected, 192);
}

ZTEST(mdec, output_overrun_and_recovery) {
    uint32_t data[6], pixels[257];
    make_flat(data, 1);
    memset(pixels, 0xA5, sizeof(pixels));
    zassert_s32_eq(0, Psyz_MdecCommand(0x38000006, data, 6));
    zexpect_s32_eq(-1, Psyz_MdecRead(pixels, 256));
    for (unsigned i = 0; i < 256; ++i)
        zassert_u32_eq(i < 128 ? 0x42104210 : 0, pixels[i]);
    zexpect_u32_eq(0xA5A5A5A5, pixels[256]);
    zassert_s32_eq(0, Psyz_MdecCommand(0x38000006, data, 6));
    zassert_s32_eq(0, Psyz_MdecRead(pixels, 128));
}

ZTEST(mdec, truncated_and_malformed_coefficients) {
    uint32_t data[6], pixels[193];
    for (unsigned malformed = 0; malformed < 2; ++malformed) {
        make_flat(data, 1);
        if (malformed)
            data[0] = 0xFC010400;
        unsigned count = malformed ? 6 : 3;
        memset(pixels, 0xA5, sizeof(pixels));
        zassert_s32_eq(0, Psyz_MdecCommand(0x30000000 | count, data, count));
        zexpect_s32_eq(-1, Psyz_MdecRead(pixels, 192));
        for (unsigned i = 0; i < 192; ++i)
            zassert_u32_eq(0, pixels[i]);
        zexpect_u32_eq(0xA5A5A5A5, pixels[192]);
    }
}

ZTEST(mdec, full_block_does_not_require_end_code) {
    uint8_t data[148] = {0};
    data[1] = 4;
    for (unsigned i = 0; i < 5; ++i) {
        data[129 + i * 4] = 4;
        data[130 + i * 4] = 0;
        data[131 + i * 4] = 0xFE;
    }
    uint32_t pixels[192];
    zassert_s32_eq(0, Psyz_MdecCommand(0x30000025, data, 37));
    zassert_s32_eq(0, Psyz_MdecRead(pixels, 192));
    for (unsigned i = 0; i < 192; ++i)
        zassert_u32_eq(0x80808080, pixels[i]);
}

ZTEST(mdec, invalid_commands_reject_before_reading_input) {
    const uint32_t value = 0;
    const uint32_t commands[] = {
        0, 0x80000000, 0x30000002, 0x40000000, 0x40000001, 0x60000000};
    for (unsigned i = 0; i < sizeof(commands) / sizeof(*commands); ++i)
        zexpect_s32_eq(-1, Psyz_MdecCommand(commands[i], &value, 1));
    zexpect_s32_eq(-1, Psyz_MdecCommand(0x30000001, NULL, 1));
    zexpect_s32_eq(-1, Psyz_MdecCommand(0x30000000, &value, SIZE_MAX));
    check_reference(2u << 27, mdec_nonflat + 1, 15, mdec_rgb24_expected, 192);
}

ZTEST(mdec, invalid_output_sizes_leave_destination_untouched) {
    uint32_t pixel = 0xA5A5A5A5;
    zexpect_s32_eq(-1, Psyz_MdecRead(NULL, 1));
    zexpect_s32_eq(-1, Psyz_MdecRead(&pixel, SIZE_MAX / 4 + 1));
    zexpect_u32_eq(0xA5A5A5A5, pixel);
    zexpect_s32_eq(0, Psyz_MdecRead(NULL, 0));
}

ZTEST(mdec, unaligned_input_and_output) {
    uint8_t data[61], pixels[770];
    memcpy(data + 1, mdec_nonflat + 1, 60);
    memset(pixels, 0xA5, sizeof(pixels));
    zassert_s32_eq(0, Psyz_MdecCommand(0x3000000F, data + 1, 15));
    zassert_s32_eq(0, Psyz_MdecRead(pixels + 1, 192));
    zexpect_u8array_eq(mdec_rgb24_expected, pixels + 1, 768);
    zexpect_u8_eq(0xA5, pixels[0]);
    zexpect_u8_eq(0xA5, pixels[769]);
}

ZTEST(mdec, maximum_input_count) {
    uint32_t* data = malloc(UINT16_MAX * sizeof(*data));
    zassert_u32_eq(1, data != NULL);
    for (unsigned i = 0; i < UINT16_MAX; ++i)
        data[i] = 0xFE000400;
    zexpect_s32_eq(0, Psyz_MdecCommand(0x3000FFFF, data, UINT16_MAX));
    free(data);
    uint32_t pixels[192];
    zassert_s32_eq(0, Psyz_MdecRead(pixels, 192));
    for (unsigned i = 0; i < 192; ++i)
        zassert_u32_eq(0x80808080, pixels[i]);
}
#endif
