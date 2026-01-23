#include <common.h>
#include <libapi.h>

static int is_pad_init = 0;
static volatile int D_800B5024 = 0x1F801040; // gamepad+memcard data
static volatile int D_800B5028 = 0x1F801070; // interrupt status register

void SetInitPadFlag(int initPadFlag) { is_pad_init = initPadFlag; }

int ReadInitPadFlag(void) { return is_pad_init; }

void PAD_init2(char* bufA, char* bufB, long lenA, long lenB);
void PAD_init(char* bufA, char* bufB, long lenA, long lenB) {
    _remove_ChgclrPAD();
    EnterCriticalSection();
    _patch_pad();
    ExitCriticalSection();
    ChangeClearPAD(0);
    SetPatchPad();
    PAD_init2(bufA, bufB, lenA, lenB);
    _send_pad();
    is_pad_init = 1;
}

void InitPAD2(char* bufA, char* bufB, long lenA, long lenB);
long InitPAD(char* bufA, char* bufB, long lenA, long lenB) {
    _remove_ChgclrPAD();
    EnterCriticalSection();
    _patch_pad();
    ExitCriticalSection();
    ChangeClearPAD(0);
    SetPatchPad();
    InitPAD2(bufA, bufB, lenA, lenB);
    _send_pad();
    is_pad_init = 1;
    return is_pad_init;
}

void StartPAD(void) {
    StartPAD2();
    ChangeClearPAD(0);
    EnablePAD();
}

void StopPAD(void) {
    DisablePAD();
    StopPAD2();
    RemovePatchPad();
    is_pad_init = 0;
}

INCLUDE_ASM("asm/nonmatchings/libapi/pad", SetPatchPad);

INCLUDE_ASM("asm/nonmatchings/libapi/pad", RemovePatchPad);

INCLUDE_ASM("asm/nonmatchings/libapi/pad", _Pad1);

INCLUDE_ASM("asm/nonmatchings/libapi/pad", _IsVSync);
