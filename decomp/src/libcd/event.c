#include <common.h>
#include <libapi.h>
#include <libcd.h>

void def_cbsync(void);
void def_cbready(void);
void def_cbread(void);

int CdInit(void) {
    int i = 4;

    while (CdReset(1) != 1) {
        if (!i--) {
            printf("CdInit: Init failed\n");
            return 0;
        }
    }

    CdSyncCallback(def_cbsync);
    CdReadyCallback(def_cbready);
    CdReadCallback(def_cbread);
    CdReadMode(0);
    return 1;
}

void def_cbsync(void) { DeliverEvent(0xF0000003, 0x20); }

void def_cbready(void) { DeliverEvent(0xF0000003, 0x40); }

void def_cbread(void) { DeliverEvent(0xF0000003, 0x40); }
