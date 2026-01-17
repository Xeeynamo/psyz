#include <psyz.h>
#include <psyz/log.h>
#include <kernel.h>
#include "libgpu.h"
#include "../draw.h"
#include "../internal.h"

#undef _get_errno // Windows: avoid conflicts
#undef undelete // macOS: avoid conflicts

int MyVSync(int mode);
int VSync(int mode) { return MyVSync(mode); }

// https://problemkaputt.de/psxspx-controllers-communication-sequence.htm
typedef enum {
    PAD_KIND_MOUSE = 0x12,
    PAD_KIND_DIGITAL_PAD = 0x41,
    PAD_KIND_ANALOG_STICK = 0x53,
    PAD_KIND_ANALOG_PAD = 0x73,
    PAD_KIND_KEYBOARD = 0x96,
    PAD_KIND_DISCONNECTED = 0xFF,
} PadKind;

long InitPAD(char* bufA, char* bufB, long lenA, long lenB) {
    if (bufA) {
        memset(bufA, 0, lenA);
        if (lenA >= 2) {
            bufA[0] = 0;
            bufA[1] = PAD_KIND_DIGITAL_PAD;
        } else {
            WARNF("bufA len too small");
        }
    } else {
        WARNF("bufA is NULL");
    }
    if (bufB) {
        memset(bufB, 0, lenB);
        if (lenB >= 2) {
            bufB[0] = 0;
            bufB[1] = PAD_KIND_DISCONNECTED;
        } else {
            WARNF("bufB len too small");
        }
    } else {
        WARNF("bufB is NULL");
    }
    return 1;
}
long StartPAD(void) {
    NOT_IMPLEMENTED;
    return 1;
}
void StopPAD(void) { NOT_IMPLEMENTED; }

void _96_remove(void) { NOT_IMPLEMENTED; }

long SetRCnt(unsigned long a, unsigned short b, long c) { NOT_IMPLEMENTED; }

long ReadInitPadFlag(void) {
    NOT_IMPLEMENTED;
    return 0;
}

void ChangeClearPAD(long a) { NOT_IMPLEMENTED; }

static unsigned long event_first_empty = 0;
static struct EvCB events[0x100] = {0};
static long GetFirstFreeEvent() {
    // event_first_empty brings the function to O(1) in an optimistic scenario,
    // but it does not guarantee it always points to an empty event
    // Search from event_first_empty to end
    for (unsigned long i = event_first_empty; i < LEN(events); i++) {
        if (!events[i].desc) {
            return (long)i;
        }
    }
    // Search from beginning to event_first_empty (wrap around)
    for (unsigned long i = 0; i < event_first_empty; i++) {
        if (!events[i].desc) {
            return (long)i;
        }
    }
    return -1;
}
long OpenEvent(unsigned long desc, long spec, long mode, long (*func)()) {
    if (!desc) {
        WARNF("invalid desc %08X", desc);
        return -1;
    }
    long id = GetFirstFreeEvent();
    if (id < 0) {
        WARNF("run out of memory");
        return -1;
    }
    struct EvCB* e = &events[id];
    e->desc = desc;
    e->spec = spec;
    e->mode = mode;
    e->FHandler = func;
    e->system[0] = 0;
    e->system[1] = 0;
    int supported = 1;
    switch (desc) {
    case SwCARD:
        switch (spec) {
        case EvSpIOE: // always report memory card as connected
            e->status = 1;
            break;
        case EvSpERROR:  // never errors
        case EvSpTIMOUT: // never report memory card as disconnected
        case EvSpNEW:    // never block writing after connection
            e->status = 0;
            break;
        default:
            supported = 0;
            break;
        }
        break;
    case HwCARD:
        switch (spec) {
        case EvSpIOE: // always report end of IO
            e->status = 1;
            break;
        case EvSpERROR:  // never errors
        case EvSpTIMOUT: // never timeout
        case EvSpNEW:    // never report a new memory card
            e->status = 0;
            break;
        default:
            supported = 0;
            break;
        }
        break;
    default:
        supported = 0;
        break;
    }
    if (!supported) {
        e->status = 0;
        WARNF("unsupported spec:%08X, desc:%04X, mode:%04X", spec, desc, mode);
    }
    event_first_empty = id + 1;
    // Wrap around if event_first_empty is beyond array bounds
    if (event_first_empty >= LEN(events)) {
        event_first_empty = 0;
    }
    return id;
}
long CloseEvent(unsigned long event) {
    if (event >= LEN(events)) {
        WARNF("invalid event ID %d", event);
        return 0;
    }
    events[event].desc = 0;
    event_first_empty = event;
    return 1;
}
long WaitEvent(unsigned long event) {
    if (event >= LEN(events)) {
        WARNF("invalid event ID %d", event);
        return 0;
    }
    // never waits
    return 1;
}
long EnableEvent(unsigned long event) {
    if (event >= LEN(events)) {
        WARNF("invalid event ID %d", event);
        return 0;
    }
    NOT_IMPLEMENTED;
    return 1;
}
long DisableEvent(unsigned long event) {
    if (event >= LEN(events)) {
        WARNF("invalid event ID %d", event);
        return 0;
    }
    NOT_IMPLEMENTED;
    return 1;
}
long TestEvent(unsigned long event) {
    if (event >= LEN(events)) {
        WARNF("invalid event ID %d", event);
        return 0;
    }
    return events[event].status;
}

void EnterCriticalSection(void) { NOT_IMPLEMENTED; }

void ExitCriticalSection(void) { NOT_IMPLEMENTED; }

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
