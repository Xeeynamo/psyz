#include <common.h>
#include <libcd.h>

typedef struct {
    int num_sectors;
    u_long* buf;
    u_long* cur_buf;
    int mode;
    int size;
    int remaining;
    int start_time2;
    int start_time;
    int pos;
    CdlCB sync_callback;
    CdlCB ready_callback;
    CdlCB data_callback;
    int read_mode;
    u_char* result;
    int : 32;
} CD_READ_ATTR;

static CdlCB CD_cbread = NULL;
static volatile CD_READ_ATTR ReadAttr = {0};

void cb_data(void);
int cd_read_retry(int mode);

void cb_read(u_char intr, u_char* result) {
    u_long buf[3];

    ReadAttr.result = result;
    if (intr == CdlDataReady) {
        if (ReadAttr.remaining > 0) {
            if (ReadAttr.size == 0x200) {
                if (ReadAttr.read_mode & 1) {
                    CdDataCallback(NULL);
                    CdGetSector2(buf, 3);
                    CdDataSync(0);
                    CdDataCallback(cb_data);
                } else {
                    CdGetSector(buf, 3);
                }
                if (CdPosToInt((CdlLOC*)buf) != ReadAttr.pos) {
                    puts("CdRead: sector error\n");
                    ReadAttr.remaining = -1;
                }
            }
            if (ReadAttr.read_mode & 1) {
                CdGetSector2(ReadAttr.cur_buf, ReadAttr.size);
            } else {
                CdGetSector(ReadAttr.cur_buf, ReadAttr.size);
                ReadAttr.cur_buf += ReadAttr.size;
                ReadAttr.remaining--;
                ReadAttr.pos++;
            }
        }
    } else {
        ReadAttr.remaining = -1;
    }

    ReadAttr.start_time2 = VSync(-1);
    if (ReadAttr.remaining < 0) {
        cd_read_retry(1);
    }
    if (VSync(-1) > ReadAttr.start_time + 1200) {
        ReadAttr.remaining = -1;
    }
    if (ReadAttr.remaining == 0 || VSync(-1) > ReadAttr.start_time + 1200) {
        CdSyncCallback(ReadAttr.sync_callback);
        CdReadyCallback(ReadAttr.ready_callback);
        if (ReadAttr.read_mode & 1) {
            CdDataCallback(ReadAttr.data_callback);
        }
        CdControlF(CdlPause, NULL);
        if (CD_cbread) {
            CD_cbread(
                ReadAttr.remaining == 0 ? CdlComplete : CdlDiskError, result);
        }
    }
}

void cb_data(void) {
    ReadAttr.cur_buf += ReadAttr.size;
    ReadAttr.remaining--;
    ReadAttr.pos++;
    if (ReadAttr.remaining == 0) {
        CdSyncCallback(ReadAttr.sync_callback);
        CdReadyCallback(ReadAttr.ready_callback);
        if (ReadAttr.read_mode & 1) {
            CdDataCallback(ReadAttr.data_callback);
        }
        CdControlF(CdlPause, NULL);
        if (CD_cbread != NULL) {
            CD_cbread(CdlComplete, (u_char*)ReadAttr.result);
        }
    }
}

int cd_read_retry(int mode) {
    u_char param;

    CdSyncCallback(0);
    CdReadyCallback(0);
    if (ReadAttr.read_mode & 1) {
        CdDataCallback(0);
    }
    if (CdStatus() & CdlStatShellOpen) {
        if ((VSync(-1) & 0x3f) == 0) {
            puts("CdRead: Shell open...\n");
        }
        CdControlF(CdlNop, NULL);
        ReadAttr.start_time = VSync(-1);
        ReadAttr.remaining = -1;
        return ReadAttr.remaining;
    }
    if (mode) {
        puts("CdRead: retry...\n");
        CdControl(CdlPause, NULL, NULL);
        if (CdControl(CdlSetloc, (u_char*)CdLastPos(), NULL) == 0) {
            ReadAttr.remaining = -1;
            return ReadAttr.remaining;
        }
    }
    CdFlush();
    param = ReadAttr.mode;
    if (param != CdMode() || mode) {
        if (CdControl(CdlSetmode, &param, NULL) == 0) {
            ReadAttr.remaining = -1;
            return ReadAttr.remaining;
        }
    }
    ReadAttr.pos = CdPosToInt(CdLastPos());
    CdReadyCallback(cb_read);
    if (ReadAttr.read_mode & 1) {
        CdDataCallback(cb_data);
    }
    ReadAttr.cur_buf = ReadAttr.buf;
    CdControlF(CdlReadN, NULL);
    ReadAttr.remaining = ReadAttr.num_sectors;
    ReadAttr.start_time2 = VSync(-1);
    return ReadAttr.remaining;
}

void CdReadBreak(void) {
    if (ReadAttr.read_mode & 1) {
        CdDataSync(0);
    }
    ReadAttr.remaining = 0;
    CdSyncCallback(ReadAttr.sync_callback);
    CdReadyCallback(ReadAttr.ready_callback);
    if (ReadAttr.read_mode & 1) {
        CdDataCallback(ReadAttr.data_callback);
    }
    CdControlF(CdlPause, NULL);
}

int CdRead(int sectors, u_long* buf, int mode) {
    ReadAttr.mode = mode;
    switch (ReadAttr.mode & (CdlModeSize0 | CdlModeSize1)) {
    case 0:
        ReadAttr.size = 0x200;
        break;
    case CdlModeSize1:
        ReadAttr.size = 0x249;
        break;
    default:
        ReadAttr.size = 0x246;
        break;
    }
    ReadAttr.mode |= CdlModeSize1;
    ReadAttr.buf = buf;
    ReadAttr.num_sectors = sectors;
    ReadAttr.sync_callback = CdSyncCallback(NULL);
    ReadAttr.ready_callback = CdReadyCallback(NULL);
    if (ReadAttr.read_mode & 1) {
        ReadAttr.data_callback = CdDataCallback(NULL);
    }
    ReadAttr.start_time = VSync(-1);
    if (CdStatus() & (CdlStatPlay | CdlStatSeek | CdlStatRead)) {
        CdControlB(CdlPause, NULL, NULL);
    }
    return cd_read_retry(0) > 0;
}

int CdReadSync(int mode, u_char* result) {
    int ret;

    do {
        if (VSync(-1) > ReadAttr.start_time + 1200) {
            ret = -1;
        } else if (
            ReadAttr.remaining < 0 || VSync(-1) > ReadAttr.start_time2 + 60) {
            cd_read_retry(1);
            ret = ReadAttr.num_sectors;
        } else {
            ret = ReadAttr.remaining;
        }
    } while (mode == 0 && ret > 0);

    CdReady(1, result);
    return ret;
}

CdlCB CdReadCallback(CdlCB func) {
    CdlCB prev = CD_cbread;
    CD_cbread = func;
    return prev;
}

int CdReadMode(int mode) {
    int prev = ReadAttr.read_mode;
    ReadAttr.read_mode = mode;
    return prev;
}
