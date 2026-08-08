// PSP stubs to no-op PC specific functions
#include <psyz.h>
#include <psyz/log.h>

void Psyz_SetTitle(const char* str) { (void)str; }

void Psyz_SetWindowScale(int scale) { (void)scale; }

int Psyz_VideoSetInternalResolution(unsigned multiplier) {
    return multiplier == 1 ? 0 : -1;
}

unsigned Psyz_VideoGetInternalResolution(void) { return 1; }
