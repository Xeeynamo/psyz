#include <common.h>
#include <libetc.h>
#include <setjmp.h>

struct intr {
    const char* ver;
    void (*cb)();
    void (*set)(int arg0, void (*cb)(void));
    int (*start)();
    int (*stop)();
    int (*unk14)(int, void (*f)());
    int (*restart)();
    short* unk1C;
};

struct Intr {
    u16 unk0;
    u16 isCbContext;
    s16 D_800B5FFC[24];
    s32 D_800B602C;
    jmp_buf env;
    s32 stack[0x400];
}; // size=0x1068

extern char D_800B5FA0[];
extern struct Intr D_800B5FF8;
extern struct intr* D_800B7080;
volatile extern u16* D_800B7084;
volatile extern u16* D_800B7088;
volatile extern s32* D_800B708C;

void HookEntryInt(jmp_buf env);
long long startIntrDMA(); // TODO fix return type
void* startIntrVSync();
void trapIntr();
static void memclr(s32* mem, int len);

int ResetCallback(void) { return D_800B7080->start(); }

void InterruptCallback(int arg0, void (*cb)(void)) {
    D_800B7080->set(arg0, cb);
}

void DMACallback(void) { D_800B7080->cb(); }

int VSyncCallback(void (*func)()) { return D_800B7080->unk14(4, func); }

int VSyncCallbacks(int n, void (*func)()) { return D_800B7080->unk14(n, func); }

int StopCallback(void) { return D_800B7080->stop(); }

int RestartCallback(void) { return D_800B7080->restart(); }

int CheckCallback() { return D_800B5FF8.isCbContext; }

int GetIntrMask() { return *D_800B7088; }

int SetIntrMask(int mask) {
    int prev = *D_800B7088;
    *D_800B7088 = mask;
    return prev;
}

void* startIntr(void) {
    if (D_800B5FF8.unk0) {
        return NULL;
    }
    *D_800B7084 = *D_800B7088 = 0;
    *D_800B708C = 0x33333333;
    memclr(&D_800B5FF8, sizeof(D_800B5FF8) / sizeof(s32));
    if (setjmp(D_800B5FF8.env)) {
        trapIntr();
    }
    D_800B5FF8.env[JB_SP] = &D_800B5FF8.stack[0x3EC];
    HookEntryInt(D_800B5FF8.env);
    D_800B5FF8.unk0 = 1;
    D_800B7080->unk14 = startIntrVSync();
    D_800B7080->cb = startIntrDMA();
    _96_remove();
    ExitCriticalSection();
    return &D_800B5FF8.unk0;
}

const char D_800B2D70[] = "$Id: intr.c,v 1.76 1997/02/12 12:45:05 makoto Exp $";

WEAK_INCLUDE_ASM("asm/nonmatchings/libetc/intr", trapIntr);

INCLUDE_ASM("asm/nonmatchings/libetc/intr", setIntr);

u16* stopIntr(void) {
    if (D_800B5FF8.unk0) {
        EnterCriticalSection();
        D_800B5FF8.D_800B5FFC[0x17] = *D_800B7088;
        D_800B5FF8.D_800B602C = *D_800B708C;
        *D_800B7084 = *D_800B7088 = 0;
        *D_800B708C &= 0x77777777;
        ResetEntryInt();
        if (D_800B708C && D_800B708C) { // FAKE
        }
        D_800B5FF8.unk0 = 0;
        return &D_800B5FF8.unk0;
    }
    return NULL;
}

u16* restartIntr(void) {
    if (D_800B5FF8.unk0) {
        return NULL;
    }
    HookEntryInt(D_800B5FF8.env);
    D_800B5FF8.unk0 = 1;
    *D_800B7088 = D_800B5FF8.D_800B5FFC[0x17];
    *D_800B708C = D_800B5FF8.D_800B602C;
    ExitCriticalSection();
    return &D_800B5FF8.unk0;
}

static void memclr(s32* mem, int len) {
    int i;
    for (i = len - 1; i != -1; i--) {
        *mem++ = 0;
    }
}
