#include <psyz.h>
#include <libgpu.h>

void Psyz_PadsOnVSync(void);

void MyDrawSyncCallback(int mode) { NOT_IMPLEMENTED; }

void (*g_VsyncCallback)() = NULL;
int MyVSyncCallback(void (*f)()) { g_VsyncCallback = f; }

int Psyz_VSync(int mode);
int MyVSync(int mode) {
    // TODO the implementation is most likely incorrect
    int elapsed = (unsigned short)Psyz_VSync(mode);
    if (mode < 0) {
        // TODO return vsync, not elapsed
        return elapsed;
    }
    if (mode == 1) {
        return elapsed;
    }
    Psyz_PadsOnVSync(); // this is done on vsync by the BIOS
    if (g_VsyncCallback) {
        g_VsyncCallback();
    }
    return elapsed;
}
