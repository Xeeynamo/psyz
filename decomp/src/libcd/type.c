#include <common.h>
#include <libcd.h>
#include <libetc.h>

int CdDiskReady(int mode) {
    u_char result[8];
    int ret;
    int i;

    CdControlB(CdlNop, NULL, result);
    if (result[0] & CdlStatShellOpen)
        return CdlStatShellOpen;

    ret = CdControlB(CdlGetTN, NULL, result);
    if (mode == 1) {
        if (result[0] == CdlStatStandby && ret != 0)
            return CdlComplete;
        return CdlDiskError;
    }

    for (i = 0; i < 10; i++) {
        if (result[0] & CdlStatStandby) {
            if (result[0] == CdlStatStandby && ret != 0)
                return CdlComplete;
            do {
                VSync(30);
                ret = CdControlB(CdlGetTN, NULL, result);
            } while (result[0] != CdlStatStandby || ret == 0);
            return CdlComplete;
        }
        VSync(30);
        ret = CdControlB(CdlGetTN, NULL, result);
    }
    return CdlDiskError;
}

int CdGetDiskType(void) {
    CdlLOC pos;
    u_char buf[2048];
    u_char result[8];
    int i;
    int ready;

    CdControl(CdlNop, NULL, result);
    if (result[0] & CdlStatShellOpen)
        return CdlStatShellOpen;

    CdIntToPos(16, &pos);
    CdControl(CdlReadS, (u_char*)&pos, NULL);
    for (i = 0;;) {
        ready = CdReady(0, result);
        if (ready == 1)
            break;
        if (++i >= 10)
            break;
        CdControl(CdlReadS, (u_char*)&pos, NULL);
    }

    // ???
    if (ready == 1) {
    }

    if (ready != 1) {
        if (result[0] & CdlStatShellOpen)
            return CdlStatShellOpen;
        if (result[0] & CdlStatError) {
            if (result[1] & 0x40) {
                printf("Command Error: ");
                return CdlOtherFormat;
            }
        }
        if (result[0] & CdlStatStandby)
            return CdlOtherFormat;
        return CdlStatNoDisk;
    }

    CdControl(CdlPause, NULL, NULL);
    CdGetSector(buf, 0x200);
    if (strncmp((char*)&buf[1], "CD001", 5) != 0)
        return CdlOtherFormat;
    return CdlCdromFormat;
}
