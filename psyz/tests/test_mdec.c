#include "ztest.h"
#include <string.h>
#include "../src/psyz/mdec.h"
#ifdef __psx__
#include <malloc.h>
#else
#include <stdlib.h>
#endif

static const uint32_t mdec_mono[] = {0x00091010, 0x080407FE, 0xFE0013F0};

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

static const unsigned mdec_words[] = {8, 16, 192, 128};

static uint32_t fnv1a(uint32_t hash, const void* data, size_t size) {
    const uint8_t* bytes = data;
    for (size_t i = 0; i < size; ++i)
        hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}

static uint32_t decode_hash(
    uint32_t hash, uint32_t flags, const uint32_t* data, unsigned count,
    unsigned macroblocks, const uint8_t quant[128], const uint16_t scale[64]) {
    static uint32_t pixels[385];
    unsigned words = mdec_words[(flags >> 27) & 3] * macroblocks;
    memset(pixels, 0xA5, sizeof(pixels));
    zassert_s32_eq(0, Psyz_MdecCommand(0x40000001, quant, 32));
    zassert_s32_eq(0, Psyz_MdecCommand(0x60000000, scale, 32));
    zassert_s32_eq(
        0, Psyz_MdecCommand(0x20000000 | flags | count, data, count));
    zassert_s32_eq(0, Psyz_MdecRead(pixels, words));
    zexpect_u32_eq(0xA5A5A5A5, pixels[words]);
    return fnv1a(hash, pixels, words * 4);
}
static void default_tables(uint8_t quant[128], uint16_t scale[64]) {
    memcpy(quant, mdec_quant, 64);
    memcpy(quant + 64, mdec_quant, 64);
    memcpy(scale, mdec_scale, 128);
}

static unsigned pack_halfwords(
    uint32_t* data, uint16_t* halfwords, unsigned count) {
    if (count & 1)
        halfwords[count++] = 0xFE00;
    for (unsigned i = 0; i < count / 2; ++i)
        data[i] = halfwords[i * 2] | ((uint32_t)halfwords[i * 2 + 1] << 16);
    return count / 2;
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
    return pack_halfwords(data, coefficients, count);
}

static const uint32_t mdec_rgb24_expected[] = {
    0x87A99191, 0x83769F87, 0x8B82758C, 0x728C8974, 0x7D678A87, 0x80735D8A,
    0x6CA48374, 0x74679C7B, 0xA87C6FA0, 0x7EB08E76, 0x9673B896, 0xA18D6AAA,
    0x89AA9292, 0x8578A189, 0x8D84778E, 0x748D8A75, 0x7F698C89, 0x82755F8C,
    0x67A07F70, 0x6F629776, 0xA3776A9B, 0x79AC8A72, 0x916EB391, 0x9D8966A5,
    0x8FA69599, 0x877F9C8B, 0x88857D8A, 0x7B898C7C, 0x806F888B, 0x7D766587,
    0x6592776D, 0x68608A6F, 0x9570688D, 0x779E836F, 0x8A6DA68B, 0x9181649A,
    0x91A8979B, 0x88809E8D, 0x8A877F8B, 0x7C8B8E7E, 0x8271898C, 0x7F786789,
    0x608E7369, 0x635B856A, 0x906B6388, 0x729A7F6B, 0x8568A186, 0x8D7D6095,
    0x979F95A1, 0x8786958B, 0x81868582, 0x80828E82, 0x8177808C, 0x76776D80,
    0x6685726F, 0x62617C69, 0x886A6980, 0x78917E71, 0x856C9885, 0x847D648C,
    0x959D939F, 0x86859389, 0x7F848381, 0x7F808C80, 0x7F757F8B, 0x74756B7E,
    0x6B897673, 0x6766816E, 0x8D6F6E85, 0x7D958275, 0x8A719D8A, 0x88816891,
    0x9695909F, 0x81868C87, 0x78808579, 0x80768881, 0x7C777587, 0x6B726D75,
    0x75897C7E, 0x6D6F8073, 0x8C757784, 0x87978780, 0x8F7B9E8E, 0x88877390,
    0x94948F9E, 0x7F848A85, 0x767E8377, 0x7E758780, 0x7A757385, 0x69706B73,
    0x7A8D8082, 0x72748578, 0x917A7C89, 0x8C9B8B84, 0x9480A393, 0x8C8B7795,
    0x8A8C8796, 0x757A807B, 0x70787D6D, 0x85768881, 0x7F7A7A8C, 0x6C736E78,
    0x7E96898B, 0x7173897C, 0x8F787A88, 0x8B9B8B84, 0x8E7AA292, 0x81806C8F,
    0x8F928D9C, 0x7A7F8580, 0x757D8272, 0x8A7C8E87, 0x85807F91, 0x7178737E,
    0x78908385, 0x6B6D8376, 0x89727482, 0x8495857E, 0x87739B8B, 0x7B7A6688,
    0x949E94A0, 0x84839288, 0x8287867F, 0x8E8A968A, 0x8E848E9A, 0x8081778D,
    0x6B8E7B78, 0x6261816E, 0x86686780, 0x77917E71, 0x7F669784, 0x79725986,
    0x99A49AA6, 0x8988978D, 0x888D8C84, 0x93909C90, 0x9389939F, 0x85867C92,
    0x64887572, 0x5B5A7A67, 0x80626179, 0x718A776A, 0x7960917E, 0x736C5380,
    0x93AD9CA0, 0x8A82A08F, 0x918E868D, 0x8F999C8C, 0x94839C9F, 0x8E87769B,
    0x5E91766C, 0x5C548368, 0x88635B81, 0x6B937864, 0x795C9A7F, 0x7C6C4F89,
    0x8EA7969A, 0x857D9B8A, 0x8B888088, 0x8A939686, 0x8F7E979A, 0x89827196,
    0x65977C72, 0x635B8A6F, 0x8E696188, 0x719A7F6B, 0x7F62A085, 0x8272558F,
    0x82A78F8F, 0x7E719A82, 0x8A817487, 0x7E93907B, 0x88729693, 0x887B6595,
    0x6AA78677, 0x6D609A79, 0xA0746799, 0x76AA8870, 0x8966B08E, 0x907C599D,
    0x7DA18989, 0x796C957D, 0x857C6F82, 0x798D8A75, 0x826C918E, 0x8376608F,
    0x70AD8C7D, 0x7366A07F, 0xA67A6D9F, 0x7DB08E76, 0x906DB795, 0x96825FA4,
};

static const uint32_t mdec_rgb555_expected[] = {
    0x52315652, 0x460F4A0F, 0x462E462E, 0x41CC460D, 0x4DED520E, 0x55EE51CD,
    0x5E705A4F, 0x524D566E, 0x52315652, 0x4A0F4A2F, 0x462E4A2F, 0x41EC460D,
    0x4DED520E, 0x51ED4DCC, 0x5A4F5A2E, 0x522D564E, 0x52325673, 0x46304630,
    0x462F464F, 0x41ED460E, 0x45CD49EE, 0x4DCD49AC, 0x562F520E, 0x4A0C4E2E,
    0x52525673, 0x46304630, 0x464F4650, 0x41ED460E, 0x45AC49CD, 0x49AC458B,
    0x522E4E0D, 0x4A0C4E2D, 0x4E335274, 0x42314231, 0x42304250, 0x3DEE420F,
    0x3DAD45CE, 0x45AD418C, 0x4E2F4A0E, 0x460C4A2D, 0x4A335254, 0x42104231,
    0x42304230, 0x39ED420F, 0x41CD45EE, 0x49CE45AD, 0x52304E0F, 0x460D4A2E,
    0x4A334E54, 0x3E113E11, 0x3E303E30, 0x35CE3E0F, 0x41CF45F0, 0x45EF41CE,
    0x52514E30, 0x462E4A4F, 0x46324E54, 0x3E103E11, 0x3A303E30, 0x35CD39EF,
    0x45EF4A10, 0x49EF45CE, 0x52514E30, 0x4A2F4E50, 0x41F14A33, 0x39F039EF,
    0x3E513E30, 0x35CE3E0F, 0x45F04E31, 0x49EF45CE, 0x52514E30, 0x420D4A4F,
    0x46124A53, 0x3E1039F0, 0x42513E51, 0x39EE4230, 0x41EF4A11, 0x45CE41AE,
    0x4E304E30, 0x3DED462E, 0x4A335274, 0x42314210, 0x4A724671, 0x420F4A50,
    0x41CD49EF, 0x45AD418C, 0x4E0F4A0E, 0x3DCB460D, 0x4E535675, 0x46524631,
    0x4A924A72, 0x462F4A51, 0x3DAC45EE, 0x418C3D6B, 0x4A0E45ED, 0x39AA41EC,
    0x52525A74, 0x4A514A30, 0x4E924E91, 0x4A2F4E70, 0x41AC49EE, 0x458B418B,
    0x4E0D49ED, 0x41AA45EB, 0x4E325673, 0x46304630, 0x4E714A71, 0x460E4E50,
    0x45CD4DEE, 0x49AC458B, 0x522E4E0D, 0x41CB4A0C, 0x4E105652, 0x460F460E,
    0x4E504A4F, 0x45ED4E2E, 0x4DED562F, 0x51CD4DCC, 0x5A4F562E, 0x4A0B522D,
    0x4E105231, 0x45EE41EE, 0x4A4F4A2F, 0x41EC4A0E, 0x520E5A30, 0x55EE51CD,
    0x5E705A4F, 0x4E0C564E,
};

static const uint32_t mdec_custom_quant_expected[] = {
    0x819F8C87, 0x847A9986, 0x8D83798E, 0x758E8776, 0x80708D86, 0x887B6B8D,
    0x6F988074, 0x786D937B, 0x987C7194, 0x7A9F8676, 0x8B74A38A, 0x96866F9B,
    0x82A08D88, 0x857B9A87, 0x8E847A8F, 0x768F8877, 0x81718E87, 0x897C6C8E,
    0x6D957D71, 0x756A9179, 0x967A6F91, 0x789D8474, 0x8871A188, 0x94846D98,
    0x879D8E8C, 0x867D9889, 0x8D867D8D, 0x7B8C897B, 0x82758C89, 0x867D708B,
    0x6D8E7971, 0x72688A75, 0x8F766C8B, 0x76958071, 0x856F9A85, 0x90816B94,
    0x889E8F8D, 0x877E998A, 0x8E877E8E, 0x7C8D8A7C, 0x83768D8A, 0x877E718C,
    0x6A8C776F, 0x70668772, 0x8D746A89, 0x73937E6F, 0x836D9782, 0x8D7E6892,
    0x8B998E90, 0x86819489, 0x8A86818A, 0x7F88897F, 0x83788889, 0x827E7387,
    0x6C877771, 0x6F698272, 0x89736D85, 0x76907D72, 0x82709481, 0x877D6B8C,
    0x8A988D8F, 0x85809388, 0x89858089, 0x7E87887E, 0x82778788, 0x817D7286,
    0x6F897973, 0x716B8575, 0x8B756F87, 0x79927F74, 0x84729784, 0x8A806E8E,
    0x89938C8F, 0x83828D86, 0x84828185, 0x7D82877E, 0x80788186, 0x7D7B7382,
    0x74887C78, 0x74718478, 0x8C797687, 0x7E92837A, 0x87789687, 0x8A83748E,
    0x88928B8E, 0x82818C85, 0x83818084, 0x7C81867D, 0x7F778085, 0x7C7A7281,
    0x768B7F7B, 0x7774867A, 0x8E7B788A, 0x8094857C, 0x8A7B9889, 0x8C857691,
    0x8590898C, 0x7E7D8982, 0x82807F80, 0x82848980, 0x847C868B, 0x807E7686,
    0x778E827E, 0x7572877B, 0x8B787588, 0x7E92837A, 0x84759687, 0x847D6E8B,
    0x88928B8E, 0x81808C85, 0x85838283, 0x85878C83, 0x877F898E, 0x82807889,
    0x748B7F7B, 0x716E8478, 0x88757284, 0x7A8F8077, 0x81729283, 0x817A6B88,
    0x8B9A8F91, 0x86819489, 0x8C88838A, 0x888F9086, 0x8C819192, 0x89857A90,
    0x6D8A7A74, 0x6D678373, 0x86706A83, 0x748E7B70, 0x7C6A927F, 0x7F756386,
    0x8D9D9294, 0x8984968B, 0x8F8B868D, 0x8B929389, 0x8F849495, 0x8C887D93,
    0x6A877771, 0x69638070, 0x836D677F, 0x708B786D, 0x79678E7B, 0x7C726083,
    0x8AA29391, 0x8A819B8C, 0x938C8391, 0x88979486, 0x8F829996, 0x91887B98,
    0x688C776F, 0x6A608570, 0x876E6483, 0x6D8E796A, 0x7A64917C, 0x82735D89,
    0x889F908E, 0x877E998A, 0x9089808E, 0x85949183, 0x8C7F9693, 0x8E857895,
    0x6B8F7A72, 0x6E648873, 0x8A716787, 0x71917C6D, 0x7D679580, 0x8576608C,
    0x819F8C87, 0x83799986, 0x8F857B8D, 0x7E948D7C, 0x8878968F, 0x8E817195,
    0x6D988074, 0x72679179, 0x92766B8E, 0x749A8171, 0x826B9D84, 0x8B7B6492,
    0x7E9D8A85, 0x80769683, 0x8C82788A, 0x7B918A79, 0x8575938C, 0x8C7F6F92,
    0x709B8377, 0x766B947C, 0x95796E92, 0x789D8474, 0x856EA188, 0x8E7E6795,
};

static const uint32_t mdec_depth0_expected[] = {
    0x99998777, 0x6788889A, 0x56789ABC, 0x678899AB,
    0x99998888, 0xAAA98777, 0x88988889, 0x567889BB,
};

static const uint32_t mdec_depth1_expected[] = {
    0x786C6C70, 0x9296968A, 0x8387939E, 0x67737E83, 0x8C9DB2C2, 0x48586E7F,
    0x8A94A4B2, 0x5C6A7A84, 0x80797C82, 0x8C92958D, 0x7B6D6A6E, 0x9DA09E8F,
    0x7F7D858D, 0x78818886, 0x8695A8B7, 0x4A5A6D7C,
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

static uint32_t random_state;

static unsigned random_below(unsigned n) {
    random_state = random_state * 1103515245u + 12345u;
    return ((random_state >> 16) * n) >> 16;
}

static unsigned random_runlevels(uint32_t data[512], unsigned blocks) {
    uint16_t halfwords[1024];
    unsigned count = 0;
    int carried = 0;
    for (unsigned block = 0; block < blocks; ++block) {
        if (!carried) {
            unsigned first = (random_below(64) << 10) | random_below(1024);
            halfwords[count++] = first == 0xFE00 ? 0xFE01 : first;
        }
        carried = 0;
        for (unsigned index = 0; index < 63;) {
            unsigned run = random_below(4) ? random_below(4) : random_below(64);
            unsigned halfword = (run << 10) | random_below(1024);
            halfwords[count++] = halfword == 0xFE00 ? 0xFE01 : halfword;
            index += run + 1;
            if (index > 63)
                carried = 1;
            else if (index < 63 && !random_below(12)) {
                halfwords[count++] = 0xFE00;
                break;
            }
        }
    }
    return pack_halfwords(data, halfwords, count);
}

static void check_flag(uint32_t flags, uint32_t flag, const uint32_t* data,
                       unsigned count, uint32_t changed_bits) {
    uint32_t without[192], with[192];
    unsigned words = mdec_words[(flags >> 27) & 3];
    zassert_s32_eq(
        0, Psyz_MdecCommand(0x20000000 | flags | count, data, count));
    zassert_s32_eq(0, Psyz_MdecRead(without, words));
    zassert_s32_eq(
        0, Psyz_MdecCommand(0x20000000 | flags | flag | count, data, count));
    zassert_s32_eq(0, Psyz_MdecRead(with, words));
    for (unsigned i = 0; i < words; ++i)
        zassert_u32_eq(without[i] ^ changed_bits, with[i]);
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

ZTEST_TEARDOWN(mdec) { Psyz_MdecReset(); }

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

ZTEST(mdec, monochrome_reference) {
    check_reference(0, mdec_mono, 3, mdec_depth0_expected, 8);
    check_reference(1u << 27, mdec_mono, 3, mdec_depth1_expected, 16);
}

ZTEST(mdec, custom_quantization_reference) {
    load_quant(1);
    check_reference(
        2u << 27, mdec_nonflat + 1, 15, mdec_custom_quant_expected, 192);
}

ZTEST(mdec, signed_output_flips_sign_bits) {
    check_flag(0, 1u << 26, mdec_mono, 3, 0x88888888);
    check_flag(1u << 27, 1u << 26, mdec_mono, 3, 0x80808080);
    check_flag(2u << 27, 1u << 26, mdec_nonflat + 1, 15, 0x80808080);
    check_flag(3u << 27, 1u << 26, mdec_nonflat + 1, 15, 0x42104210);
}

ZTEST(mdec, bit15_only_affects_rgb555) {
    check_flag(0, 1u << 25, mdec_mono, 3, 0);
    check_flag(1u << 27, 1u << 25, mdec_mono, 3, 0);
    check_flag(2u << 27, 1u << 25, mdec_nonflat + 1, 15, 0);
    check_flag(3u << 27, 1u << 25, mdec_nonflat + 1, 15, 0x80008000);
    check_flag(7u << 26, 1u << 25, mdec_nonflat + 1, 15, 0x80008000);
}

ZTEST(mdec, custom_quantization_tables) {
    uint8_t quant[128];
    uint16_t scale[64];
    default_tables(quant, scale);
    for (unsigned i = 0; i < 128; ++i)
        quant[i] = (i * 17 + 3) & 255;
    uint32_t hash = 2166136261u;
    hash = decode_hash(hash, 2u << 27, mdec_nonflat + 1, 15, 1, quant, scale);
    hash = decode_hash(hash, 3u << 27, mdec_nonflat + 1, 15, 1, quant, scale);
    zexpect_u32_eq(0x1E1386E7, hash);
}

ZTEST(mdec, custom_scale_table) {
    uint8_t quant[128];
    uint16_t scale[64];
    default_tables(quant, scale);
    for (unsigned i = 0; i < 64; ++i)
        scale[i] ^= (i * 13 + 9) & 255;
    uint32_t hash = 2166136261u;
    hash = decode_hash(hash, 2u << 27, mdec_nonflat + 1, 15, 1, quant, scale);
    hash = decode_hash(hash, 3u << 27, mdec_nonflat + 1, 15, 1, quant, scale);
    zexpect_u32_eq(0x98294EAA, hash);
}

ZTEST(mdec, generated_runlevels) {
    const unsigned scales[] = {1, 4, 63};
    uint8_t quant[128];
    uint16_t scale[64];
    uint32_t data[192];
    default_tables(quant, scale);
    uint32_t hash = 2166136261u;
    for (unsigned s = 0; s < 3; ++s) {
        for (unsigned depth = 0; depth < 4; ++depth) {
            unsigned count =
                generated_runlevels(data, depth < 2 ? 1 : 6, scales[s], 0);
            hash = decode_hash(hash, depth << 27, data, count, 1, quant, scale);
        }
    }
    zexpect_u32_eq(0x3301DD45, hash);
}

ZTEST(mdec, zero_quantization_scale) {
    uint8_t quant[128];
    uint16_t scale[64];
    uint32_t data[192];
    default_tables(quant, scale);
    uint32_t hash = 2166136261u;
    for (unsigned depth = 0; depth < 4; ++depth) {
        unsigned count = generated_runlevels(data, depth < 2 ? 1 : 6, 0, 0);
        hash = decode_hash(hash, depth << 27, data, count, 1, quant, scale);
    }
    zexpect_u32_eq(0x74430DA3, hash);
}

ZTEST(mdec, dense_blocks_without_end_codes) {
    uint8_t quant[128];
    uint16_t scale[64];
    uint32_t data[192];
    default_tables(quant, scale);
    uint32_t hash = 2166136261u;
    for (unsigned depth = 0; depth < 4; ++depth) {
        unsigned count = generated_runlevels(data, depth < 2 ? 1 : 6, 4, 1);
        hash = decode_hash(hash, depth << 27, data, count, 1, quant, scale);
    }
    zexpect_u32_eq(0x32699428, hash);
}

ZTEST(mdec, random_macroblocks) {
    static uint32_t data[512];
    uint8_t quant[128];
    uint16_t scale[64];
    uint32_t hash = 2166136261u;
    random_state = 1;
    for (unsigned pass = 0; pass < 16; ++pass) {
        unsigned depth = pass & 3;
        for (unsigned i = 0; i < 128; ++i)
            quant[i] = random_below(256);
        for (unsigned i = 0; i < 64; ++i)
            scale[i] = random_below(65536);
        uint32_t flags = (depth << 27) | (random_below(4) << 25);
        unsigned count = random_runlevels(data, depth < 2 ? 2 : 12);
        hash = decode_hash(hash, flags, data, count, 2, quant, scale);
    }
    zexpect_u32_eq(0x98EB8F52, hash);
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
    zskip_targets("ps1");
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

ZTEST(mdec, truncated_input_fails_read) {
    zskip_targets("ps1");
    uint32_t data[6], pixels[193];
    make_flat(data, 1);
    memset(pixels, 0xA5, sizeof(pixels));
    zassert_s32_eq(0, Psyz_MdecCommand(0x30000003, data, 3));
    zexpect_s32_eq(-1, Psyz_MdecRead(pixels, 192));
    for (unsigned i = 0; i < 192; ++i)
        zassert_u32_eq(0, pixels[i]);
    zexpect_u32_eq(0xA5A5A5A5, pixels[192]);
}

ZTEST(mdec, run_past_last_coefficient_starts_next_block) {
    uint16_t overrun[] = {0x0810, 0x1408, 0xF81F, 0xFE00, 0x0400, 0xFE00,
                          0x0400, 0xFE00, 0x0400, 0xFE00, 0x0400, 0xFE00};
    uint16_t ended[] = {0x0810, 0x1408, 0xFE00, 0xF81F, 0xFE00, 0x0400, 0xFE00,
                        0x0400, 0xFE00, 0x0400, 0xFE00, 0x0400, 0xFE00, 0xFE00};
    uint32_t data[7], expected[192], actual[192];
    unsigned count = pack_halfwords(data, ended, 14);
    zassert_s32_eq(0, Psyz_MdecCommand(0x30000000 | count, data, count));
    zassert_s32_eq(0, Psyz_MdecRead(expected, 192));
    count = pack_halfwords(data, overrun, 12);
    zassert_s32_eq(0, Psyz_MdecCommand(0x30000000 | count, data, count));
    zassert_s32_eq(0, Psyz_MdecRead(actual, 192));
    zexpect_u8array_eq(expected, actual, sizeof(actual));
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

ZTEST(mdec, empty_decode_has_no_pixels) {
    uint32_t pixel = 0xA5A5A5A5;
    zassert_s32_eq(0, Psyz_MdecCommand(0x30000000, NULL, 0));
    zexpect_s32_eq(0, Psyz_MdecRead(NULL, 0));
    zexpect_s32_eq(-1, Psyz_MdecRead(&pixel, 1));
    zexpect_u32_eq(0, pixel);
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
