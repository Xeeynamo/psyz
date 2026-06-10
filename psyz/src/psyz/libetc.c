#include <psyz.h>
#include <libetc.h>

#include "libapi.h"

#include <psyz/log.h>

void MyPadInit(int mode);
void PadInit(int mode) { MyPadInit(mode); }

static unsigned UnpackPadBits(const char* buf) {
    if ((unsigned char)buf[0] != 0) {
        return 0; // port disconnected or frame not valid
    }
    switch ((unsigned char)buf[1]) {
    case PSYZ_CTRL_DIGITAL_PAD:
    case PSYZ_CTRL_ANALOG_STICK:
    case PSYZ_CTRL_ANALOG_PAD:
        break;
    default:
        return 0; // not a pad-like device, switches are not in bytes 2-3
    }
    unsigned int buttons0 = (unsigned char)buf[2];
    unsigned int buttons1 = (unsigned char)buf[3];
    unsigned int padq = (buttons0 << 8) | buttons1;
    return (~padq) & 0xFFFF;
}

unsigned int PadRead(int id) {
    char p0[PSYZ_PAD_BUF_LEN], p1[PSYZ_PAD_BUF_LEN];
    Psyz_PadsGet(0, p0, sizeof(p0));
    Psyz_PadsGet(1, p1, sizeof(p1));
    unsigned r = UnpackPadBits(p0) | (UnpackPadBits(p1) << 16);

    // PS1 gamepad cannot have opposite D-pad pressed at the same time
    // solves a bug on Castlevania SOTN when pressing both Left and Right
    // where its behavior is undefined.
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

static void (*intr_cb[0x100])(void) = {NULL};
void* InterruptCallback(int arg0, void (*cb)()) {
    if (arg0 < 0 || arg0 >= 0x100) {
        return NULL;
    }
    void (*prev)(void) = intr_cb[arg0];
    intr_cb[arg0] = cb;
    NOT_IMPLEMENTED;
    return prev;
}

int InitTAP(char* bufA, long lenA, char* bufB, long lenB) {
    LOG_ONCE("forwarding to InitPAD");
    return InitPAD(bufA, lenA, bufB, lenB);
}

void StartTAP(void) {
    LOG_ONCE("forwarding to StartPAD");
    StartPAD();
}

int PadGetState(int port) {
    // from PSY-Q 4.2
    NOT_IMPLEMENTED;
    return 0;
}

void PadSetAct(int port, u_char* data, int len) {
    // from PSY-Q 4.2
    NOT_IMPLEMENTED;
}

int PadSetActAlign(int port, char* data) {
    // from PSY-Q 4.2
    NOT_IMPLEMENTED;
    return 0;
}

void PadInitMtap(u_char* pad1, u_char* pad2) {
    // from PSY-Q 4.2
    NOT_IMPLEMENTED;
}

void PadStartCom(void) {
    // from PSY-Q 4.2
    NOT_IMPLEMENTED;
}
