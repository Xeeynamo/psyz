#include <psyz.h>
#include <libpress.h>
#include <log.h>

void DecDCTReset(long mode) { NOT_IMPLEMENTED; }

void DecDCTin(unsigned long* buf, long mode) { NOT_IMPLEMENTED; }

void DecDCTout(unsigned long* buf, long size) { NOT_IMPLEMENTED; }

void (*DecDCTinCallback(void (*func)()))(void) {
    NOT_IMPLEMENTED;
    return NULL;
}

void (*DecDCToutCallback(void (*func)()))(void) {
    NOT_IMPLEMENTED;
    return NULL;
}

int DecDCTvlc(u_long* bs, u_long* buf) {
    NOT_IMPLEMENTED;
    return 0;
}
