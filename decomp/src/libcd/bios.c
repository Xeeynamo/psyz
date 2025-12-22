#include <common.h>
#include <libcd.h>
#include <libds.h>

#ifndef CdlReset
#define CdlReset 10
#endif

typedef struct {
    s8 D_800B53D0;
    u8 D_800B53D1;
    u8 D_800B53D2;
} flush;

struct init {
    flush* unk0;
    s32* unk4;
    u8* unk8;
    u8* unkC;
    CdlLOC* unk10;
    u_long* unk14;
};

extern s32 Result__one; // ???

static u8 D_800B50E8[] = {0x50, 0x73, 0x04, 0x15, 0x9C, 0x85, 0x40};
CdlCB CD_cbsync = NULL;
CdlCB CD_cbready = NULL;
int CD_cbread = 0;
int CD_debug = 0;
int CD_status = 0;
int CD_status1 = 0;
int CD_nopen = 0;
CdlLOC CD_pos = {2, 0, 0, 0};
u_char CD_mode = 0;
u_char CD_com = 0;
static short __padding = 0;
int DS_active = 0;

char* CD_comstr[] = {
    "CdlSync",    "CdlNop",
    "CdlSetloc",  "CdlPlay",
    "CdlForward", "CdlBackward",
    "CdlReadN",   "CdlStandby",
    "CdlStop",    "CdlPause",
    "CdlReset",   "CdlMute",
    "CdlDemute",  "CdlSetfilter",
    "CdlSetmode", "?",
    "CdlGetlocL", "CdlGetlocP",
    "?",          "CdlGetTN",
    "CdlGetTD",   "CdlSeekL",
    "CdlSeekP",   "?",
    "?",          "?",
    "?",          "CdlReadS",
    "?",          "?",
    "?",          "?",
};
char* CD_intstr[] = {
    "NoIntr",  "DataReady", "Complete", "Acknowledge",
    "DataEnd", "DiskError", "?",        "?",
};
const char D_800B245C[] = "CD timeout: ";
const char D_800B246C[] = "%s:(%s) Sync=%s, Ready=%s\n";

static int D_800B51B8[] = {0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0,
                           0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0};
static int D_800B5238[] = {0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                           0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0};
static int D_800B52B8[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                           0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
static int D_800B5338[] = {0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 0,
                           0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

volatile s8* D_800B53B8 = 0x1F801800;
volatile u8* D_800B53BC = 0x1F801801;  // volume for CD(R) -> SPU (L)
volatile u8* D_800B53C0 = 0x1F801802;  // volume for CD(L) -> SPU (L)
volatile u8* D_800B53C4 = 0x1F801803;  // volume for CD(L) -> SPU (R)
volatile s32* D_800B53C8 = 0x1F801020; // volume for CD(L) -> SPU (R)
volatile u16* D_800B53CC = 0x1F801C00;
volatile flush D_800B53D0 = {0, 0, 0, 0};
static struct init D_800B53D4 = {
    &D_800B53D0, &Result__one, &CD_com, &CD_status, &CD_pos, 0x800B2518,
};
volatile u_long* D_800B53EC = (u_long*)0x1F801018;
volatile u_long* D_800B53F0 = (u_long*)0x1F8010F0;
volatile u_long* D_800B53F4 = (u_long*)0x1F8010B0;
volatile u_long* D_800B53F8 = (u_long*)0x1F8010B4;
volatile u_long* D_800B53FC = (u_long*)0x1F8010B8;

extern void(*callback);

INCLUDE_ASM("asm/nonmatchings/libcd/bios", getintr);

INCLUDE_ASM("asm/nonmatchings/libcd/bios", CD_sync);

INCLUDE_ASM("asm/nonmatchings/libcd/bios", CD_ready);

INCLUDE_ASM("asm/nonmatchings/libcd/bios", CD_cw);

int CD_vol(DslATV* vol) {
    *D_800B53B8 = 2;
    *D_800B53C0 = vol->val0;
    *D_800B53C4 = vol->val1;
    *D_800B53B8 = 3;
    *D_800B53BC = vol->val2;
    *D_800B53C0 = vol->val3;
    *D_800B53C4 = 0x20;
    return 0;
}

void CD_flush(void) {
    *D_800B53B8 = 1;
    while (*D_800B53C4 & 7) {
        *D_800B53B8 = 1;
        *D_800B53C4 = 7;
        *D_800B53C0 = 7;
    }
    D_800B53D0.D_800B53D2 = 0;
    D_800B53D0.D_800B53D1 = D_800B53D0.D_800B53D2;
    D_800B53D0.D_800B53D0 = 2;
    *D_800B53B8 = 0;
    *D_800B53C4 = 0;
    *D_800B53C8 = 0x1325;
}

int CD_initvol(void) {
    CdlATV vol;

    if (D_800B53CC[0x1B8 / 2] == 0) {
        if (D_800B53CC[0x1BA / 2] == 0) {
            D_800B53CC[0x180 / 2] = 0x3FFF;
            D_800B53CC[0x182 / 2] = 0x3FFF;
        }
    }
    D_800B53CC[0x1B0 / 2] = 0x3FFF;
    D_800B53CC[0x1B2 / 2] = 0x3FFF;
    D_800B53CC[0x1AA / 2] = 0xC001;
    vol.val2 = 128;
    vol.val0 = 128;
    vol.val3 = 0;
    vol.val1 = 0;
    *D_800B53B8 = 2;
    *D_800B53C0 = vol.val0;
    *D_800B53C4 = vol.val1;
    *D_800B53B8 = 3;
    *D_800B53BC = vol.val2;
    *D_800B53C0 = vol.val3;
    *D_800B53C4 = 0x20;
    return 0;
}

void CD_initintr(void) {
    CD_cbready = 0;
    CD_cbsync = 0;
    CD_status1 = 0;
    CD_status = 0;
    ResetCallback();
    InterruptCallback(2, &callback);
}

int CD_init(void) {
    puts("CD_init:");
    printf("addr=%08x\n", &D_800B53D4);
    CD_com = 0;
    CD_mode = 0;
    CD_cbready = 0;
    CD_cbsync = 0;
    CD_status1 = 0;
    CD_status = 0;
    ResetCallback();
    InterruptCallback(2, &callback);
    *D_800B53B8 = 1;
    while (*D_800B53C4 & 7) {
        *D_800B53B8 = 1;
        *D_800B53C4 = 7;
        *D_800B53C0 = 7;
    }
    D_800B53D0.D_800B53D2 = 0;
    D_800B53D0.D_800B53D1 = D_800B53D0.D_800B53D2;
    D_800B53D0.D_800B53D0 = 2;
    *D_800B53B8 = 0;
    *D_800B53C4 = 0;
    *D_800B53C8 = 0x1325;
    CD_cw(CdlNop, 0, 0, 0);
    if (CD_status & CdlStatShellOpen) {
        CD_cw(CdlNop, 0, 0, 0);
    }
    if (CD_cw(CdlReset, 0, 0, 0)) {
        return -1;
    }
    if (CD_cw(CdlDemute, 0, 0, 0)) {
        return -1;
    }
    if (CD_sync(0, 0) != 2) {
        return -1;
    }
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/libcd/bios", CD_datasync);

INCLUDE_ASM("asm/nonmatchings/libcd/bios", CD_getsector);

INCLUDE_ASM("asm/nonmatchings/libcd/bios", CD_getsector2);

void CD_set_test_parmnum(int arg0) { D_800B5338[25] = arg0; }

INCLUDE_ASM("asm/nonmatchings/libcd/bios", callback);
