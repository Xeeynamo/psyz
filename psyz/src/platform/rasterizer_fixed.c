#include <psyz/video.h>

PsyzRasterizer Psyz_VideoGetRasterizer(void) { return PSYZ_RASTERIZER_GPU; }

int Psyz_VideoSetRasterizer(PsyzRasterizer rasterizer) {
    return rasterizer == PSYZ_RASTERIZER_GPU ? 0 : -1;
}
