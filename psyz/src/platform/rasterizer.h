#ifndef RASTERIZER_H
#define RASTERIZER_H

#define RECT PS1_RECT
#include "../draw.h"
#undef RECT

typedef struct {
    void (*reset)(void);
    void (*texpage_mode)(ParamDrawTexpageMode* mode);
    void (*texture_window)(
        unsigned int mx, unsigned int my, unsigned int ox, unsigned int oy);
    void (*area_start)(int x, int y);
    void (*area_end)(int x, int y);
    void (*offset)(int x, int y);
    int (*horizontal_grid)(unsigned int source, unsigned int target);
    void (*mask)(int set, int preserve);
    void (*clear_image)(PS1_RECT* rect, u_char r, u_char g, u_char b);
    void (*load_image)(PS1_RECT* rect, u_long* data);
    void (*store_image)(PS1_RECT* rect, u_long* data);
    void (*move_image)(PS1_RECT* rect, unsigned int x, unsigned int y);
    void (*reset_buffer)(void);
    void (*flush_buffer)(void);
    int (*push_prim)(u_long* words, int count);
    int (*exeque_sync)(void);
} DrawBackend;

extern const DrawBackend software_rasterizer;
void SoftwareRasterizer_Reset(void);
const u16* SoftwareRasterizer_GetVram(void);

#endif
