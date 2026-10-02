#include <common.h>
#include <libc.h>

static unsigned int n;

int rand(void) {
    n = n * 0x41C64E6D + 0x3039;
    return (n >> 16) & 0x7FFF;
}

void srand(unsigned int seed) { n = seed; }
