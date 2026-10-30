#include <common.h>
#include <libgs.h>
#include "libgs_private.h"

int GsGetActiveBuff(void) { return PSDIDX; }

void GsSetDrawBuffOffset(void) {
    long x, y;
    if (PSDGPU) {
        GsDRAWENV.ofs[0] = POSITION.offx + PSDOFSX[PSDIDX];
        GsORGOFSY = 0;
        GsORGOFSX = 0;
        GsDRAWENV.ofs[1] = POSITION.offy + PSDOFSY[PSDIDX];
        PutDrawEnv(&GsDRAWENV);
    } else {
        x = POSITION.offx + PSDOFSX[PSDIDX == 0];
        y = POSITION.offy + PSDOFSY[PSDIDX == 0];
        SetGeomOffset(x, y);
        GsORGOFSX = x;
        GsORGOFSY = y;
    }
}
