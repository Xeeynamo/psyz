// Shared SDL3 platform code. Only use via #include.
#ifndef SDL3_COMMON_H
#define SDL3_COMMON_H

#include <SDL3/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <libetc.h>
#include <psyz.h>
#include <psyz/overlay.h>
#include <psyz/overlay_sdl3.h>
#include "../internal.h"

// HACK to avoid conflicting with RECT from windef.h
// It renames the libgpu RECT into PS1_RECT, ensuring the windef struct RECT
// does not conflict. The hack is applied to all platform for consistency.
#define RECT PS1_RECT
#include "libgpu.h"
#include "../draw.h"
#undef RECT

#define VSYNC_NTSC 59.94
#define VSYNC_PAL 50.0

// hooks the including renderer backend must implement
static void PlatformBackend_SetDriverVsync(bool enable);
static void PlatformBackend_Present(void);
static void QuitPlatform(void);

// shared window/platform state
static SDL_Window* sdl3_window = NULL;
static char window_title[0x100] = {"PSY-Z"};
static bool is_window_visible = false;
static bool is_platform_initialized = false;
static bool is_platform_init_successful = false;
static bool quit_requested = false;
static bool debug_show_vram = false;

// defaults to better support integer scaling
#define DEFAULT_FRONT_W 1280
#define DEFAULT_FRONT_H 960

typedef struct {
    int w, h;
} WndSize;

static inline u8 color_5to8(unsigned int c5) {
    return (u8)((c5 * 255 + 15) / 31);
}
static inline unsigned int color_8to5(unsigned int c8) {
    return (c8 * 31 + 127) / 255;
}
static void ConvertRgb5551ToRgba8888(const u16* src, u8* dst, size_t pixels) {
    for (size_t i = 0; i < pixels; i++) {
        u16 v = src[i];
        dst[0] = color_5to8(v & 0x1F);
        dst[1] = color_5to8((v >> 5) & 0x1F);
        dst[2] = color_5to8((v >> 10) & 0x1F);
        dst[3] = (v & 0x8000) ? 0xFF : 0x00;
        dst += 4;
    }
}
static void ConvertRgba8888ToRgb5551(const u8* src, u16* dst, size_t pixels) {
    for (size_t i = 0; i < pixels; i++) {
        u16 v = (u16)color_8to5(src[0]);
        v |= (u16)(color_8to5(src[1]) << 5);
        v |= (u16)(color_8to5(src[2]) << 10);
        v |= (src[3] >= 0x80) ? 0x8000 : 0;
        dst[i] = v;
        src += 4;
    }
}

static void* vram_convert_buf = NULL;
static size_t vram_convert_cap = 0;
static void* GetVramConvertBuffer(size_t size) {
    if (vram_convert_cap < size) {
        void* p = realloc(vram_convert_buf, size);
        if (!p) {
            ERRORF("failed to allocate %zu bytes", size);
            return NULL;
        }
        vram_convert_buf = p;
        vram_convert_cap = size;
    }
    return vram_convert_buf;
}

static bool disp_on = false;
static int set_disp_horiz = 256;
static int set_disp_vert = 240;
static int cur_disp_horiz = -1;
static int cur_disp_vert = -1;
static bool is_pal = false;
static PsyzAspectMode aspect_mode = PSYZ_ASPECT_DISPLAY;
static WndSize wnd_size_in_pixels = {0, 0};

static void SetWindowSizeInPixels(int width, int height) {
    if (!sdl3_window) {
        return;
    }
    wnd_size_in_pixels.w = width;
    wnd_size_in_pixels.h = height;
    float density = SDL_GetWindowPixelDensity(sdl3_window);
    if (density <= 0.0f) {
        density = 1.0f;
    }
    int actual_width = (int)(width / density + 0.5f);
    int actual_height = (int)(height / density + 0.5f);
    SDL_SetWindowSize(sdl3_window, actual_width, actual_height);
}

static float GetCurrentGameAspectRatio(int disp_w, int disp_h) {
    if (aspect_mode == PSYZ_ASPECT_DISPLAY) {
        float vref = is_pal ? 288.0f : 240.0f;
        return (4.0f / 3.0f) * ((float)set_disp_horiz / 256.0f) /
               ((float)set_disp_vert / vref);
    }
    if (disp_h <= 0) {
        return 4.0f / 3.0f;
    }
    return (float)disp_w / (float)disp_h;
}

// Fit a game of the given aspect ratio into the window, centering it and
// leaving black bars on the axis that does not fill.
static SDL_Rect FitGameToWindow(float game_aspect, WndSize win) {
    SDL_Rect r;
    if (game_aspect <= 0.0 || win.w <= 0 || win.h <= 0) {
        r.x = 0;
        r.y = 0;
        r.w = win.w;
        r.h = win.h;
        return r;
    }
    float window_aspect = (float)win.w / (float)win.h;
    if (window_aspect > game_aspect) {
        r.h = win.h;
        r.w = (int)(win.h * game_aspect + 0.5f);
    } else {
        r.w = win.w;
        r.h = (int)(win.w / game_aspect + 0.5f);
    }
    r.x = (win.w - r.w) / 2;
    r.y = (win.h - r.h) / 2;
    return r;
}

int Psyz_VideoSetAspectMode(PsyzAspectMode mode) {
    if (mode != PSYZ_ASPECT_DISPLAY && mode != PSYZ_ASPECT_SQUARE) {
        return -1;
    }
    aspect_mode = mode;
    return 0;
}

static Uint32 elapsed_from_beginning = 0;
static Uint32 last_vsync = 0;

// frame pacing state
static double target_frame_rate = VSYNC_NTSC;
static double target_frame_time_us = 1000000.0 / VSYNC_NTSC;
static Uint64 perf_frequency = 0;
static Uint64 last_frame_time = 0;
static Uint64 finish_time = 0;
static double drift_compensation = 0.0;
static PsyzVsyncMode vsync_mode = PSYZ_VSYNC_AUTO;
static PsyzDitherMode dither_mode = PSYZ_DITHER_AUTO;
static bool use_driver_vsync = false;
static PsyzVideoStats gpu_stats = {0};

static void PollEvents(void);

// overlay callbacks shared across backends
static PsyzOverlayEventCB_SDL3 overlay_event_cb;
PsyzOverlayEventCB_SDL3 Psyz_OverlayEvent_SDL3(PsyzOverlayEventCB_SDL3 cb) {
    const PsyzOverlayEventCB_SDL3 prev = overlay_event_cb;
    overlay_event_cb = cb;
    return prev;
}

static PsyzOverlayFrameCB overlay_frame_cb;
PsyzOverlayFrameCB Psyz_OverlayFrameCB(PsyzOverlayFrameCB cb) {
    const PsyzOverlayFrameCB prev = overlay_frame_cb;
    overlay_frame_cb = cb;
    return prev;
}

static PsyzOverlayDestroyCB overlay_destroy_cb;
PsyzOverlayDestroyCB Psyz_OverlayDestroyCB(PsyzOverlayDestroyCB cb) {
    const PsyzOverlayDestroyCB prev = overlay_destroy_cb;
    overlay_destroy_cb = cb;
    return prev;
}

void Psyz_SetTitle(const char* str) {
    strncpy(window_title, str, sizeof(window_title) - 1);
    window_title[sizeof(window_title) - 1] = 0;
    if (sdl3_window) {
        SDL_SetWindowTitle(sdl3_window, window_title);
    }
}

void Psyz_GetWindowSize(int* width, int* height) {
    SDL_GetWindowSize(sdl3_window, width, height);
}

static double GetElapsedMicroseconds(Uint64 start, Uint64 end) {
    return ((double)(end - start) * 1000000.0) / (double)perf_frequency;
}

static void ConfigureVSync(double target_fps) {
    if (vsync_mode == PSYZ_VSYNC_ON) {
        PlatformBackend_SetDriverVsync(true);
        use_driver_vsync = true;
        INFOF("vsync forced ON (driver VSync)");
        return;
    }
    if (vsync_mode == PSYZ_VSYNC_OFF) {
        PlatformBackend_SetDriverVsync(false);
        use_driver_vsync = false;
        INFOF("vsync forced OFF (frame limiter)");
        return;
    }

    SDL_DisplayID display_id = SDL_GetPrimaryDisplay();
    const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(display_id);
    double detected_framerate = 60.0;
    if (mode && mode->refresh_rate > 0.0f) {
        detected_framerate = mode->refresh_rate;
    }

    const double target_time = 1.0 / target_fps;
    const double tolerance_us = target_time * 0.05;
    bool can_use_driver_vsync =
        fabs((1.0 / detected_framerate) - target_time) < tolerance_us;

    if (can_use_driver_vsync) {
        PlatformBackend_SetDriverVsync(true);
        use_driver_vsync = true;
        INFOF("detected %.2f Hz monitor, use driver VSync", detected_framerate);
    } else {
        PlatformBackend_SetDriverVsync(false);
        use_driver_vsync = false;
        INFOF("detected %.2f Hz monitor but targeting %.2f, use frame limiter",
              detected_framerate, target_fps);
    }
}

static void UpdateTargetFramerate(double fps) {
    target_frame_rate = fps;
    target_frame_time_us = 1000000.0 / fps;
    drift_compensation = 0.0;
    last_frame_time = SDL_GetPerformanceCounter();
    ConfigureVSync(target_frame_rate);
}

// Initialize the timing/vsync state. Call once at the end of InitPlatform.
static void Sdl3Common_TimingInit(void) {
    elapsed_from_beginning = (Uint32)SDL_GetTicks();
    last_vsync = elapsed_from_beginning;
    perf_frequency = SDL_GetPerformanceFrequency();
    last_frame_time = SDL_GetPerformanceCounter();
    UpdateTargetFramerate(VSYNC_NTSC);
}

static void WaitForNextFrame(void) {
    Uint64 current_time = SDL_GetPerformanceCounter();
    double elapsed_us = GetElapsedMicroseconds(last_frame_time, current_time);

    if (!use_driver_vsync) {
        double time_to_wait_us =
            target_frame_time_us - elapsed_us + drift_compensation;

        // only wait if we're ahead of schedule (not running slow)
        if (time_to_wait_us > 100.0) { // more than 0.1ms to wait
            // High-precision mode: sleep + busy-wait
            if (time_to_wait_us > 1000.0) {
                Uint64 sleep_us = (Uint64)(time_to_wait_us - 1000.0);
                SDL_DelayNS(sleep_us * 1000);
            }

            // busy-wait for precision
            double target_elapsed = target_frame_time_us + drift_compensation;
            while (GetElapsedMicroseconds(
                       last_frame_time, SDL_GetPerformanceCounter()) <
                   target_elapsed) {
            }
        }

        // measure actual frame time and compensate drift
        Uint64 frame_end_time = SDL_GetPerformanceCounter();
        double actual_frame_time =
            GetElapsedMicroseconds(last_frame_time, frame_end_time);
        double frame_error = actual_frame_time - target_frame_time_us;

        // apply gentle correction (10% per frame)
        drift_compensation -= frame_error * 0.1;
        if (drift_compensation > 5000.0) {
            drift_compensation = 5000.0;
        }
        if (drift_compensation < -5000.0) {
            drift_compensation = -5000.0;
        }
    }

    Uint64 frame_end_time = SDL_GetPerformanceCounter();
    gpu_stats.last_frame_time_us =
        GetElapsedMicroseconds(last_frame_time, frame_end_time);
    gpu_stats.last_draw_time_us =
        GetElapsedMicroseconds(last_frame_time, finish_time);
    gpu_stats.target_frame_time_us = target_frame_time_us;
    gpu_stats.total_frames++;
    gpu_stats.using_driver_vsync = use_driver_vsync;

    last_frame_time = frame_end_time;
}

int Psyz_VideoVSync(int mode) {
    Uint32 cur;
    unsigned short ret;
    cur = (Uint32)SDL_GetTicks();
    if (mode >= 0) {
        ret = (unsigned short)(cur - last_vsync);
    } else {
        ret = (unsigned short)gpu_stats.total_frames;
    }
    last_vsync = cur;
    if (mode == 0) {
        PlatformBackend_Present();
        PollEvents();
        WaitForNextFrame();
    }
    return ret;
}

int Psyz_VideoSetVsyncMode(PsyzVsyncMode mode) {
    if (mode < 0 || mode > 2) {
        return -1;
    }
    vsync_mode = mode;
    if (sdl3_window && is_platform_init_successful) {
        ConfigureVSync(target_frame_rate);
    }
    return 0;
}

static int s_dither = 0;
static int GetCurrentDither(void) {
    if (dither_mode == PSYZ_DITHER_OFF) {
        return 0;
    }
    return s_dither;
}
static void SetDither(int dither) {
    if (dither == s_dither) {
        return;
    }
    if (GetCurrentDither() != (dither_mode == PSYZ_DITHER_OFF ? 0 : dither)) {
        Draw_FlushBuffer();
    }
    s_dither = dither;
}
int Psyz_VideoSetDitheringMode(PsyzDitherMode mode) {
    if (mode != PSYZ_DITHER_AUTO && mode != PSYZ_DITHER_OFF) {
        return -1;
    }
    if (dither_mode == mode) {
        return 0;
    }
    if (GetCurrentDither() != (mode == PSYZ_DITHER_OFF ? 0 : s_dither)) {
        Draw_FlushBuffer();
    }
    dither_mode = mode;
    return 0;
}

int Psyz_VideoStats(PsyzVideoStats* stats) {
    if (!stats || !is_platform_init_successful) {
        return -1;
    }
    *stats = gpu_stats;
    return 0;
}

struct Gamepad {
    SDL_JoystickID id;
    SDL_Gamepad* dev;
    const char* name;
};
static bool is_pads_init = false;
static struct Gamepad gamepads[16] = {0};
static void AddGamepad(SDL_JoystickID id) {
    int i;
    for (i = 0; i < LEN(gamepads); i++) {
        if (!gamepads[i].id) {
            break;
        }
    }
    if (i == LEN(gamepads)) {
        ERRORF("too many gamepads connected");
    } else {
        SDL_Gamepad* dev = SDL_OpenGamepad(id);
        if (!dev) {
            ERRORF("failed to open gamepad %d", (int)id);
            return;
        }
        gamepads[i].id = id;
        gamepads[i].dev = dev;
        gamepads[i].name = SDL_GetGamepadName(dev);
        INFOF("connected %s", gamepads[i].name);
    }
}

static void RemoveGamepad(SDL_JoystickID id) {
    int i;
    for (i = 0; i < LEN(gamepads); i++) {
        if (gamepads[i].id == id) {
            break;
        }
    }
    if (i == LEN(gamepads)) {
        WARNF("gamepad already removed");
    } else {
        SDL_CloseGamepad(gamepads[i].dev);
        gamepads[i] = (struct Gamepad){0};
    }
}

void MyPadInit(int mode) {
    if (!SDL_WasInit(SDL_INIT_GAMEPAD)) {
        if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
            ERRORF("failed to initialize SDL_INIT_GAMEPAD");
        }
    }
    is_pads_init = true;
}

static u_long keyb_p1[] = {
    SDL_SCANCODE_W,         // PAD_L2
    SDL_SCANCODE_E,         // PAD_R2
    SDL_SCANCODE_Q,         // PAD_L1
    SDL_SCANCODE_R,         // PAD_R1
    SDL_SCANCODE_S,         // PAD_TRIANGLE
    SDL_SCANCODE_D,         // PAD_CIRCLE
    SDL_SCANCODE_X,         // PAD_CROSS
    SDL_SCANCODE_Z,         // PAD_SQUARE
    SDL_SCANCODE_BACKSPACE, // PAD_SELECT
    SDL_SCANCODE_1,         // PAD_L3
    SDL_SCANCODE_2,         // PAD_R3
    SDL_SCANCODE_RETURN,    // PAD_START
    SDL_SCANCODE_UP,        // PAD_UP
    SDL_SCANCODE_RIGHT,     // PAD_RIGHT
    SDL_SCANCODE_DOWN,      // PAD_DOWN
    SDL_SCANCODE_LEFT,      // PAD_LEFT
};
static unsigned int PadRead_Keyboard(u_long* config, int config_len) {
    const bool* keyb = SDL_GetKeyboardState(NULL);
    unsigned int r = 0;
    for (int i = 0; i < config_len; i++) {
        if (keyb[config[i]]) {
            r |= 1UL << i;
        }
    }
    return r;
}
static unsigned int PadRead_Gamepad(struct Gamepad* g) {
    if (!g || !g->dev) {
        return 0;
    }
    unsigned int r = 0;
    if (SDL_GetGamepadButton(g->dev, SDL_GAMEPAD_BUTTON_NORTH)) {
        r |= PADRup;
    }
    if (SDL_GetGamepadButton(g->dev, SDL_GAMEPAD_BUTTON_SOUTH)) {
        r |= PADRdown;
    }
    if (SDL_GetGamepadButton(g->dev, SDL_GAMEPAD_BUTTON_WEST)) {
        r |= PADRleft;
    }
    if (SDL_GetGamepadButton(g->dev, SDL_GAMEPAD_BUTTON_EAST)) {
        r |= PADRright;
    }
    if (SDL_GetGamepadButton(g->dev, SDL_GAMEPAD_BUTTON_DPAD_UP)) {
        r |= PADLup;
    }
    if (SDL_GetGamepadButton(g->dev, SDL_GAMEPAD_BUTTON_DPAD_DOWN)) {
        r |= PADLdown;
    }
    if (SDL_GetGamepadButton(g->dev, SDL_GAMEPAD_BUTTON_DPAD_LEFT)) {
        r |= PADLleft;
    }
    if (SDL_GetGamepadButton(g->dev, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) {
        r |= PADLright;
    }
    if (SDL_GetGamepadButton(g->dev, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) {
        r |= PADn;
    }
    if (SDL_GetGamepadButton(g->dev, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) {
        r |= PADl;
    }
    if (SDL_GetGamepadAxis(g->dev, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 8000) {
        r |= PADo;
    }
    if (SDL_GetGamepadAxis(g->dev, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 8000) {
        r |= PADm;
    }
    if (SDL_GetGamepadButton(g->dev, SDL_GAMEPAD_BUTTON_LEFT_STICK)) {
        r |= PADi;
    }
    if (SDL_GetGamepadButton(g->dev, SDL_GAMEPAD_BUTTON_RIGHT_STICK)) {
        r |= PADj;
    }
    if (SDL_GetGamepadButton(g->dev, SDL_GAMEPAD_BUTTON_START)) {
        r |= PADh;
    }
    if (SDL_GetGamepadButton(g->dev, SDL_GAMEPAD_BUTTON_BACK)) {
        r |= PADk;
    }
    return r;
}

static unsigned int SinglePadRead(int id) {
    if (!is_pads_init) {
        WARNF("PadInit not called");
        return 0;
    }
    if (id < 0 || id >= LEN(gamepads)) {
        WARNF("invalid pad id %d", id);
        return 0;
    }

    u_long pressed = 0;
    if (id == 0) {
        pressed |= PadRead_Keyboard(keyb_p1, LEN(keyb_p1));
    }
    return pressed | PadRead_Gamepad(&gamepads[id]);
}

static unsigned char AxisToByte(short v) {
    int u = ((int)v + 32768) >> 8;
    if (u < 0) {
        u = 0;
    }
    if (u > 0xFF) {
        u = 0xFF;
    }
    return (unsigned char)u;
}

static bool PortHasHostInput(int port) {
    if (port < 0 || port >= LEN(gamepads)) {
        return false;
    }
    if (gamepads[port].dev) {
        return true;
    }
    if (port == 0) {
        // The keyboard always acts as a fallback for port 0.
        return true;
    }
    return false;
}

static void BuildPadFrame(int port, PsyzControllerKind kind, char* buf) {
    memset(buf, 0, PSYZ_PAD_BUF_LEN);
    // No physical input for this port -> report as disconnected regardless of
    // the requested kind. Games look at buf[0]=0xFF / buf[1]=0xFF to know.
    if (kind != PSYZ_CTRL_DISCONNECTED && !PortHasHostInput(port)) {
        kind = PSYZ_CTRL_DISCONNECTED;
    }
    buf[0] = 0x00;
    buf[1] = (char)kind;

    switch (kind) {
    case PSYZ_CTRL_DIGITAL_PAD:
    case PSYZ_CTRL_ANALOG_PAD:
    case PSYZ_CTRL_ANALOG_STICK: {
        unsigned int pressed = SinglePadRead(port);
        if (kind == PSYZ_CTRL_ANALOG_STICK) {
            // The analog stick (SCPH-1110) has no L3/R3; on real hardware
            // those bits always read as released.
            pressed &= ~(PADi | PADj);
        }
        buf[2] = (char)(~(pressed >> 8) & 0xFF);
        buf[3] = (char)(~pressed & 0xFF);

        if (kind == PSYZ_CTRL_DIGITAL_PAD) {
            break;
        }
        unsigned char rx = 0x80, ry = 0x80, lx = 0x80, ly = 0x80;
        if (gamepads[port].dev) {
            SDL_Gamepad* d = gamepads[port].dev;
            rx = AxisToByte(SDL_GetGamepadAxis(d, SDL_GAMEPAD_AXIS_RIGHTX));
            ry = AxisToByte(SDL_GetGamepadAxis(d, SDL_GAMEPAD_AXIS_RIGHTY));
            lx = AxisToByte(SDL_GetGamepadAxis(d, SDL_GAMEPAD_AXIS_LEFTX));
            ly = AxisToByte(SDL_GetGamepadAxis(d, SDL_GAMEPAD_AXIS_LEFTY));
        }
        buf[4] = (char)rx;
        buf[5] = (char)ry;
        buf[6] = (char)lx;
        buf[7] = (char)ly;
        break;
    }
    case PSYZ_CTRL_MOUSE:
        // TODO: read SDL mouse state into buf[3] buttons and buf[4..5] motion
        buf[2] = (char)0xFF;
        buf[3] = (char)0xFC;
        buf[4] = 0x00;
        buf[5] = 0x00;
        break;
    case PSYZ_CTRL_KEYBOARD:
        LOG_ONCE("keyboard controller kind not implemented");
        break;
    case PSYZ_CTRL_DISCONNECTED:
        memset(buf, 0xFF, PSYZ_PAD_BUF_LEN);
        break;
    default:
        WARNF("unknown controller kind 0x%02X", (unsigned)kind);
        break;
    }
}

void MyPadPoll(void) {
    char frame[PSYZ_PAD_BUF_LEN];

    PollEvents();
    for (int port = 0; port < 2; port++) {
        BuildPadFrame(
            port, Psyz_PadsSetKind(port, 0, PSYZ_CTRL_QUERY_KIND), frame);
        Psyz_PadsSet(port, frame, sizeof(frame));
    }
}

static void PollEvents(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (overlay_event_cb) {
            overlay_event_cb(&event);
        }
        switch (event.type) {
        case SDL_EVENT_GAMEPAD_ADDED:
            AddGamepad(event.gdevice.which);
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            RemoveGamepad(event.gdevice.which);
            break;
        case SDL_EVENT_QUIT:
            quit_requested = true;
            break;
        case SDL_EVENT_WINDOW_RESIZED:
            SDL_GetWindowSizeInPixels(
                sdl3_window, &wnd_size_in_pixels.w, &wnd_size_in_pixels.h);
            break;
        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
            SetWindowSizeInPixels(wnd_size_in_pixels.w, wnd_size_in_pixels.h);
            break;
        case SDL_EVENT_KEY_DOWN:
            if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                quit_requested = true;
            }
            if (event.key.scancode == SDL_SCANCODE_F6) {
                debug_show_vram ^= 1;
            }
            break;
        default:
            break;
        }
    }
    if (quit_requested) {
        QuitPlatform();
        exit(0);
    }
}

typedef struct {
    short x, y;
    unsigned short u, v, c, t;
    unsigned char r, g, b, a;
} Vertex;

#define TPAGE_NOTEXTURE 0x8000 // reuse reserved bit to flag untextured poly
#define VRGBA(p) (*(unsigned int*)(&((p).r)))
#define SET_TC(p, tpage, clut) (p)->t = (u16)(tpage), (p)->c = (u16)(clut);
#define SET_TC_ALL(p, t, c)                                                    \
    SET_TC(p, t, c)                                                            \
    SET_TC(&(p)[1], t, c) SET_TC(&(p)[2], t, c) SET_TC(&(p)[3], t, c)

#define MAX_VERTEX_COUNT 4096
#define MAX_INDEX_COUNT (MAX_VERTEX_COUNT / 4 * 6)

#define SEMITRANSP 0x02
#define TEXTURED 0x04
#define EXTRA_VERTEX 0x08
#define GOURAUD 0x10
#define TRIANGLE 0x20

static u_short cur_tpage = 0;
static Vertex vertex_buf[MAX_VERTEX_COUNT];
static unsigned short index_buf[MAX_INDEX_COUNT];
static Vertex* vertex_cur;
static unsigned short* index_cur;
static unsigned short n_vertices;
static int n_indices;

static void Draw_EnsureBufferWillNotOverflow(int vertices, int indices) {
    bool bufferFull = n_vertices + vertices > MAX_VERTEX_COUNT ||
                      n_indices + indices > MAX_INDEX_COUNT;
    if (bufferFull) {
        Draw_FlushBuffer();
    }
}
static void Draw_EnqueueBuffer(int vertices, int indices) {
    assert(n_vertices + vertices <= MAX_VERTEX_COUNT);
    assert(n_indices + indices <= MAX_INDEX_COUNT);

    vertex_cur += vertices;
    index_cur += indices;
    n_vertices += vertices;
    n_indices += indices;
}

// real hardware use XY coords as signed 11-bit
static short s11(short v) { return (short)(((v & 0x7FF) ^ 1024) - 1024); }

static int writePacket(Vertex* v, int code, int n, u_long* packet, u16* pOut) {
    int w;
    if (!n) {
        return 0;
    }
    v->x = s11(((short*)packet)[0]);
    v->y = s11(((short*)packet)[1]);
    packet++;
    n--;
    if (!n) {
        return 1;
    }
    w = 1;
    if (code & TEXTURED) {
        v->u = ((u8*)packet)[0];
        v->v = ((u8*)packet)[1];
        *pOut = ((u16*)packet)[1];
        w++;
        packet++;
        n--;
        if (!n) {
            return w;
        }
    } else {
        *pOut = 0;
    }
    if (code & GOURAUD) {
        v++;
        v->r = ((u8*)packet)[0];
        v->g = ((u8*)packet)[1];
        v->b = ((u8*)packet)[2];
        v->a = code & SEMITRANSP ? 0x80 : 0xFF;
        w++;
    }
    return w;
}

static void FixupFlipUV(Vertex* v, int hasFourVertices) {
    bool fix_u = (v[0].x > v[1].x) ^ (v[0].u > v[1].u);
    fix_u |= (v[0].x > v[2].x) ^ (v[0].u > v[2].u);
    if (fix_u) {
        v[0].u++;
        v[1].u++;
        v[2].u++;
        if (hasFourVertices) {
            v[3].u++;
        }
    }

    bool fix_v = (v[0].y > v[1].y) ^ (v[0].v > v[1].v);
    fix_v |= (v[0].y > v[2].y) ^ (v[0].v > v[2].v);
    if (fix_v) {
        v[0].v++;
        v[1].v++;
        v[2].v++;
        if (hasFourVertices) {
            v[3].v++;
        }
    }
}

static inline bool is_subtract_abr(const Vertex* v) {
    return v->a == 0x80 && (v->t & 0x60) == 0x40;
}

void Draw_SetTexpageMode(ParamDrawTexpageMode* p) {
    // implements SetDrawMode, SetDrawEnv
    unsigned short mode = *(u_short*)p;
    SetDither((mode & 0x200) ? 1 : 0);
    cur_tpage = mode & 0x1FF;
    if (p->tex_y_extra_vram) {
        DEBUGF("tex_y_extra_vram not implemented");
    }
    if (p->tex_flip_x) {
        DEBUGF("tex_flip_x not implemented");
    }
    if (p->tex_flip_y) {
        DEBUGF("tex_flip_y not implemented");
    }
}
void Draw_SetTextureWindow(unsigned int mask_x, unsigned int mask_y,
                           unsigned int off_x, unsigned int off_y) {
    // implements SetTexWindow
    // it seems it is some kind of texture clamp/repeat
    if (off_x > 0 || off_y > 0 || mask_x != 0xFFFFFFFF ||
        mask_y != 0xFFFFFFFF) {
        NOT_IMPLEMENTED;
    }
}
void Draw_SetMask(int bit0, int bit1) {
    if (bit0 || bit1) {
        NOT_IMPLEMENTED;
    }
}

#endif // SDL3_COMMON_H
