#include <common.h>
#include <libcd.h>
#include "libcd_stream.h"

#ifdef __psyz
s32 StMode;
#endif

static inline void StCdInterrupt2(u_char intr, u_char* result) {
    StCdInterrupt();
}

int CdRead2(long mode) {
    u8 param = mode;
    CdControl(CdlSetmode, &param, NULL);
    if (mode & CdlModeStream) {
        if (mode & CdlModeSize1) {
            StMode = 0;
        } else {
            StMode = 1;
        }
        CdDataCallback(data_ready_callback);
        CdReadyCallback(StCdInterrupt2);
    }
    return CdControl(CdlReadS, NULL, NULL);
}
