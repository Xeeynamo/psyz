#include <psyz.h>
#include <stdlib.h>
#include <string.h>

#include "ztest.h"

zimage zimage_frontbuffer(void) {
    int w = 0, h = 0;
    unsigned char* rgb = Psyz_VideoAllocCapturedFrame(&w, &h);
    zimage img = zimage_alloc(w, h, ZIMAGE_RGB888);
    if (rgb && img.data) {
        memcpy(img.data, rgb, (size_t)w * (size_t)h * 3);
    }
    free(rgb);
    return img;
}
