#include <psyz.h>
#include <psyz/log.h>
#include <kernel.h>
#include <libetc.h>
#include "libgpu.h"
#include "../draw.h"
#include "../internal.h"

#undef _get_errno // Windows: avoid conflicts
#undef undelete   // macOS: avoid conflicts

typedef struct {
    char frame[PSYZ_PAD_BUF_LEN];   // last frame published by the platform
    PsyzControllerKind channels[4]; // only channel 0 until multitap lands
} ControllerPort;

static ControllerPort ports[2] = {
    {{0},
     {
         PSYZ_CTRL_DIGITAL_PAD,
         PSYZ_CTRL_DISCONNECTED,
         PSYZ_CTRL_DISCONNECTED,
         PSYZ_CTRL_DISCONNECTED,
     }},
    {{0},
     {
         PSYZ_CTRL_DIGITAL_PAD,
         PSYZ_CTRL_DISCONNECTED,
         PSYZ_CTRL_DISCONNECTED,
         PSYZ_CTRL_DISCONNECTED,
     }},
};

static int pads_sampled = 0; // avoid more than one input polling per frame
static char* pad_buffers[2];
static int pad_buffer_lens[2];
static void ReadPadsOnVsync(void) {
    pads_sampled = 0;
    for (int p = 0; p < LEN(ports); p++) {
        if (pad_buffers[p]) {
            Psyz_PadsGet(p, pad_buffers[p], pad_buffer_lens[p]);
        }
    }
}

static PsyzVSyncCb g_PsyzVsyncCb = NULL;
PsyzVSyncCb Psyz_SetVSyncCb(PsyzVSyncCb cb) {
    PsyzVSyncCb prev = g_PsyzVsyncCb;
    g_PsyzVsyncCb = cb;
    return prev;
}

// Known limitation: VSync(n) with n > 1 paces a single frame, not n vblanks
int VSync(int mode) {
    int elapsed, n;
    Psyz_KernelPoll();
    if (mode < 0) {
        return Psyz_VideoVSync(-1);
    } else if (mode == 1) {
        return Psyz_VideoVSync(1);
    } else if (mode > 1) {
        static int logged_once = 0;
        if (!logged_once) {
            logged_once = 1;
            WARNF("VSync(n>1) is not fully implemented.");
        }
    }
    n = mode > 0 ? mode : 1;
    elapsed = Psyz_VideoVSync(0);
    ReadPadsOnVsync(); // this is done on vsync by the BIOS
    if (g_PsyzVsyncCb) {
        g_PsyzVsyncCb();
    }
    for (; n > 0; n--) {
        Psyz_KernelVBlank();
    }
    return elapsed;
}

PsyzControllerKind Psyz_PadsSetKind(
    int port, int channel, PsyzControllerKind kind) {
    if (port < 0 || port >= LEN(ports)) {
        return PSYZ_CTRL_ERROR;
    }
    if (channel < 0 || channel >= LEN(ports->channels)) {
        return PSYZ_CTRL_ERROR;
    }
    if (kind == PSYZ_CTRL_QUERY_KIND) {
        return ports[port].channels[channel];
    }
    PsyzControllerKind prev = ports[port].channels[channel];
    ports[port].channels[channel] = kind;
    return prev;
}

void Psyz_PadsSet(int port, const char* src, int len) {
    if (port < 0 || port >= LEN(ports) || !src || len <= 0) {
        return;
    }
    if (len > PSYZ_PAD_BUF_LEN) {
        len = PSYZ_PAD_BUF_LEN;
    }
    memcpy(ports[port].frame, src, len);
    pads_sampled = 1;
}

void Psyz_PadsPoll(void); // implemented by the platform layer
void Psyz_PadsGet(int port, char* dst, int len) {
    if (port < 0 || port >= LEN(ports) || !dst || len <= 0) {
        return;
    }
    if (!pads_sampled) {
        pads_sampled = 1;
        Psyz_PadsPoll();
    }
    if (len > PSYZ_PAD_BUF_LEN) {
        len = PSYZ_PAD_BUF_LEN;
    }
    memcpy(dst, ports[port].frame, len);
}

int InitPAD(char* bufA, int lenA, char* bufB, int lenB) {
    PadInit(0);
    if (bufA) {
        memset(bufA, 0, lenA);
    } else {
        WARNF("bufA is NULL");
    }
    if (bufB) {
        memset(bufB, 0, lenB);
    } else {
        WARNF("bufB is NULL");
    }
    pad_buffers[0] = bufA;
    pad_buffer_lens[0] = lenA;
    pad_buffers[1] = bufB;
    pad_buffer_lens[1] = lenB;
    return 1;
}

long StartPAD(void) {
    NOT_IMPLEMENTED;
    return 1;
}

void StopPAD(void) { NOT_IMPLEMENTED; }

int PAD_init(int type, void* unused) {
    PadInit(0);
    return 1;
}

int PAD_dr(int port, char* dst) {
    if (port < 0 || port > 1 || !dst) {
        return 0;
    }
    Psyz_PadsGet(port, dst, PSYZ_PAD_BUF_LEN);
    return PSYZ_PAD_BUF_LEN;
}

void _96_remove(void) { NOT_IMPLEMENTED; }

long ReadInitPadFlag(void) {
    NOT_IMPLEMENTED;
    return 0;
}

void ChangeClearPAD(long val) { (void)val; }

void SystemError(char c, long n) {
    NOT_IMPLEMENTED;
    ERRORF("SystemError('%c', 0x%X)", c, n);
}

struct DIRENTRY* my_firstfile(char* dirPath, struct DIRENTRY* firstEntry);
struct DIRENTRY* firstfile(char* dirPath, struct DIRENTRY* firstEntry) {
    return my_firstfile(dirPath, firstEntry);
}

struct DIRENTRY* my_nextfile(struct DIRENTRY* outEntry);
struct DIRENTRY* nextfile(struct DIRENTRY* outEntry) {
    return my_nextfile(outEntry);
}

long my_erase(char* path);
long erase(char* path) { return my_erase(path); }

long psyz_undelete(char* name) {
    NOT_IMPLEMENTED;
    return 0;
}

long my_format(char* fs);
long format(char* fs) { return my_format(fs); }

int psyz_get_errno(void) {
    NOT_IMPLEMENTED;
    return 0;
}
