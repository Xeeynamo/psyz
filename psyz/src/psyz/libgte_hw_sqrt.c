// Hardware-accelerated implementation of SquareRoot using native C math
// functions. This provides faster execution on modern hardware compared to
// lookup tables. It might not supported on old consoles or embedded hardware.

#include <psyz.h>
#include <libgte.h>
#include <psyz/log.h>
#include <math.h>

long SquareRoot0_impl(long a) {
    if (a == 0) {
        return 0;
    }
    if (a < 0) {
        WARNF("SquareRoot0(%ld) negative value", a);
        return 0;
    }

    // Use native sqrt function
    double result = sqrt((double)a);
    return (long)result;
}

long SquareRoot12_impl(long a) {
    if (a == 0) {
        return 0;
    }
    if (a < 0) {
        WARNF("SquareRoot12(%ld) negative value", a);
        return 0;
    }

    // For .12 fixed point: result = sqrt(a) * 2^6
    // Since sqrt(a * 2^12) = sqrt(a) * 2^6
    double result = sqrt((double)a);
    return (long)(result * 64.0); // * 2^6
}
