#include <psyz.h>
#include <libetc.h>
#include <psyz/log.h>

void MyPadInit(int mode);
void PadInit(int mode) { MyPadInit(mode); }

u_long MyPadRead(int id);
u_long PadRead(int id) {
    u_long r = MyPadRead(id);

    // PS1 gamepad cannot have opposite D-pad pressed at the same time
    // sovles a bug on Castlevania SOTN when pressing both Left and Right
    if (r & PADLleft && r & PADLright) {
        r &= ~(PADLleft | PADLright);
    }
    if (r & PADLup && r & PADLdown) {
        r &= ~(PADLup | PADLdown);
    }

    return r;
}

void PadStop(void) { NOT_IMPLEMENTED; }

int MyVSyncCallback(void (*f)());
int VSyncCallback(void (*f)()) { return MyVSyncCallback(f); }

int VSyncCallbacks(int ch, void (*f)()) { NOT_IMPLEMENTED; }

static long video_mode = 0;
long GetVideoMode() { return video_mode; }
long SetVideoMode(long mode) {
    long prev = video_mode;
    video_mode = mode;
    return prev;
}

int SetIntrMask(int arg) {
    NOT_IMPLEMENTED;
    return 0;
}

int StopCallback(void) { NOT_IMPLEMENTED; }
int ResetCallback(void) { NOT_IMPLEMENTED; }
void* DMACallback(int dma, void (*func)()) {
    NOT_IMPLEMENTED;
    return NULL;
}
