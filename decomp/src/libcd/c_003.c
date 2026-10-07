#include <common.h>
#include <libcd.h>
#include <libapi.h>
#include "libcd_stream.h"

void StUnSetRing() {
    EnterCriticalSection();
    if (DS_active == 1) {
        DsDataCallback(NULL);
        DsReadyCallback(NULL);
    } else {
        CdDataCallback(NULL);
        CdReadyCallback(NULL);
    }
    *D_800B5410 = 0;
    *D_800B541C = 0;
    ExitCriticalSection();
}
