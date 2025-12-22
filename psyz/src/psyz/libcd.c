#include <psyz.h>
#include <libcd.h>
#include <log.h>
#include <inttypes.h>
#include <stdio.h>

#define SECTOR_SIZE 2352

char* CD_comstr[] = {
    "CdlSync",    "CdlNop",
    "CdlSetloc",  "CdlPlay",
    "CdlForward", "CdlBackward",
    "CdlReadN",   "CdlStandby",
    "CdlStop",    "CdlPause",
    "CdlReset",   "CdlMute",
    "CdlDemute",  "CdlSetfilter",
    "CdlSetmode", "CdlGetparam",
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

int CD_init(void) {
    NOT_IMPLEMENTED;
    return 1;
}
void CD_initintr(void) { NOT_IMPLEMENTED; }
void CD_flush(void) { NOT_IMPLEMENTED; }
int CD_initvol(void) {
    NOT_IMPLEMENTED;
    return 0;
}
int CD_sync(int mode, u_char* result) {
    NOT_IMPLEMENTED;
    return CdlComplete;
}
int CD_ready(int mode, u_char* result) {
    NOT_IMPLEMENTED;
    return CdlComplete;
}
int CD_cw(u8 com, u8* param, u_char* result, s32 arg3) {
    CD_sync(0, 0);
    switch (com) {
    default:
        if (com >= LEN(CD_comstr)) {
            ERRORF("com %X invalid", com);
            return -1;
        }
        DEBUGF("com %s not implemented", CD_comstr[com]);
    }
    return 0;
}
int CD_vol(CdlATV* vol) { NOT_IMPLEMENTED; }
int CD_getsector(void* madr, int size) { NOT_IMPLEMENTED; }
int CD_getsector2(void) { NOT_IMPLEMENTED; }
void CD_datasync(int mode) { NOT_IMPLEMENTED; }

int CdInit(void) {
    NOT_IMPLEMENTED;
    return CD_init();
}

int CdReading() {
    NOT_IMPLEMENTED;
    return 0;
}

void ExecCd() { NOT_IMPLEMENTED; }

CdlFILE* CdSearchFile(CdlFILE* fp, char* name) {
    NOT_IMPLEMENTED;
    return NULL;
}

int CdRead(int sectors, u_long* buf, int mode) {
    NOT_IMPLEMENTED;
    return 0;
}

int CdRead2(long mode) {
    NOT_IMPLEMENTED;
    return 0;
}

int CdReadSync(int mode, u_char* result) {
    NOT_IMPLEMENTED;
    return 0;
}

u_long StGetNext(u_long** addr, u_long** header) {
    NOT_IMPLEMENTED;
    return 0;
}

void StSetRing(u_long* ring_addr, u_long ring_size) { NOT_IMPLEMENTED; }

void StSetStream(u_long mode, u_long start_frame, u_long end_frame,
                 void (*func1)(), void (*func2)()) {
    NOT_IMPLEMENTED;
}

u_long StFreeRing(u_long* base) {
    NOT_IMPLEMENTED;
    return 0;
}
