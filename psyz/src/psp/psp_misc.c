// PSP stubs to no-op PC specific functions
#include <psyz.h>
#include <psyz/log.h>
#include <libgpu.h>
#include "../draw.h"

void Psyz_SetTitle(const char* str) { (void)str; }

void Psyz_SetWindowScale(int scale) { (void)scale; }

int Psyz_VideoSetInternalResolution(unsigned multiplier) {
    return multiplier == 1 ? 0 : -1;
}

unsigned Psyz_VideoGetInternalResolution(void) { return 1; }

int Draw_SetHorizontalGrid(
    unsigned int source_width, unsigned int target_width) {
    if (source_width == 0 || target_width == 0) {
        return -1;
    }
    return source_width == target_width ? 0 : -1;
}
