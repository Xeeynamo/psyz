#include "psyz.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>

#include <SDL3/SDL.h>
#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__APPLE__)
#include "glad/glad.h"

#else
#include <SDL3/SDL_opengl.h>
#include <GLES3/gl3.h>

#endif

#ifdef _MSC_VER
#define ALIGNAS(n) __declspec(align(n))
#else
#include <stdalign.h>
#define ALIGNAS(n) alignas(n)
#endif

#define VSYNC_NTSC 59.94
#define VSYNC_PAL 50.0

// HACK to avoid conflicting with RECT from windef.h
// It renames the libgpu RECT into PS1_RECT, ensuring the windef struct RECT
// does not conflict. The hack is applied to all platform for consistency.
#define RECT PS1_RECT
#include "libgpu.h"
#include "../draw.h"
#undef RECT

static const char gl33_vertex_shader[] = {
    "#version 330 core\n"
    "layout(location = 0) in vec2 pos;\n"
    "layout(location = 1) in vec4 tex;\n"
    "layout(location = 2) in vec4 color;\n"
    "uniform vec2 resolution;\n"
    "uniform vec2 drawOffset;\n"
    "out vec4 vertexColor;\n"
    "out vec2 texCoord;\n"
    "flat out uint tpage;\n"
    "flat out uint clut;\n"
    // Pre-computed pixel shader parameters
    "flat out uint textureMode;\n"  // 0=untextured, 1=16-bit, 2=indexed
    "flat out float vramScaleX;\n"  // X scale for indexed modes
    "flat out uint subPixelMask;\n" // Sub-pixel mask (8-bit:1, 4-bit:3)
    "flat out uint texelShift;\n"   // Right shift for texel X
    "flat out uint indexShift;\n"   // Shift for index extraction
    "flat out uint indexMask;\n"    // Mask for color index
    "\n"
    "void main() {\n"
    "    float x = ((pos.x + drawOffset.x) / (1024.0 / 2.0)) - 1.0;\n"
    "    float y = ((pos.y + drawOffset.y) / (512.0 / 2.0)) - 1.0;\n"
    "    gl_Position = vec4(x, y, 0.0, 1.0);\n"
    // gouraud colors
    "    vertexColor = color * vec4(2, 2, 2, 1);\n"
    // select the right texture coords based on the tpage
    "    clut = uint(tex.z);\n"
    "    tpage = uint(tex.w);\n"
    "    texCoord = vec2(tex.x / 4096.0, tex.y / 512.0);\n"
    // Determine texture mode and pre-compute parameters
    "    if (tpage == 0xFFFFu) {\n"
    "        textureMode = 0u;\n" // untextured
    "    } else if ((tpage & 0x180u) >= 0x100u) {\n"
    "        textureMode = 1u;\n" // 16-bit direct
    "        texCoord.x *= 4;\n"
    "    } else {\n"
    "        textureMode = 2u;\n"            // indexed
    "        if ((tpage & 0x80u) != 0u) {\n" // 8-bit indexed
    "            texCoord.x *= 2;\n"
    "            vramScaleX = 2048.0;\n"
    "            subPixelMask = 1u;\n"
    "            texelShift = 1u;\n"
    "            indexShift = 8u;\n"
    "            indexMask = 0xFFu;\n"
    "        } else {\n" // 4-bit indexed
    "            vramScaleX = 4096.0;\n"
    "            subPixelMask = 3u;\n"
    "            texelShift = 2u;\n"
    "            indexShift = 4u;\n"
    "            indexMask = 0xFu;\n"
    "        }\n"
    "    }\n"
    "    vec2 page = vec2(\n"
    "        float((tpage % 32u) % 16u) / 16.0,\n"
    "        float((tpage % 32u) / 16u) / 2.0);\n"
    "    texCoord += page;\n"
    "}\n"};

static const char gl33_fragment_shader[] = {
    "#version 330 core\n"
    "in vec4 vertexColor;\n"
    "in vec2 texCoord;\n"
    "out vec4 FragColor;\n"
    "flat in uint clut;\n"
    "flat in uint tpage;\n"
    "flat in uint textureMode;\n"
    "flat in float vramScaleX;\n"
    "flat in uint subPixelMask;\n"
    "flat in uint texelShift;\n"
    "flat in uint indexShift;\n"
    "flat in uint indexMask;\n"
    "uniform sampler2D texVram;\n"
    "\n"
    "uint rgb5551ToU16(vec4 c) {\n"
    "    uint r = uint(c.r * 31.0 + 0.5);\n"
    "    uint g = uint(c.g * 31.0 + 0.5);\n"
    "    uint b = uint(c.b * 31.0 + 0.5);\n"
    "    uint a = uint(c.a + 0.5);\n"
    "    return r | (g << 5u) | (b << 10u) | (a << 15u);\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    vec4 texColor;\n"
    "    if (textureMode == 0u) {\n" // untextured
    "        texColor = vec4(1, 1, 1, 2);\n"
    "    } else if (textureMode == 1u) {\n" // 16-bit bitmap
    "        texColor = texture(texVram, texCoord);\n"
    "    } else {\n" // indexed
    "        vec2 vramCoord = texCoord * vec2(vramScaleX, 512.0);\n"
    "        int pixelX = int(floor(vramCoord.x));\n"
    "        uint subPixel = uint(pixelX) & subPixelMask;\n"
    "        ivec2 texelPos = ivec2(pixelX >> texelShift, int(vramCoord.y));\n"
    "        vec4 texel = texelFetch(texVram, texelPos, 0);\n"
    "        uint word16 = rgb5551ToU16(texel);\n"
    "        uint colorIdx = (word16 >> (subPixel * indexShift)) & indexMask;\n"
    "        ivec2 clutBase = ivec2((clut % 64u) * 16u, clut / 64u);\n"
    "        texColor = texelFetch(texVram, clutBase + ivec2(colorIdx, 0), "
    "0);\n"
    "    }\n"
    // check for full transparency
    "    bool colorDiscard = texColor == vec4(0, 0, 0, 0);"
    "    if (!colorDiscard) {\n"
    // check for setSemiTrans(p, 1)
    "        bool isSemiTrans = vertexColor.a < 0.75;"
    // when a color has the 0x8000 bit left then it has the semitrans flag on
    "        bool colorSemiTrans = texColor.a > 0;"
    "        if (colorSemiTrans && isSemiTrans) {\n"
    "            uint abr = (tpage & 0x60u) >> 5u;\n"
    "            if (abr == 0u) {\n"
    "                texColor.a = 1;\n" // 50% opacity
    "            } else if (abr == 1u) {\n"
    "                texColor.a = 1;\n" // TODO additive blending
    "            } else if (abr == 2u) {\n"
    "                texColor.a = 1;\n"   // TODO subtractive blending
    "            } else {\n"              // abr == 3u
    "                texColor.a = 0.5;\n" // 25% opacity?
    "            }\n"
    "        } else {\n"
    "            texColor.a = 2;\n" // full opacity
    "        }\n"
    "    } else {\n"
    "        texColor.a = 0;\n"
    "    }\n"
    "    FragColor = texColor * vertexColor;\n"
    "}\n"};

typedef struct {
    short x, y;
    unsigned short u, v, c, t;
    unsigned char r, g, b, a;
} Vertex;
typedef struct {
    GLint x, y, w, h;
} GLrecti;
typedef struct {
    GLint x, y;
} GLposi; // this is custom

#define VRGBA(p) (*(unsigned int*)(&((p).r)))
#define SET_TC(p, tpage, clut) (p)->t = (u16)tpage, (p)->c = (u16)clut;
#define SET_TC_ALL(p, t, c)                                                    \
    SET_TC(p, t, c) SET_TC(&p[1], t, c) SET_TC(&p[2], t, c) SET_TC(&p[3], t, c)

const int glVer_required_major = 3;
const int glVer_required_minor = 3;

static SDL_Window* window = NULL;
static SDL_GLContext glContext = NULL;
static int glVer_major = 0;
static int glVer_minor = 0;
static GLuint shader_program = 0;
static Uint32 elapsed_from_beginning = 0;
static Uint32 last_vsync = 0;
static u_short cur_tpage = 0;
static GLint uniform_resolution = 0;
static GLint uniform_tex_vram = 0;
static GLint uniform_draw_offset = 0;
static GLuint vram_texture;
static GLuint vram_fbo = 0;
static GLposi display_area = {0, 0};
static GLposi display_size = {256, 240};
static SDL_AudioStream* audio_stream = NULL;
static SDL_AudioDeviceID audio_device_id = {0};
static GLposi draw_offset = {0, 0};
static GLposi draw_area_start = {0, 0};
static GLposi draw_area_end = {0x10000, 0x10000};

static double target_frame_rate = VSYNC_NTSC;
static double target_frame_time_us = 1000000.0 / VSYNC_NTSC;
static Uint64 perf_frequency = 0;
static Uint64 last_frame_time = 0;
static double drift_compensation = 0.0;
static Psyz_VsyncMode vsync_mode = PSYZ_VSYNC_AUTO;
static bool use_driver_vsync = false;
static Psyz_GpuStats gpu_stats = {0};

static double GetElapsedMicroseconds(Uint64 start, Uint64 end);
static void UpdateTargetFramerate(double fps);
static void WaitForNextFrame(void);

static GLuint Init_CompileShader(const char* source, GLenum kind) {
    GLuint shader = glCreateShader(kind);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char compilerLog[512];
        glGetShaderInfoLog(shader, 512, NULL, compilerLog);
        switch (kind) {
        case GL_VERTEX_SHADER:
            ERRORF("vertex shader compilation failed:\n%s", compilerLog);
            break;
        case GL_FRAGMENT_SHADER:
            ERRORF("fragment shader compilation failed:\n%s", compilerLog);
            break;
        default:
            ERRORF("shader compilation failed:\n%s", compilerLog);
        }
    }
    return shader;
}

static GLuint Init_SetupShader() {
    GLuint vertShader =
        Init_CompileShader(gl33_vertex_shader, GL_VERTEX_SHADER);
    GLuint fragShader =
        Init_CompileShader(gl33_fragment_shader, GL_FRAGMENT_SHADER);
    GLuint program = glCreateProgram();
    glAttachShader(program, vertShader);
    glAttachShader(program, fragShader);
    glLinkProgram(program);

    int success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char compilerLog[512];
        glGetShaderInfoLog(program, 512, NULL, compilerLog);
        ERRORF("shader linking failed:\n%s", compilerLog);
    }
    glDeleteShader(vertShader);
    glDeleteShader(fragShader);
    return program;
}

static bool disp_on = false;
static int set_wnd_width = 256;
static int set_wnd_height = 240;
static int set_wnd_scale = SCREEN_SCALE;
static int cur_wnd_width = -1;
static int cur_wnd_height = -1;
static int cur_wnd_scale = -1;
static int set_disp_horiz = 256;
static int set_disp_vert = 240;
static int cur_disp_horiz = -1;
static int cur_disp_vert = -1;
static int fb_w = 0, fb_h = 0;

static void QuitPlatformAtExit(void);
static bool is_window_visible = false;
static bool is_platform_initialized = false;
static bool is_platform_init_successful = false;
bool InitPlatform() {
    if (is_platform_initialized) {
        return is_platform_init_successful;
    }
    // avoid re-initializing it continuously on failures
    is_platform_initialized = true;

    atexit(QuitPlatformAtExit);
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        ERRORF("SDL_Init: %s", SDL_GetError());
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, glVer_required_major);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, glVer_required_minor);
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
    cur_wnd_height = DISP_HEIGHT;
    window = SDL_CreateWindow(
        "PSY-Z", DISP_WIDTH * SCREEN_SCALE, DISP_HEIGHT * SCREEN_SCALE,
        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!window) {
        ERRORF("SDL_CreateWindow: %s", SDL_GetError());
        return false;
    }

    glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        ERRORF("opengl init failed: %s", SDL_GetError());
        return false;
    }
    SDL_GL_MakeCurrent(window, glContext);

#if defined(_WIN32) || defined(__APPLE__)
    if (!gladLoadGL()) {
        ERRORF("gladLoadGL failed");
        return false;
    }
#endif

    const char* glStrVersion = (const char*)glGetString(GL_VERSION);
#ifdef _MSC_VER
    sscanf_s(glStrVersion, "%d.%d", &glVer_major, &glVer_minor);
#else
    sscanf(glStrVersion, "%d.%d", &glVer_major, &glVer_minor);
#endif
    if (glVer_major < glVer_required_major ||
        (glVer_major == glVer_required_major &&
         glVer_minor < glVer_required_minor)) {
        ERRORF("opengl %d.%d not supported (%d.%d or above is required)",
               glVer_major, glVer_minor, glVer_required_major,
               glVer_required_minor);
        return false;
    }

    INFOF("opengl %d.%d initialized", glVer_major, glVer_minor);
    glLineWidth(SCREEN_SCALE);
    shader_program = Init_SetupShader();
    if (!shader_program) {
        ERRORF("failed to compile shaders: %s", SDL_GetError());
        return false;
    }
    glUseProgram(shader_program);
    uniform_resolution = glGetUniformLocation(shader_program, "resolution");
    uniform_tex_vram = glGetUniformLocation(shader_program, "texVram");
    uniform_draw_offset = glGetUniformLocation(shader_program, "drawOffset");

    glUniform1i(uniform_tex_vram, 0);
    glUniform2f(uniform_draw_offset, 0, 0);
    glGenTextures(1, &vram_texture);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, vram_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, VRAM_W, VRAM_H, 0, GL_RGBA,
                 GL_UNSIGNED_SHORT_1_5_5_5_REV, NULL);

    glGenFramebuffers(1, &vram_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, vram_fbo);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, vram_texture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        ERRORF("VRAM FBO creation failed");
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, vram_fbo);
    glViewport(0, 0, VRAM_W, VRAM_H);

    elapsed_from_beginning = (Uint32)SDL_GetTicks();
    last_vsync = elapsed_from_beginning;
    cur_tpage = 0;

    perf_frequency = SDL_GetPerformanceFrequency();
    last_frame_time = SDL_GetPerformanceCounter();
    UpdateTargetFramerate(VSYNC_NTSC);

    is_platform_init_successful = true;
    return true;
}

static void PresentBufferToScreen(void) {
    if (!window && !InitPlatform()) {
        return;
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, vram_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);

    int src_x = display_area.x;
    int src_y = display_area.y;
    int src_w = display_size.x;
    int src_h = display_size.y;
    glBlitFramebuffer(src_x, src_y + src_h, src_x + src_w, src_y, 0, 0, fb_w,
                      fb_h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    SDL_GL_SwapWindow(window);

    glBindFramebuffer(GL_FRAMEBUFFER, vram_fbo);
    glEnable(GL_SCISSOR_TEST);
}

static void QuitPlatformAtExit() {
    if (glContext) {
        SDL_GL_DestroyContext(glContext);
        glContext = NULL;
    }
}

static void QuitPlatform() {
    QuitPlatformAtExit();
    // for some reason, the functions below cannot be called by atexit,
    // or it will cause a segfault while on Wayland.
    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
        is_window_visible = false;
    }
    SDL_Quit();
    is_platform_initialized = false;
    is_platform_init_successful = false;
}

void ResetPlatform(void) {
    cur_tpage = 0;
    if (shader_program) {
        glDeleteProgram(shader_program);
        shader_program = 0;
    }
    if (vram_texture) {
        glDeleteTextures(1, &vram_texture);
        vram_texture = 0;
    }
    if (vram_fbo) {
        glDeleteFramebuffers(1, &vram_fbo);
        vram_fbo = 0;
    }
    QuitPlatform();
}

static double GetElapsedMicroseconds(Uint64 start, Uint64 end) {
    return ((double)(end - start) * 1000000.0) / (double)perf_frequency;
}

static void ConfigureVSync(double target_fps) {
    if (vsync_mode == PSYZ_VSYNC_ON) {
        SDL_GL_SetSwapInterval(1);
        use_driver_vsync = true;
        INFOF("vsync forced ON (driver VSync)");
        return;
    }
    if (vsync_mode == PSYZ_VSYNC_OFF) {
        SDL_GL_SetSwapInterval(0);
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
        SDL_GL_SetSwapInterval(1);
        use_driver_vsync = true;
        INFOF("detected %.2f Hz monitor, use driver VSync", detected_framerate);
    } else {
        SDL_GL_SetSwapInterval(0);
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
        if (drift_compensation > 5000.0)
            drift_compensation = 5000.0;
        if (drift_compensation < -5000.0)
            drift_compensation = -5000.0;
    }

    Uint64 frame_end_time = SDL_GetPerformanceCounter();
    gpu_stats.last_frame_time_us = elapsed_us;
    gpu_stats.target_frame_time_us = target_frame_time_us;
    gpu_stats.total_frames++;
    gpu_stats.using_driver_vsync = use_driver_vsync;

    last_frame_time = frame_end_time;
}

int PlatformVSync(int mode) {
    Uint32 cur;
    unsigned short ret;
    cur = (Uint32)SDL_GetTicks();
    if (mode >= 0) {
        ret = (unsigned short)(cur - last_vsync);
    } else {
        ret = (unsigned short)(cur - elapsed_from_beginning);
    }
    last_vsync = cur;
    if (mode == 0) {
        PresentBufferToScreen();
        WaitForNextFrame();
    }
    return ret;
}

void SDLAudioCallback(void* userdata, SDL_AudioStream* stream,
                      int additional_amount, int total_amount) {
    NOT_IMPLEMENTED;
}

void MySsInitHot(void) {
    SDL_AudioSpec specs = {0};
    specs.freq = 44100;
    specs.format = SDL_AUDIO_S16;
    specs.channels = 2;
    audio_stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &specs, SDLAudioCallback, NULL);
    if (!audio_stream) {
        WARNF("SDL_OpenAudioDevice failed: %s", SDL_GetError());
        return;
    }
    INFOF("SDL audio device stream opened");
    SDL_PauseAudioStreamDevice(audio_stream);
}

static void PollEvents(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            exit(0);
        case SDL_EVENT_KEY_DOWN:
            if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                exit(0);
            }
            break;
        }
    }
}

void MyPadInit(int mode) { INFOF("use keyboard"); }

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
u_long MyPadRead(int id) {
    const bool* keyb;
    int numkeys;
    PollEvents();
    keyb = SDL_GetKeyboardState(&numkeys);
    u_long pressed = 0;

    if (id == 0) {
        const int to_read =
            LEN(keyb_p1) < (size_t)numkeys ? (int)LEN(keyb_p1) : numkeys;
        for (int i = 0; i < to_read; i++) {
            if (keyb[keyb_p1[i]]) {
                pressed |= 1UL << i;
            }
        }
    }
    return pressed;
}

void Psyz_SetWindowScale(int scale) { set_wnd_scale = scale; }
void Psyz_GetWindowSize(int* width, int* height) {
    SDL_GetWindowSize(window, width, height);
}
unsigned char* Psyz_AllocAndCaptureFrame(int* w, int* h) {
    const int channels = 3;
    if (vram_fbo == 0) {
        *w = *h = 0;
        ERRORF("FBO not initialized");
        return NULL;
    }

    *w = display_size.x;
    *h = display_size.y;
    unsigned char* pixels = malloc((*w) * (*h) * channels);
    if (!pixels) {
        return NULL;
    }

    while (glGetError() != GL_NO_ERROR)
        ;

    glBindFramebuffer(GL_READ_FRAMEBUFFER, vram_fbo);

    glReadPixels(display_area.x, display_area.y, *w, *h, GL_RGB,
                 GL_UNSIGNED_BYTE, pixels);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        ERRORF("glReadPixels failed: 0x%X (w=%d, h=%d)", err, *w, *h);
        free(pixels);
        return NULL;
    }

    return pixels;
}

int Psyz_SetVsyncMode(Psyz_VsyncMode mode) {
    if (mode < 0 || mode > 2) {
        return -1;
    }
    vsync_mode = mode;
    if (window && is_platform_init_successful) {
        ConfigureVSync(target_frame_rate);
    }
    return 0;
}

int Psyz_GetGpuStats(Psyz_GpuStats* stats) {
    if (!stats || !is_platform_init_successful) {
        return -1;
    }
    *stats = gpu_stats;
    return 0;
}

static void UpdateScissor() {
    int width = draw_area_end.x - draw_area_start.x + 1;
    int height = draw_area_end.y - draw_area_start.y + 1;
    if (width <= 0 || height <= 0) {
        // Draw_SetAreaStart gets called before Draw_SetAreaEnd, it happens
        // that either width or height are zero. Just ignore the call.
        return;
    }

    if (!window || !InitPlatform()) {
        return;
    }
    Draw_FlushBuffer();

    int sx = draw_area_start.x;
    int sy = draw_area_start.y;
    int sw = width;
    int sh = height;
    glEnable(GL_SCISSOR_TEST);
    glScissor(sx, sy, sw, sh);
}
static void ApplyDisplayPendingChanges() {
#if 0
    if (!disp_on) {
        return;
    }
#endif
    if (!window && !InitPlatform()) {
        return;
    }
    if (cur_wnd_width != set_wnd_width || cur_wnd_height != set_wnd_height ||
        cur_wnd_scale != set_wnd_scale) {
        cur_wnd_width = set_wnd_width;
        cur_wnd_height = set_wnd_height;
        cur_wnd_scale = set_wnd_scale;
        fb_w = cur_wnd_width * cur_wnd_scale;
        fb_h = cur_wnd_height * cur_wnd_scale;
        SDL_SetWindowSize(window, fb_w, fb_h);

        display_size.x = cur_wnd_width;
        display_size.y = cur_wnd_height;
        glUniform2f(
            uniform_resolution, (float)cur_wnd_width, (float)cur_wnd_height);
        glBindFramebuffer(GL_FRAMEBUFFER, vram_fbo);
    }
    if (!is_window_visible) {
        SDL_ShowWindow(window);
        is_window_visible = true;
#ifdef __APPLE__
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
        }
        SDL_GL_SwapWindow(window);
#endif
    }
    if (cur_disp_horiz != set_disp_horiz || cur_disp_vert != set_disp_vert) {
        cur_disp_horiz = set_disp_horiz;
        cur_disp_vert = set_disp_vert;
        WARNF("setting screen aspect ratio not supported");
    }
}

void Draw_Reset() { NOT_IMPLEMENTED; }

void Draw_DisplayEnable(unsigned int on) {
    disp_on = on;
    if (!on) {
        glBindFramebuffer(GL_FRAMEBUFFER, vram_fbo);
        glClearColor(0, 0, 0, 1);
        glDisable(GL_SCISSOR_TEST);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_SCISSOR_TEST);
    } else {
        ApplyDisplayPendingChanges();
    }
}

void Draw_DisplayArea(unsigned int x, unsigned int y) {
    display_area.x = (GLint)x;
    display_area.y = (GLint)y;

    Draw_FlushBuffer();
    ApplyDisplayPendingChanges();
    glBindFramebuffer(GL_FRAMEBUFFER, vram_fbo);
}

void Draw_DisplayHorizontalRange(unsigned int start, unsigned int end) {
    set_disp_horiz = (int)(end - start) / 10;
    ApplyDisplayPendingChanges();
}

void Draw_DisplayVerticalRange(unsigned int start, int unsigned end) {
    set_disp_vert = (int)(end - start);
    ApplyDisplayPendingChanges();
}

void Draw_SetDisplayMode(DisplayMode* mode) {
    // TODO the interlace flag is ignored
    // TODO rgb24 is ignored, the color output will always max the color space
    if (mode->reversed) {
        WARNF("reverse mode not supported");
    }
    if (mode->horizontal_resolution_368) {
        set_wnd_width = 368;
    } else {
        switch (mode->horizontal_resolution) {
        case 0:
            set_wnd_width = 256;
            break;
        case 1:
            set_wnd_width = 320;
            break;
        case 2:
            set_wnd_width = 512;
            break;
        case 3:
            set_wnd_width = 640;
            break;
        }
    }
    set_wnd_height = mode->vertical_resolution ? 480 : 240;
    display_size.x = set_wnd_width;
    display_size.y = set_wnd_height;
    ApplyDisplayPendingChanges();

    double new_target_fps = mode->pal ? VSYNC_PAL : VSYNC_NTSC;
    if (new_target_fps != target_frame_rate) {
        UpdateTargetFramerate(new_target_fps);
    }
}

int Draw_ExequeSync() {
    PollEvents();
    return 0;
}

#define MAX_VERTEX_COUNT 4096
#define MAX_INDEX_COUNT (MAX_VERTEX_COUNT / 4 * 6)

static unsigned int VAO = -1, VBO = -1, EBO = -1;
static Vertex vertex_buf[MAX_VERTEX_COUNT];
static unsigned short index_buf[MAX_INDEX_COUNT];
static Vertex* vertex_cur;
static unsigned short* index_cur;
static unsigned short n_vertices;
static int n_indices;
static GLenum flush_mode = GL_TRIANGLES;

static void Draw_InitBuffer() {
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(
        GL_ARRAY_BUFFER, sizeof(vertex_buf), vertex_buf, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER, sizeof(index_buf), index_buf, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(
        0, 2, GL_SHORT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_SHORT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, u));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex),
                          (void*)offsetof(Vertex, r));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}
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

#define SEMITRANSP 0x02
#define TEXTURED 0x04
#define EXTRA_VERTEX 0x08
#define GOURAUD 0x10
#define TRIANGLE 0x20

static int writePacket(Vertex* v, int code, int n, u_long* packet, u16* pOut) {
    int w;
    if (!n) {
        return 0;
    }
    v->x = ((s16*)packet)[0];
    v->y = ((s16*)packet)[1];
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

int Draw_PushPrim(u_long* packets, int max_len) {
    int len = max_len;
    int code = (int)(*packets >> 24) & 0xFF;
    bool isPoly = !(code & 0x40);
    bool isLine = (code & 0x40) && !(code & 0x20);
    bool isTile = (code & 0x40) && (code & 0x20);
    bool isTextured = (code & TEXTURED) != 0;
    bool isGouraud = (code & GOURAUD) != 0;
    bool isShadeTex = !((code & 1) && isTextured && !isLine);
    u16 tpage = -1, clut = -1, pad2, pad3;
    Vertex* v;

    // to ensure we always have space, we pretend we want to allocate a quad
    Draw_EnsureBufferWillNotOverflow(4, 6);
    v = vertex_cur;
    if (isShadeTex) {
        v->r = (unsigned char)(*packets >> 0);
        v->g = (unsigned char)(*packets >> 8);
        v->b = (unsigned char)(*packets >> 16);
    } else {
        v->r = v->g = v->b = 0x80;
    }
    v->a = code & SEMITRANSP ? 0x80 : 0xFF;
    packets++;
    len--;
    if (isPoly) {
        if (code & TRIANGLE) {
            int wr, nVertices, nIndices;
            wr = writePacket(v++, code, len, packets, &clut);
            packets += wr;
            len -= wr;
            wr = writePacket(v++, code, len, packets, &tpage);
            packets += wr;
            len -= wr;
            wr = writePacket(v++, code, len, packets, &pad2);
            packets += wr;
            len -= wr;

            index_cur[0] = n_vertices + 0;
            index_cur[1] = n_vertices + 1;
            index_cur[2] = n_vertices + 2;
            if (code & EXTRA_VERTEX) {
                index_cur[3] = n_vertices + 1;
                index_cur[4] = n_vertices + 3;
                index_cur[5] = n_vertices + 2;
                nVertices = 4;
                nIndices = 6;
                wr = writePacket(v, code, len, packets, &pad3);
                packets += wr;
                len -= wr;
            } else {
                nVertices = 3;
                nIndices = 3;
            }
            // HACK last rgb are not read by writePacket, so we patch the amount
            packets--;
            len++;

            if (!isTextured) {
                clut = -1;
                tpage = -1;
            }
            if (!isGouraud || !isShadeTex) {
                VRGBA(vertex_cur[1]) = VRGBA(vertex_cur[2]) =
                    VRGBA(vertex_cur[3]) = VRGBA(vertex_cur[0]);
            }

            SET_TC_ALL(vertex_cur, tpage, clut);
            Draw_EnqueueBuffer(nVertices, nIndices);
        } else {
            // shouldn't never happen on a normal PSX application
            WARNF("code %02X not supported", code);
        }
    } else if (isLine) {
        bool padding = true;
        int nPoints = ((code >> 2) & 3) + 1;
        if (nPoints == 1) {
            padding = false;
            nPoints++; // don't ask, have faith
        }
        Draw_FlushBuffer();
        for (int i = 0; len > 0 && i < nPoints; i++) {
            vertex_cur[i].x = ((s16*)packets)[0];
            vertex_cur[i].y = ((s16*)packets)[1];
            vertex_cur[i].c = -1;
            vertex_cur[i].t = -1;
            packets++;
            len--;
            if (len > 0 && i + 1 < nPoints) {
                if (isGouraud) {
                    vertex_cur[i + 1].r = ((u8*)packets)[0];
                    vertex_cur[i + 1].g = ((u8*)packets)[1];
                    vertex_cur[i + 1].b = ((u8*)packets)[2];
                    vertex_cur[i + 1].a = code & SEMITRANSP ? 0x80 : 0xFF;
                    packets++;
                    len--;
                }
            }
        }
        if (!isGouraud) {
            VRGBA(vertex_cur[1]) = VRGBA(vertex_cur[2]) = VRGBA(vertex_cur[3]) =
                VRGBA(vertex_cur[0]);
        }
        if (padding) {
            len--;
        }
        for (int i = 0; i < nPoints - 1; i++) {
            index_cur[i * 2] = n_vertices + i;
            index_cur[i * 2 + 1] = n_vertices + i + 1;
        }
        flush_mode = GL_LINES;
        Draw_EnqueueBuffer(nPoints, (nPoints - 1) * 2);
        Draw_FlushBuffer();
        flush_mode = GL_TRIANGLES;
    } else if (isTile) {
        int x, y, w, h, tu, tv;
        x = ((s16*)packets)[0];
        y = ((s16*)packets)[1];
        packets++;
        len--;
        if (code & TEXTURED) {
            tu = ((u8*)packets)[0];
            tv = ((u8*)packets)[1];
            clut = ((s16*)packets)[1];
            tpage = cur_tpage;
            packets++;
            len--;
        } else {
            clut = -1;
            tpage = -1;
        }
        if ((code & 0x24) == 0x20) {
            // halve the brightness only for TILE
            vertex_cur[0].r >>= 1;
            vertex_cur[0].g >>= 1;
            vertex_cur[0].b >>= 1;
        }
        switch (code & ~3) {
        case 0x60: // TILE
        case 0x64: // SPRT
            w = ((s16*)packets)[0];
            h = ((s16*)packets)[1];
            packets++;
            len--;
            break;
        case 0x68: // TILE_1
        case 0x6C:
            w = 1;
            h = 1;
            break;
        case 0x70: // TILE_8
        case 0x74: // SPRT_8
            w = 8;
            h = 8;
            break;
        case 0x78: // TILE_16
        case 0x7C: // SPRT_16
            w = 16;
            h = 16;
            break;
        }
        vertex_cur[0].x = (short)(x);
        vertex_cur[0].y = (short)(y);
        vertex_cur[1].x = (short)(x + w);
        vertex_cur[1].y = (short)(y);
        vertex_cur[2].x = (short)(x);
        vertex_cur[2].y = (short)(y + h);
        vertex_cur[3].x = (short)(x + w);
        vertex_cur[3].y = (short)(y + h);
        if (code & TEXTURED) {
            vertex_cur[0].u = tu;
            vertex_cur[0].v = tv;
            vertex_cur[1].u = tu + w;
            vertex_cur[1].v = tv;
            vertex_cur[2].u = tu;
            vertex_cur[2].v = tv + h;
            vertex_cur[3].u = tu + w;
            vertex_cur[3].v = tv + h;
        }
        VRGBA(vertex_cur[1]) = VRGBA(vertex_cur[2]) = VRGBA(vertex_cur[3]) =
            VRGBA(vertex_cur[0]);
        index_cur[0] = n_vertices + 0;
        index_cur[1] = n_vertices + 1;
        index_cur[2] = n_vertices + 2;
        index_cur[3] = n_vertices + 1;
        index_cur[4] = n_vertices + 3;
        index_cur[5] = n_vertices + 2;
        SET_TC_ALL(vertex_cur, tpage, clut);
        Draw_EnqueueBuffer(4, 6);
    }
    return max_len - len;
}

void Draw_SetTexpageMode(ParamDrawTexpageMode* p) {
    // implements SetDrawMode, SetDrawEnv
    cur_tpage = *(u_short*)p & 0xFF; // TODO
    NOT_IMPLEMENTED;
}
void Draw_SetTextureWindow(unsigned int mask_x, unsigned int mask_y,
                           unsigned int off_x, unsigned int off_y) {
    // implements SetTexWindow
    // it seems it is some kind of texture clamp/repeat
    NOT_IMPLEMENTED;
}
void Draw_SetAreaStart(int x, int y) {
    draw_area_start.x = x;
    draw_area_start.y = y;
}
void Draw_SetAreaEnd(int x, int y) {
    draw_area_end.x = x;
    draw_area_end.y = y;
    UpdateScissor();
}
void Draw_SetOffset(int x, int y) {
    Draw_FlushBuffer();

    x = x % VRAM_W;
    y = y % VRAM_H;
    if (x < 0)
        x += VRAM_W;
    if (y < 0)
        y += VRAM_H;
    draw_offset.x = x;
    draw_offset.y = y;
    glUniform2f(uniform_draw_offset, (float)x, (float)y);
}
void Draw_SetMask(int bit0, int bit1) { NOT_IMPLEMENTED; }

static u16 vram_buf[VRAM_W * VRAM_H];
void Draw_ClearImage(PS1_RECT* rect, u_char r, u_char g, u_char b) {
    if (rect->w == 0 || rect->h == 0) {
        return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, vram_fbo);
    glScissor(rect->x, rect->y, rect->w, rect->h);
    glClearColor(r / 255.0f, g / 255.0f, b / 255.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    UpdateScissor();
}
void Draw_LoadImage(PS1_RECT* rect, u_long* p) {
    if (rect->w == 0 || rect->h == 0) {
        return;
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, vram_texture);
    glGetTexImage(
        GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, vram_buf);

    u16* dst = vram_buf + rect->x + rect->y * VRAM_W;
    u16* src = (u16*)p;
    for (int i = 0; i < rect->h; i++) {
        memcpy(dst, src, rect->w * sizeof(u16));
        src += rect->w;
        dst += VRAM_W;
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, VRAM_W, VRAM_H, 0, GL_RGBA,
                 GL_UNSIGNED_SHORT_1_5_5_5_REV, vram_buf);
}
void Draw_StoreImage(PS1_RECT* rect, u_long* p) {
    if (rect->w == 0 || rect->h == 0) {
        return;
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, vram_texture);
    glGetTexImage(
        GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, vram_buf);

    u16* dst = (u16*)p;
    u16* src = vram_buf + rect->x + rect->y * VRAM_W;
    for (int i = 0; i < rect->h; i++) {
        for (int j = 0; j < rect->w; j++) {
            *dst++ = src[j];
        }
        src += VRAM_W;
    }
}
void Draw_MoveImage(PS1_RECT* rect, unsigned int x, unsigned int y) {
    if (rect->x == x && rect->y == y) {
        return;
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, vram_texture);
    glGetTexImage(
        GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, vram_buf);
    int src_x = CLAMP(rect->x, 0, VRAM_W - 1);
    int src_y = CLAMP(rect->y, 0, VRAM_H - 1);
    int src_w = CLAMP(rect->w, 0, VRAM_W - src_x);
    int src_h = CLAMP(rect->h, 0, VRAM_H - src_y);
    int dst_x = CLAMP((int)x, 0, VRAM_W - 1);
    int dst_y = CLAMP((int)y, 0, VRAM_H - 1);
    int copy_w = CLAMP(src_w, 0, VRAM_W - dst_x);
    int copy_h = CLAMP(src_h, 0, VRAM_H - dst_y);

    if (src_x != rect->x || src_y != rect->y || src_w != rect->w ||
        src_h != rect->h || dst_x != (int)x || dst_y != (int)y ||
        copy_w != src_w || copy_h != src_h) {
        WARNF("params out of bounds: src=(%d,%d,%d,%d)->(%d,%d,%d,%d) "
              "dst=(%d,%d)->(%d,%d)",
              rect->x, rect->y, rect->w, rect->h, src_x, src_y, src_w, src_h,
              (int)x, (int)y, dst_x, dst_y);
    }

    if (copy_w <= 0 || copy_h <= 0) {
        WARNF("nothing to copy");
        return;
    }
    u16* src = vram_buf;
    u16* dst = vram_buf;
    if (src_y < dst_y) {
        src += src_x + (src_y + copy_h - 1) * VRAM_W;
        dst += dst_x + (dst_y + copy_h - 1) * VRAM_W;
        for (int i = 0; i < copy_h; i++) {
            memmove(dst, src, copy_w * sizeof(u16));
            src -= VRAM_W;
            dst -= VRAM_W;
        }
    } else {
        src += src_x + src_y * VRAM_W;
        dst += dst_x + dst_y * VRAM_W;
        for (int i = 0; i < copy_h; i++) {
            memmove(dst, src, copy_w * sizeof(u16));
            src += VRAM_W;
            dst += VRAM_W;
        }
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, VRAM_W, VRAM_H, 0, GL_RGBA,
                 GL_UNSIGNED_SHORT_1_5_5_5_REV, vram_buf);
}
void Draw_ResetBuffer(void) {
    n_vertices = 0;
    n_indices = 0;
    vertex_cur = vertex_buf;
    index_cur = index_buf;
}
void Draw_FlushBuffer(void) {
    if (n_vertices == 0) {
        return;
    }
    if (VBO == -1) {
        Draw_InitBuffer();
    }
    glUseProgram(shader_program);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(
        GL_ARRAY_BUFFER, 0, sizeof(Vertex) * n_vertices, vertex_buf);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferSubData(
        GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(*index_buf) * n_indices, index_buf);
    glBindVertexArray(VAO);
    glDrawElements(flush_mode, n_indices, GL_UNSIGNED_SHORT, 0);
    Draw_ResetBuffer();
}
