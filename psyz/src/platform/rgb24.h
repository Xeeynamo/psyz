#ifndef PSYZ_RGB24_H
#define PSYZ_RGB24_H

static int Rgb24RegionOverlaps(
    int x, int y, int w, int h, int rx, int ry, int rw, int rh) {
    if (w <= 0 || h <= 0 || w > VRAM_W || h > VRAM_H || x < 0 || x >= VRAM_W ||
        y >= VRAM_H || y <= -h || rw <= 0 || rh <= 0)
        return 0;
    int right = x + (w * 3 + 1) / 2;
    if (right > VRAM_W)
        right = VRAM_W;
    int top = y < 0 ? 0 : y;
    int bottom = y + h;
    if (bottom > VRAM_H)
        bottom = VRAM_H;
    return rx < right && (int64_t)rx + rw > x && ry < bottom &&
           (int64_t)ry + rh > top;
}

static unsigned char* AllocRgb24Region(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0 || w > VRAM_W || h > VRAM_H) {
        return NULL;
    }
    unsigned char* pixels = calloc((size_t)w * h, 3);
    if (!pixels || x < 0 || x >= VRAM_W || y >= VRAM_H || y <= -h) {
        return pixels;
    }
    int words = (w * 3 + 1) / 2;
    if (words > VRAM_W - x) {
        words = VRAM_W - x;
    }
    int first = y < 0 ? -y : 0;
    int last = h < VRAM_H - y ? h : VRAM_H - y;
    size_t count = (size_t)words * (last - first);
    uint16_t* raw = calloc((count + 1) & ~(size_t)1, sizeof(*raw));
    if (!raw) {
        free(pixels);
        return NULL;
    }
    PS1_RECT rect = {x, y + first, words, last - first};
    Draw_StoreImage(&rect, (u_long*)raw);
    size_t bytes = (size_t)words * 2;
    if (bytes > (size_t)w * 3) {
        bytes = (size_t)w * 3;
    }
    for (int row = first; row < last; ++row) {
        for (size_t b = 0; b < bytes; ++b) {
            pixels[((size_t)row * w * 3) + b] =
                raw[(row - first) * words + b / 2] >> ((b & 1) * 8);
        }
    }
    free(raw);
    return pixels;
}
#endif
