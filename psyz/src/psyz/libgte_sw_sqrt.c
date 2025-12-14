#include <psyz.h>
#include <libgte.h>
#include <log.h>

extern short SQRT[192];

// Forward declaration of helper function from libgte.c
unsigned int gte_leadingzerocount(unsigned int lzcs);

long SquareRoot0_impl(long a) {
    unsigned int lzc;
    long shift;
    long result_shift;
    long table_shift;
    long normalized;

    if (a == 0) {
        return 0;
    }
    if (a < 0) {
        WARNF("SquareRoot0(%d) negative value", a);
    }

    lzc = gte_leadingzerocount(a);
    if (lzc == 0x20) {
        return 0;
    }

    shift = lzc & ~1; // Make even
    table_shift = shift - 0x18;
    if (table_shift >= 0) {
        normalized = a << table_shift;
    } else {
        normalized = a >> (0x18 - shift);
    }

    normalized -= 0x40;
    if (normalized < 0 || normalized >= LEN(SQRT)) {
        WARNF("SquareRoot0(%d) out of bounds", a);
        normalized %= 192;
    }
    result_shift = (0x1F - shift) >> 1;
    return (long)(SQRT[normalized] << result_shift) >> 12;
}

long SquareRoot12_impl(long a) {
    unsigned int lzc;
    long shift;
    long result_shift;
    long table_shift;
    long normalized;

    if (a == 0) {
        return 0;
    }
    if (a < 0) {
        WARNF("SquareRoot12(%d) negative value", a);
    }

    lzc = gte_leadingzerocount(a);
    if (lzc == 0x20) {
        return 0;
    }

    shift = lzc & ~1; // Make even
    table_shift = shift - 0x18;
    if (table_shift >= 0) {
        normalized = a << table_shift;
    } else {
        normalized = a >> (0x18 - shift);
    }

    normalized -= 0x40;
    if (normalized < 0 || normalized >= LEN(SQRT)) {
        WARNF("SquareRoot12(%d) out of bounds", a);
        normalized %= 192;
    }
    result_shift = (0x1F - shift) >> 1;
    return (long)(SQRT[normalized] << result_shift) >> 6;
}
