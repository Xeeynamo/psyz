#ifdef __psyz
#include <psyz.h>
#endif
#include <libetc.h>
#include <libgpu.h>

#define WIDTH 320
#define HEIGHT 240
#define PAGE_X 2
#define PAGE_Y 7

u_long _ramsize = 0x00200000;
u_long _stacksize = 0x00004000;
static u_long pixels[WIDTH * HEIGHT * 3 / 4];
volatile unsigned int rgb24_ready;

int main(int argc, char** argv) {
    DISPENV display;
    RECT upload = {PAGE_X, PAGE_Y, WIDTH * 3 / 2, HEIGHT};
    unsigned char* rgb = (unsigned char*)pixels;
    int x, y;
    (void)argc;
    (void)argv;
    ResetGraph(0);
    SetVideoMode(MODE_NTSC);
    SetDispMask(0);
#ifdef __psyz
    Psyz_SetTitle("Sample: RGB24 XOR");
    Psyz_VideoSetAspectMode(PSYZ_ASPECT_SQUARE);
#endif
    for (y = 0; y < HEIGHT; y++) {
        for (x = 0; x < WIDTH; x++) {
            *rgb++ = x ^ y;
            *rgb++ = (x * 2) ^ y;
            *rgb++ = x ^ (y * 2);
        }
    }
    LoadImage(&upload, pixels);
    DrawSync(0);
    SetDefDispEnv(&display, PAGE_X | 1, PAGE_Y, WIDTH, HEIGHT);
    display.isrgb24 = 1;
    PutDispEnv(&display);
    SetDispMask(1);
    VSync(0);
    VSync(0);
    rgb24_ready = 0x52474224;
#ifdef __psyz
    while (!Psyz_QuitRequested()) {
#else
    while (1) {
#endif
        VSync(0);
    }
    return 0;
}
