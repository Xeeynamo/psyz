#include <common.h>
#include <libgs.h>
#include "libgs_private.h"

void GsSetDrawBuffClip(void) {
    int x = CLIP2.x + PSDOFSX[PSDIDX];
    int y = CLIP2.y + PSDOFSY[PSDIDX];
    GsDRAWENV.clip.x = x;
    GsDRAWENV.clip.y = y;
    GsDRAWENV.clip.w = CLIP2.w;
    GsDRAWENV.clip.h = CLIP2.h;
    PutDrawEnv(&GsDRAWENV);
}
