#include <psyz.h>
#include <libpress.h>
#include <stdint.h>

typedef struct {
    const uint8_t* data;
    size_t bit, bytes;
    int error;
} Bits;
static unsigned take(Bits* bits, unsigned count) {
    unsigned value = 0;
    for (unsigned i = 0; i < count; ++i) {
        size_t half = bits->bit / 16;
        if (half * 2 + 1 >= bits->bytes) {
            bits->error = 1;
            return 0;
        }
        unsigned word = bits->data[half * 2] | (bits->data[half * 2 + 1] << 8);
        value = (value << 1) | ((word >> (15 - bits->bit % 16)) & 1);
        ++bits->bit;
    }
    return value;
}

typedef struct {
    unsigned prefix, length, index_bits, offset;
} AcGroup;
static const AcGroup ac_groups[] = {
    {3, 2, 0, 0},  {3, 3, 0, 1},   {2, 3, 1, 2},   {3, 4, 1, 4},  {5, 5, 0, 6},
    {4, 5, 3, 7},  {1, 4, 2, 15},  {1, 5, 2, 19},  {1, 7, 3, 23}, {1, 8, 4, 31},
    {1, 9, 4, 47}, {1, 10, 4, 63}, {1, 11, 4, 79}, {1, 12, 4, 95}};
static const uint16_t ac_values[] = {
    0x0001, 0x0401, 0x0002, 0x0801, 0x1001, 0x0c01, 0x0003, 0x3401, 0x0006,
    0x3001, 0x2c01, 0x0c02, 0x0403, 0x0005, 0x2801, 0x1c01, 0x1801, 0x0402,
    0x1401, 0x0802, 0x2401, 0x0004, 0x2001, 0x4001, 0x1402, 0x0007, 0x0803,
    0x0404, 0x3c01, 0x3801, 0x1002, 0x000b, 0x2002, 0x1003, 0x000a, 0x0804,
    0x1c02, 0x5401, 0x5001, 0x0009, 0x4c01, 0x4801, 0x0405, 0x0c03, 0x0008,
    0x1802, 0x4401, 0x2802, 0x2402, 0x1403, 0x0c04, 0x0805, 0x0407, 0x0406,
    0x000f, 0x000e, 0x000d, 0x000c, 0x6801, 0x6401, 0x6001, 0x5c01, 0x5801,
    0x001f, 0x001e, 0x001d, 0x001c, 0x001b, 0x001a, 0x0019, 0x0018, 0x0017,
    0x0016, 0x0015, 0x0014, 0x0013, 0x0012, 0x0011, 0x0010, 0x0028, 0x0027,
    0x0026, 0x0025, 0x0024, 0x0023, 0x0022, 0x0021, 0x0020, 0x040e, 0x040d,
    0x040c, 0x040b, 0x040a, 0x0409, 0x0408, 0x0412, 0x0411, 0x0410, 0x040f,
    0x1803, 0x4002, 0x3c02, 0x3802, 0x3402, 0x3002, 0x2c02, 0x7c01, 0x7801,
    0x7401, 0x7001, 0x6c01};
static unsigned ac(Bits* bits) {
    unsigned prefix = 0;
    for (unsigned length = 1; length <= 12; ++length) {
        prefix = (prefix << 1) | take(bits, 1);
        if (bits->error)
            break;
        if (length == 2 && prefix == 2)
            return 0xfe00;
        if (length == 6 && prefix == 1)
            return take(bits, 16);
        for (size_t i = 0; i < sizeof(ac_groups) / sizeof(*ac_groups); ++i) {
            const AcGroup* group = &ac_groups[i];
            if (length == group->length && prefix == group->prefix) {
                unsigned value =
                    ac_values[group->offset + take(bits, group->index_bits)];
                if (take(bits, 1))
                    value = (value & 0xfc00) | ((-(int)value) & 0x3ff);
                return value;
            }
        }
    }
    bits->error = 1;
    return 0;
}
static int dc3(Bits* bits, int chroma, int* end) {
    static const unsigned chroma_codes[9][2] = {
        {0, 2},  {1, 2},  {2, 2},   {6, 3},  {14, 4},
        {30, 5}, {62, 6}, {126, 7}, {254, 8}};
    static const unsigned luma_codes[9][2] = {
        {4, 3},  {0, 2},  {1, 2},  {5, 3},  {6, 3},
        {14, 4}, {30, 5}, {62, 6}, {126, 7}};
    unsigned prefix = 0;
    for (unsigned length = 1; length <= 10; ++length) {
        prefix = (prefix << 1) | take(bits, 1);
        if (bits->error)
            break;
        if (length == 10 && prefix == 1023) {
            *end = 1;
            return 0;
        }
        const unsigned (*codes)[2] = chroma ? chroma_codes : luma_codes;
        for (unsigned n = 0; n <= 8; ++n) {
            if (length == codes[n][1] && prefix == codes[n][0]) {
                if (!n)
                    return 0;
                unsigned value = take(bits, n);
                return ((value & (1u << (n - 1)))
                            ? (int)value
                            : (int)value - ((1 << n) - 1)) *
                       4;
            }
        }
    }
    bits->error = 1;
    return 0;
}

typedef struct {
    Bits bits;
    uint8_t* output;
    unsigned capacity, quant, version;
    size_t count, blocks;
    int previous[3], active, need_dc;
} VlcDecoder;

static int begin(VlcDecoder* decoder, const void* data, size_t bytes,
                 u_long* output, size_t words) {
    const uint8_t* src = data;
    if (!src || !output || bytes < 8)
        return -1;
    unsigned capacity = src[0] | ((unsigned)src[1] << 8);
    unsigned quant = src[4] | ((unsigned)src[5] << 8);
    unsigned version = src[6] | ((unsigned)src[7] << 8);
    if (src[2] != 0 || src[3] != 0x38 || quant > 63 || version < 1 ||
        version > 3 || !capacity || words < capacity + 1u)
        return -1;
    *decoder = (VlcDecoder){
        .bits = {src + 8, 0, bytes - 8, 0},
        .output = (uint8_t*)output,
        .capacity = capacity,
        .quant = quant,
        .version = version,
        .active = 1,
        .need_dc = 1};
    return 0;
}

static int emit(VlcDecoder* decoder, unsigned word) {
    if (decoder->count >= decoder->capacity * 2u)
        return -1;
    size_t offset = 4 + decoder->count++ * 2;
    decoder->output[offset] = word;
    decoder->output[offset + 1] = word >> 8;
    return 0;
}

static int decode(VlcDecoder* decoder, size_t chunk_words) {
    size_t stop =
        decoder->count ? decoder->count + chunk_words * 2 : chunk_words * 2 - 2;
    while (decoder->active) {
        if (decoder->need_dc) {
            int end = 0, dc;
            unsigned block = decoder->blocks % 6;
            if (decoder->version == 3) {
                unsigned channel = block < 2 ? block : 2;
                dc = decoder->previous[channel] +
                     dc3(&decoder->bits, block < 2, &end);
                decoder->previous[channel] = ((dc + 512) & 1023) - 512;
            } else {
                dc = take(&decoder->bits, 10);
                end = dc == 511;
            }
            if (decoder->bits.error)
                break;
            if (end) {
                if (block != 0 || !decoder->blocks)
                    break;
                while (decoder->count < decoder->capacity * 2u)
                    emit(decoder, 0xfe00);
                decoder->output[0] = decoder->capacity;
                decoder->output[1] = decoder->capacity >> 8;
                decoder->output[2] = 0;
                decoder->output[3] = 0x38;
                decoder->active = 0;
                return 0;
            }
            if (emit(decoder, (decoder->quant << 10) | (dc & 1023)))
                break;
            decoder->need_dc = 0;
            if (chunk_words && decoder->count > stop)
                return 1;
        }
        unsigned coefficient = 0;
        for (;;) {
            unsigned word = ac(&decoder->bits);
            if (decoder->bits.error || emit(decoder, word))
                goto failure;
            if (word == 0xfe00)
                break;
            coefficient += (word >> 10) + 1;
            if (coefficient >= 64)
                goto failure;
        }
        ++decoder->blocks;
        decoder->need_dc = 1;
    }
failure:
    decoder->active = 0;
    return -1;
}

static VlcDecoder sdk_decoder;
static unsigned sdk_chunk_halfwords = 0xffffff;
int DecDCTvlc(u_long* bs, u_long* output) {
    if (bs && output) {
        const uint8_t* src = (const uint8_t*)bs;
        unsigned capacity = src[0] | ((unsigned)src[1] << 8);
        sdk_decoder.active = 0;
        if (begin(&sdk_decoder, bs, (size_t)capacity * 8 + 16, output,
                  capacity + 1u))
            return -1;
        for (unsigned i = 0; i < 4; ++i)
            sdk_decoder.output[i] = src[i];
    } else if (bs || output || !sdk_decoder.active) {
        return -1;
    }
    return decode(
        &sdk_decoder,
        sdk_chunk_halfwords == 0xffffff ? 0 : sdk_chunk_halfwords / 2);
}
int DecDCTvlcSize(int size) {
    int previous = sdk_chunk_halfwords;
    sdk_chunk_halfwords = size > 1 ? (unsigned)size * 2 : 0xffffff;
    return previous;
}
