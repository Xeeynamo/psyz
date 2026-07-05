#include <psyz.h>
#include <psyz/overlay.h>
#include <psyz/overlay_sdl3_gl.h>
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
#include <libetc.h>
#include "../internal.h"
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

#include "sdl3_common.h"

// selected at runtime based on the active GL profile; the shader bodies are
// shared and must stay legal in both GLSL 330 core and GLSL ES 3.00 (the
// latter has no implicit int-to-float conversions)
static const char shader_prologue_core[] = {"#version 330 core\n"};
static const char shader_prologue_es[] = {
    "#version 300 es\n"
    "precision highp float;\n"
    "precision highp int;\n"
    "precision highp sampler2D;\n"};

static const char vertex_shader_body[] = {
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
    "    vertexColor = color;\n"
    // select the right texture coords based on the tpage
    "    clut = uint(tex.z);\n"
    "    uint texWord = uint(tex.w);\n"
    "    tpage = texWord & 0x1FFu;\n"
    "    texCoord = vec2(tex.x / 4096.0, tex.y / 512.0);\n"
    // Determine texture mode and pre-compute parameters
    "    if ((texWord & 0x8000u) != 0u) {\n"
    "        textureMode = 0u;\n" // untextured
    "    } else if ((tpage & 0x180u) >= 0x100u) {\n"
    "        textureMode = 1u;\n" // 16-bit direct
    "        texCoord.x *= 4.0;\n"
    "        vertexColor.rgb *= 2.0;\n"
    "    } else {\n"
    "        textureMode = 2u;\n" // indexed
    "        vertexColor.rgb *= 2.0;\n"
    "        if ((tpage & 0x80u) != 0u) {\n" // 8-bit indexed
    "            texCoord.x *= 2.0;\n"
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

static const char fragment_shader_body[] = {
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
    "uniform int dither;\n"
    "const mat4 ditherMatrix = mat4("
    "    -4.0, +0.0, -3.0, +1.0,"
    "    +2.0, -2.0, +3.0, -1.0,"
    "    -3.0, +1.0, -4.0, +0.0,"
    "    +3.0, -1.0, +2.0, -2.0);\n"
    "vec3 applyDither(vec3 c) {\n"
    "    if (dither == 0) return c;\n"
    "    int dx = int(gl_FragCoord.x) & 3;\n"
    "    int dy = int(gl_FragCoord.y) & 3;\n"
    "    float off = ditherMatrix[dy][dx];\n"
    "    vec3 c8 = floor(c * 255.0 + 0.5) + off;\n"
    "    vec3 c5 = clamp(floor(c8 / 8.0), 0.0, 31.0);\n"
    "    return c5 / 31.0;\n"
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
    "    if (texColor == vec4(0, 0, 0, 0)) {\n"
    "        FragColor = vec4(0);\n"
    "        return;\n"
    "    }\n"
    // check for setSemiTrans(p, 1)
    "    bool isSemiTrans = vertexColor.a < 0.75;"
    // when a color has the 0x8000 bit left then it has the semitrans flag on
    "    bool colorSemiTrans = texColor.a > 0.0;"
    // PS1-accurate texture-color modulation: (tex5 * col8) >> 7, clamp to 31
    "    vec3 modColor;\n"
    "    if (textureMode == 0u) {\n"
    // untextured: PS1 uses color directly, no modulation
    "        modColor = texColor.rgb * vertexColor.rgb;\n"
    "    } else {\n"
    "        vec3 tex5 = floor(texColor.rgb * 31.0 + 0.5);\n"
    "        vec3 col8 = min(floor(vertexColor.rgb * 127.5 + 0.5), "
    "vec3(255.0));\n"
    "        modColor = min(floor(tex5 * col8 / 128.0), vec3(31.0)) / "
    "31.0;\n"
    "    }\n"
    "    modColor = applyDither(modColor);\n"
    // pre-multiplied alpha output for GL_ONE, GL_ONE_MINUS_SRC_ALPHA blending
    "    if (colorSemiTrans && isSemiTrans) {\n"
    "        uint abr = (tpage & 0x60u) >> 5u;\n"
    "        if (abr == 0u) {\n"
    "            FragColor = vec4(modColor * 0.5, 0.5);\n" // 50% blend
    "        } else if (abr == 1u) {\n"
    "            FragColor = vec4(modColor, 0.0);\n" // additive
    "        } else if (abr == 2u) {\n"
    "            FragColor = vec4(modColor, 0.0);\n"        // subtractive
    "        } else {\n"                                    // abr == 3u
    "            FragColor = vec4(modColor * 0.25, 0.0);\n" // B + F/4
    "        }\n"
    "    } else {\n"
    "        FragColor = vec4(modColor, 1.0);\n" // full opacity
    "    }\n"
    "}\n"};

typedef struct {
    GLint x, y, w, h;
} GLrecti;
typedef struct {
    GLint x, y;
} GLposi; // this is custom

static int glVer_required_major = 3;
static int glVer_required_minor = 3;
static bool use_gles = false;

static SDL_GLContext glContext = NULL;
static int glVer_major = 0;
static int glVer_minor = 0;
static GLuint shader_program = 0;
static GLint uniform_resolution = 0;
static GLint uniform_tex_vram = 0;
static GLint uniform_draw_offset = 0;
static GLint uniform_dither = 0;
static GLuint vram_texture;
static GLuint vram_fbo = 0;
static GLuint scratch_texture = 0;
static GLuint scratch_fbo = 0;
static GLuint scaled_vram_texture = 0;
static GLuint scaled_vram_fbo = 0; // it's vram_fbo * internal_res
static unsigned internal_res = 1;
static unsigned set_internal_res = 1;
static GLposi display_area = {0, 0};
static GLposi display_size = {256, 240};
static GLposi cur_display_size = {-1, -1};
static GLposi draw_offset = {0, 0};
static GLposi draw_area_start = {0, 0};
static GLposi draw_area_end = {0x10000, 0x10000};

static GLuint Init_CompileShader(const char* source, GLenum kind) {
    const char* sources[2] = {
        use_gles ? shader_prologue_es : shader_prologue_core, source};
    GLuint shader = glCreateShader(kind);
    glShaderSource(shader, 2, sources, NULL);
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
        Init_CompileShader(vertex_shader_body, GL_VERTEX_SHADER);
    GLuint fragShader =
        Init_CompileShader(fragment_shader_body, GL_FRAGMENT_SHADER);
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

static PsyzOverlayInitCB_SDL3GL overlay_init_cb;
PsyzOverlayInitCB_SDL3GL Psyz_OverlayInit_SDL3GL(PsyzOverlayInitCB_SDL3GL cb) {
    const PsyzOverlayInitCB_SDL3GL prev = overlay_init_cb;
    overlay_init_cb = cb;
    return prev;
}

static void ResolveGlProfile(void) {
#if defined(__ANDROID__)
    bool es = true;
#elif defined(_WIN32)
    bool es = false;
#elif defined(__APPLE__)
    bool es = false;
#else
    bool es = false;
    const char* env = SDL_getenv("PSYZ_VIDEO_GLES");
    if (env) {
        if (!SDL_strcasecmp(env, "1")) {
            es = true;
        }
    }
#endif
    use_gles = es;
    glVer_required_major = 3;
    glVer_required_minor = use_gles ? 0 : 3;
}

bool InitPlatform() {
    if (is_platform_initialized) {
        return is_platform_init_successful;
    }
    // avoid re-initializing it continuously on failures
    is_platform_initialized = true;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        ERRORF("SDL_Init: %s", SDL_GetError());
        return false;
    }

    ResolveGlProfile();
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, glVer_required_major);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, glVer_required_minor);
    if (use_gles) {
        SDL_GL_SetAttribute(
            SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    } else {
        SDL_GL_SetAttribute(
            SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    }
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
    sdl3_window = SDL_CreateWindow(
        window_title, DEFAULT_FRONT_W, DEFAULT_FRONT_H,
        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE |
            SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!sdl3_window) {
        ERRORF("SDL_CreateWindow: %s", SDL_GetError());
        return false;
    }

    glContext = SDL_GL_CreateContext(sdl3_window);
    if (!glContext) {
        ERRORF("opengl init failed: %s", SDL_GetError());
        return false;
    }
    SDL_GL_MakeCurrent(sdl3_window, glContext);

#if defined(_WIN32) || defined(__APPLE__)
    if (!gladLoadGL()) {
        ERRORF("gladLoadGL failed");
        return false;
    }
#endif

    const char* glStrVersion = (const char*)glGetString(GL_VERSION);
    const char* glStrVersionNum = glStrVersion;
    while (*glStrVersionNum &&
           (*glStrVersionNum < '0' || *glStrVersionNum > '9')) {
        glStrVersionNum++;
    }
#ifdef _MSC_VER
    sscanf_s(glStrVersionNum, "%d.%d", &glVer_major, &glVer_minor);
#else
    sscanf(glStrVersionNum, "%d.%d", &glVer_major, &glVer_minor);
#endif
    const char* profile_name = use_gles ? "opengl es" : "opengl";
    if (glVer_major < glVer_required_major ||
        (glVer_major == glVer_required_major &&
         glVer_minor < glVer_required_minor)) {
        ERRORF("%s %d.%d not supported (%d.%d or above is required)",
               profile_name, glVer_major, glVer_minor, glVer_required_major,
               glVer_required_minor);
        return false;
    }

    INFOF("%s %d.%d initialized", profile_name, glVer_major, glVer_minor);
    INFOF("renderer: %s", (const char*)glGetString(GL_RENDERER));
    shader_program = Init_SetupShader();
    if (!shader_program) {
        ERRORF("failed to compile shaders: %s", SDL_GetError());
        return false;
    }
    glUseProgram(shader_program);
    uniform_resolution = glGetUniformLocation(shader_program, "resolution");
    uniform_tex_vram = glGetUniformLocation(shader_program, "texVram");
    uniform_draw_offset = glGetUniformLocation(shader_program, "drawOffset");
    uniform_dither = glGetUniformLocation(shader_program, "dither");

    glUniform1i(uniform_tex_vram, 0);
    glUniform2f(uniform_draw_offset, 0, 0);
    glUniform1i(uniform_dither, 0);
    glGenTextures(1, &vram_texture);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, vram_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, VRAM_W, VRAM_H, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);

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

    cur_tpage = 0;
    Sdl3Common_TimingInit();

    is_platform_init_successful = true;
    if (overlay_init_cb) {
        overlay_init_cb(sdl3_window, glContext);
    }
    return true;
}

static void PlatformBackend_SetDriverVsync(bool enable) {
    SDL_GL_SetSwapInterval(enable ? 1 : 0);
}

static void UpdateScissor(void);

static GLuint GetDrawFbo(void) {
    return internal_res <= 1 ? vram_fbo : scaled_vram_fbo;
}

static void BindDrawFbo(void) {
    glBindFramebuffer(GL_FRAMEBUFFER, GetDrawFbo());
    glViewport(0, 0, VRAM_W * internal_res, VRAM_H * internal_res);
}

// mirror a native VRAM region into the scaled draw target
static void SyncNativeVramToScaled(int x, int y, int w, int h) {
    if (internal_res <= 1 || w <= 0 || h <= 0) {
        return;
    }
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, vram_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, scaled_vram_fbo);
    glBlitFramebuffer(x, y, x + w, y + h, x * internal_res, y * internal_res,
                      (x + w) * internal_res, (y + h) * internal_res,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, scaled_vram_fbo);
    glEnable(GL_SCISSOR_TEST);
}

static void SyncScaledVramToNative(void) {
    if (internal_res <= 1) {
        return;
    }
    int x0 = CLAMP(draw_area_start.x, 0, VRAM_W);
    int y0 = CLAMP(draw_area_start.y, 0, VRAM_H);
    int x1 = CLAMP(draw_area_end.x + 1, 0, VRAM_W);
    int y1 = CLAMP(draw_area_end.y + 1, 0, VRAM_H);
    if (x1 <= x0 || y1 <= y0) {
        return;
    }
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, scaled_vram_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, vram_fbo);
    glBlitFramebuffer(
        x0 * internal_res, y0 * internal_res, x1 * internal_res,
        y1 * internal_res, x0, y0, x1, y1, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, scaled_vram_fbo);
    glEnable(GL_SCISSOR_TEST);
}

static bool CreateScaledVramFbo(int n) {
    if (scaled_vram_texture) {
        glDeleteTextures(1, &scaled_vram_texture);
        scaled_vram_texture = 0;
    }
    if (scaled_vram_fbo) {
        glDeleteFramebuffers(1, &scaled_vram_fbo);
        scaled_vram_fbo = 0;
    }
    glGenTextures(1, &scaled_vram_texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scaled_vram_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, VRAM_W * n, VRAM_H * n, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
    glGenFramebuffers(1, &scaled_vram_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, scaled_vram_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           scaled_vram_texture, 0);
    bool ok =
        glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindTexture(GL_TEXTURE_2D, vram_texture);
    if (!ok) {
        ERRORF("scaled VRAM FBO creation failed (%dx)", n);
        glDeleteTextures(1, &scaled_vram_texture);
        scaled_vram_texture = 0;
        glDeleteFramebuffers(1, &scaled_vram_fbo);
        scaled_vram_fbo = 0;
    }
    return ok;
}

// ensure scaled VRAM does not exceed max allowed texture size
static unsigned AdjustInternalRes(unsigned scale) {
    unsigned n = scale;
    GLint vram_size = VRAM_W > VRAM_H ? VRAM_W : VRAM_H;
    GLint max_tex_size = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex_size);
    while (n > 1 && (GLint)(vram_size * n) > max_tex_size) {
        n--;
    }
    if (n != scale) {
        WARNF("internal resolution %dx exceeds GL_MAX_TEXTURE_SIZE=%d, "
              "fallback to internal resolution %dx",
              scale, max_tex_size, n);
    }
    return n;
}

static void ApplyPendingInternalRes(void) {
    if (set_internal_res == internal_res) {
        return;
    }
    if (!sdl3_window || !is_platform_init_successful) {
        return;
    }
    unsigned n = AdjustInternalRes(set_internal_res);
    if (n == internal_res) {
        return;
    }
    Draw_FlushBuffer(); // render pending prims with the old multiplier
    if (n > 1 && !CreateScaledVramFbo(n)) {
        n = 1;
        set_internal_res = 1;
    }
    if (n > 1) {
        // scale native VRAM to scaled VRAM according to internal resolution
        glDisable(GL_SCISSOR_TEST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, vram_fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, scaled_vram_fbo);
        glBlitFramebuffer(0, 0, VRAM_W, VRAM_H, 0, 0, VRAM_W * n, VRAM_H * n,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glEnable(GL_SCISSOR_TEST);
    } else if (scaled_vram_fbo || scaled_vram_texture) {
        glDeleteTextures(1, &scaled_vram_texture);
        scaled_vram_texture = 0;
        glDeleteFramebuffers(1, &scaled_vram_fbo);
        scaled_vram_fbo = 0;
    }
    internal_res = n;
    BindDrawFbo();
    UpdateScissor();
    INFOF("internal resolution set to %dx (%dx%d)", n, VRAM_W * n, VRAM_H * n);
}

static void PlatformBackend_Present(void) {
    if (!sdl3_window && !InitPlatform()) {
        return;
    }
    ApplyPendingInternalRes();

    const int n = (int)internal_res;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, GetDrawFbo());
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);

    SDL_Rect src = {display_area.x, display_area.y, display_size.x,
                    display_size.y};
    float game_aspect =
        GetCurrentGameAspectRatio(display_size.x, display_size.y);
    if (debug_show_vram) {
        src = (SDL_Rect){0, 0, VRAM_W, VRAM_H};
        game_aspect = (float)VRAM_W / (float)VRAM_H;
    }

    WndSize win;
    SDL_GetWindowSizeInPixels(sdl3_window, &win.w, &win.h);
    SDL_Rect dst = FitGameToWindow(game_aspect, win);

    glViewport(0, 0, win.w, win.h);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glBlitFramebuffer(
        src.x * n, (src.y + src.h) * n, (src.x + src.w) * n, src.y * n, dst.x,
        dst.y, dst.x + dst.w, dst.y + dst.h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    if (overlay_frame_cb) {
        overlay_frame_cb();
    }
    glFinish(); // fix: black screen on Windows+Nvidia
    finish_time = SDL_GetPerformanceCounter();
    SDL_GL_SwapWindow(sdl3_window);

    BindDrawFbo();
    glBindTexture(GL_TEXTURE_2D, vram_texture);
    glViewport(0, 0, VRAM_W, VRAM_H);
    glEnable(GL_SCISSOR_TEST);
}

static void QuitPlatform(void) {
    if (overlay_destroy_cb) {
        overlay_destroy_cb();
    }
    if (glContext) {
        SDL_GL_DestroyContext(glContext);
        glContext = NULL;
    }
    if (sdl3_window) {
        SDL_DestroyWindow(sdl3_window);
        sdl3_window = NULL;
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
    if (scratch_texture) {
        glDeleteTextures(1, &scratch_texture);
        scratch_texture = 0;
    }
    if (scratch_fbo) {
        glDeleteFramebuffers(1, &scratch_fbo);
        scratch_fbo = 0;
    }
    if (scaled_vram_texture) {
        glDeleteTextures(1, &scaled_vram_texture);
        scaled_vram_texture = 0;
    }
    if (scaled_vram_fbo) {
        glDeleteFramebuffers(1, &scaled_vram_fbo);
        scaled_vram_fbo = 0;
    }
    internal_res = 1;
    free(vram_convert_buf);
    vram_convert_buf = NULL;
    vram_convert_cap = 0;
    QuitPlatform();
}

unsigned char* Psyz_VideoAllocCapturedFrame(int* w, int* h) {
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

    while (glGetError() != GL_NO_ERROR) {
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, vram_fbo);

    // GL_RGB read-back is not guaranteed on GLES, read RGBA and repack
    size_t count = (size_t)(*w) * (*h);
    u8* rgba = GetVramConvertBuffer(count * 4);
    if (!rgba) {
        free(pixels);
        return NULL;
    }
    glReadPixels(display_area.x, display_area.y, *w, *h, GL_RGBA,
                 GL_UNSIGNED_BYTE, rgba);
    for (size_t i = 0; i < count; i++) {
        pixels[i * 3 + 0] = rgba[i * 4 + 0];
        pixels[i * 3 + 1] = rgba[i * 4 + 1];
        pixels[i * 3 + 2] = rgba[i * 4 + 2];
    }

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        ERRORF("glReadPixels failed: 0x%X (w=%d, h=%d)", err, *w, *h);
        free(pixels);
        return NULL;
    }

    return pixels;
}

unsigned Psyz_VideoGetInternalResolution(void) { return internal_res; }

int Psyz_VideoSetInternalResolution(unsigned multiplier) {
    if (multiplier < 1) {
        return -1;
    }
    if (multiplier > PSYZ_INTERNAL_RES_MAX) {
        WARNF("internal resolution %dx exceeds maximum value of %dx",
              PSYZ_INTERNAL_RES_MAX);
        return -1;
    }
    set_internal_res = multiplier;
    if (sdl3_window && is_platform_init_successful) {
        ApplyPendingInternalRes();
    }
    return 0;
}

static void UpdateScissor(void) {
    int width = draw_area_end.x - draw_area_start.x + 1;
    int height = draw_area_end.y - draw_area_start.y + 1;
    if (width <= 0 || height <= 0) {
        // Draw_SetAreaStart gets called before Draw_SetAreaEnd, it happens
        // that either width or height are zero. Just ignore the call.
        return;
    }

    if (!sdl3_window || !InitPlatform()) {
        return;
    }
    Draw_FlushBuffer();

    int sx = draw_area_start.x * internal_res;
    int sy = draw_area_start.y * internal_res;
    int sw = width * internal_res;
    int sh = height * internal_res;
    glEnable(GL_SCISSOR_TEST);
    glScissor(sx, sy, sw, sh);
}
static void ApplyDisplayPendingChanges() {
    if (!sdl3_window && !InitPlatform()) {
        return;
    }
    ApplyPendingInternalRes();
    if (cur_display_size.x != display_size.x ||
        cur_display_size.y != display_size.y || !is_window_visible) {
        if (!is_window_visible) {
            SetWindowSizeInPixels(DEFAULT_FRONT_W, DEFAULT_FRONT_H);
        }

        cur_display_size = display_size;
        glUniform2f(
            uniform_resolution, (float)display_size.x, (float)display_size.y);
        BindDrawFbo();
    }
    if (!is_window_visible) {
        SDL_ShowWindow(sdl3_window);
        is_window_visible = true;
#ifdef __APPLE__
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
        }
        SDL_GL_SwapWindow(sdl3_window);
#endif
    }
    if (cur_disp_horiz != set_disp_horiz || cur_disp_vert != set_disp_vert) {
        cur_disp_horiz = set_disp_horiz;
        cur_disp_vert = set_disp_vert;
    }
}

void Draw_Reset() { NOT_IMPLEMENTED; }

void Draw_DisplayEnable(unsigned int on) {
    disp_on = on;
    if (!on) {
        if (!sdl3_window && !InitPlatform()) {
            return;
        }
        // when display is on, clear background in black
        glClearColor(0, 0, 0, 1);
        glDisable(GL_SCISSOR_TEST);
        glBindFramebuffer(GL_FRAMEBUFFER, vram_fbo);
        glClear(GL_COLOR_BUFFER_BIT);
        if (internal_res > 1) {
            glBindFramebuffer(GL_FRAMEBUFFER, scaled_vram_fbo);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        BindDrawFbo();
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
    BindDrawFbo();
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
    is_pal = mode->pal;
    if (mode->horizontal_resolution_368) {
        display_size.x = 368;
    } else {
        switch (mode->horizontal_resolution) {
        case 0:
            display_size.x = 256;
            break;
        case 1:
            display_size.x = 320;
            break;
        case 2:
            display_size.x = 512;
            break;
        case 3:
            display_size.x = 640;
            break;
        default:
            break;
        }
    }
    display_size.y = mode->vertical_resolution ? 480 : 240;
    ApplyDisplayPendingChanges();

    double new_target_fps = mode->pal ? VSYNC_PAL : VSYNC_NTSC;
    if (new_target_fps != target_frame_rate) {
        UpdateTargetFramerate(new_target_fps);
    }
}

int Draw_ExequeSync() { return 0; }

static unsigned int VAO = -1, VBO = -1, EBO = -1;

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
            if (isGouraud) {
                packets--;
                len++;
            }

            if (isTextured) {
                FixupFlipUV(vertex_cur, code & EXTRA_VERTEX);
            } else {
                clut = -1;
                tpage = cur_tpage | TPAGE_NOTEXTURE;
            }
            if (!isGouraud || !isShadeTex) {
                VRGBA(vertex_cur[1]) = VRGBA(vertex_cur[2]) =
                    VRGBA(vertex_cur[3]) = VRGBA(vertex_cur[0]);
            }

            SET_TC_ALL(vertex_cur, tpage, clut);
            Draw_EnqueueBuffer(nVertices, nIndices);
        } else {
            // shouldn't happen on a normal PSX application
            WARNF("code %02X not supported", code);
        }
    } else if (isLine) {
        bool padding = true;
        int nPoints = ((code >> 2) & 3) + 1;
        if (nPoints == 1) {
            padding = false;
            nPoints++; // don't ask, have faith
        }

        // accumulate line points first, then convert them to triangles
        short px[4], py[4];
        unsigned char cr[4], cg[4], cb[4], ca[4];
        int parsed = 0;
        cr[0] = v->r;
        cg[0] = v->g;
        cb[0] = v->b;
        ca[0] = v->a;
        for (int i = 0; len > 0 && i < nPoints; i++) {
            px[i] = s11(((short*)packets)[0]);
            py[i] = s11(((short*)packets)[1]);
            packets++;
            len--;
            parsed = i + 1;
            if (len > 0 && i + 1 < nPoints && isGouraud) {
                cr[i + 1] = ((u8*)packets)[0];
                cg[i + 1] = ((u8*)packets)[1];
                cb[i + 1] = ((u8*)packets)[2];
                ca[i + 1] = code & SEMITRANSP ? 0x80 : 0xFF;
                packets++;
                len--;
            }
        }
        if (!isGouraud) {
            for (int i = 1; i < parsed; i++) {
                cr[i] = cr[0];
                cg[i] = cg[0];
                cb[i] = cb[0];
                ca[i] = ca[0];
            }
        }
        if (padding) {
            len--;
        }

        int nSegments = parsed - 1;
        if (nSegments >= 1) {
            // for LINE_*4: 12 verts, 18 indices
            Draw_EnsureBufferWillNotOverflow(nSegments * 4, nSegments * 6);
            for (int s = 0; s < nSegments; s++) {
                short x0 = px[s];
                short y0 = py[s];
                short x1 = px[s + 1];
                short y1 = py[s + 1];
                int dx = x1 - x0;
                int dy = y1 - y0;

                // decides to thicken horizontally or vertically
                short ox, oy, ex, ey;
                if ((dx < 0 ? -dx : dx) >= (dy < 0 ? -dy : dy)) {
                    ox = 0;
                    oy = 1;
                    ex = dx >= 0 ? 1 : -1;
                    ey = 0;
                } else {
                    ox = 1;
                    oy = 0;
                    ex = 0;
                    ey = dy >= 0 ? 1 : -1;
                }

                Vertex* q = vertex_cur;
                unsigned short base = n_vertices;
                q[0].x = x0;
                q[0].y = y0;
                q[1].x = (short)(x1 + ex);
                q[1].y = (short)(y1 + ey);
                q[2].x = (short)(x1 + ex + ox);
                q[2].y = (short)(y1 + ey + oy);
                q[3].x = (short)(x0 + ox);
                q[3].y = (short)(y0 + oy);
                q[0].r = cr[s];
                q[0].g = cg[s];
                q[0].b = cb[s];
                q[0].a = ca[s];
                q[3].r = cr[s];
                q[3].g = cg[s];
                q[3].b = cb[s];
                q[3].a = ca[s];
                q[1].r = cr[s + 1];
                q[1].g = cg[s + 1];
                q[1].b = cb[s + 1];
                q[1].a = ca[s + 1];
                q[2].r = cr[s + 1];
                q[2].g = cg[s + 1];
                q[2].b = cb[s + 1];
                q[2].a = ca[s + 1];
                for (int k = 0; k < 4; k++) {
                    q[k].c = -1;
                    q[k].t = cur_tpage | TPAGE_NOTEXTURE;
                }
                index_cur[0] = base + 0;
                index_cur[1] = base + 1;
                index_cur[2] = base + 2;
                index_cur[3] = base + 0;
                index_cur[4] = base + 2;
                index_cur[5] = base + 3;
                Draw_EnqueueBuffer(4, 6);
            }
        }
    } else if (isTile) {
        int x, y, w, h, tu, tv;
        x = s11(((short*)packets)[0]);
        y = s11(((short*)packets)[1]);
        packets++;
        len--;
        if (isTextured) {
            tu = ((u8*)packets)[0];
            tv = ((u8*)packets)[1];
            clut = ((s16*)packets)[1];
            tpage = cur_tpage;
            packets++;
            len--;
        } else {
            clut = -1;
            tpage = cur_tpage | TPAGE_NOTEXTURE;
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
        default:
            // TODO warn about unrecognized code
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
        if (isTextured) {
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
    if (x < 0) {
        x += VRAM_W;
    }
    if (y < 0) {
        y += VRAM_H;
    }
    draw_offset.x = x;
    draw_offset.y = y;
    glUniform2f(uniform_draw_offset, (float)x, (float)y);
}

void Draw_ClearImage(PS1_RECT* rect, u_char r, u_char g, u_char b) {
    if (rect->w == 0 || rect->h == 0) {
        return;
    }
    glClearColor((float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, 0.0f);
    glBindFramebuffer(GL_FRAMEBUFFER, vram_fbo);
    glScissor(rect->x, rect->y, rect->w, rect->h);
    glClear(GL_COLOR_BUFFER_BIT);
    if (internal_res > 1) {
        glBindFramebuffer(GL_FRAMEBUFFER, scaled_vram_fbo);
        glScissor(rect->x * internal_res, rect->y * internal_res,
                  rect->w * internal_res, rect->h * internal_res);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    BindDrawFbo();
    UpdateScissor();
}
void Draw_LoadImage(PS1_RECT* rect, u_long* p) {
    if (rect->w == 0 || rect->h == 0) {
        return;
    }
    size_t count = (size_t)rect->w * rect->h;
    u8* buf = GetVramConvertBuffer(count * 4);
    if (!buf) {
        return;
    }
    ConvertRgb5551ToRgba8888((const u16*)p, buf, count);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, vram_texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexSubImage2D(GL_TEXTURE_2D, 0, rect->x, rect->y, rect->w, rect->h,
                    GL_RGBA, GL_UNSIGNED_BYTE, buf);
    SyncNativeVramToScaled(rect->x, rect->y, rect->w, rect->h);
}
void Draw_StoreImage(PS1_RECT* rect, u_long* p) {
    if (rect->w == 0 || rect->h == 0) {
        return;
    }
    size_t count = (size_t)rect->w * rect->h;
    u8* buf = GetVramConvertBuffer(count * 4);
    if (!buf) {
        return;
    }
    Draw_FlushBuffer(); // flush primitives before operating with the VRAM
    glBindFramebuffer(GL_READ_FRAMEBUFFER, vram_fbo);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(
        rect->x, rect->y, rect->w, rect->h, GL_RGBA, GL_UNSIGNED_BYTE, buf);
    ConvertRgba8888ToRgb5551(buf, (u16*)p, count);
    BindDrawFbo();
}

static bool EnsureScratchFbo(void) {
    if (scratch_fbo) {
        return true;
    }
    glGenTextures(1, &scratch_texture);
    glBindTexture(GL_TEXTURE_2D, scratch_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, VRAM_W, VRAM_H, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
    glGenFramebuffers(1, &scratch_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, scratch_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           scratch_texture, 0);
    bool ok =
        glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, vram_fbo);
    glBindTexture(GL_TEXTURE_2D, vram_texture);
    if (!ok) {
        ERRORF("scratch FBO creation failed");
        glDeleteTextures(1, &scratch_texture);
        scratch_texture = 0;
        glDeleteFramebuffers(1, &scratch_fbo);
        scratch_fbo = 0;
    }
    return ok;
}
void Draw_MoveImage(PS1_RECT* rect, unsigned int x, unsigned int y) {
    if (rect->x == x && rect->y == y) {
        return;
    }
    Draw_FlushBuffer(); // flush primitives before operating with the VRAM

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

    // same-FBO blit work-around for MESA and macOS
    if (!EnsureScratchFbo()) {
        return;
    }
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, vram_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, scratch_fbo);
    glBlitFramebuffer(src_x, src_y, src_x + copy_w, src_y + copy_h, 0, 0,
                      copy_w, copy_h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, scratch_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, vram_fbo);
    glBlitFramebuffer(0, 0, copy_w, copy_h, dst_x, dst_y, dst_x + copy_w,
                      dst_y + copy_h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    SyncNativeVramToScaled(dst_x, dst_y, copy_w, copy_h);
    BindDrawFbo();
    glEnable(GL_SCISSOR_TEST);
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
    glUniform1i(uniform_dither, GetCurrentDither());
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(
        GL_ARRAY_BUFFER, 0, (GLsizeiptr)(sizeof(Vertex) * (size_t)n_vertices),
        vertex_buf);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferSubData(
        GL_ELEMENT_ARRAY_BUFFER, 0,
        (GLsizeiptr)(sizeof(*index_buf) * (size_t)n_indices), index_buf);
    glBindVertexArray(VAO);
    int prim_size = 3;
    int start = 0;
    bool cur_subtract = false;
    while (start < n_indices) {
        Vertex* v = &vertex_buf[index_buf[start]];
        bool need_subtract = is_subtract_abr(v);
        int end = start + prim_size;
        while (end < n_indices) {
            v = &vertex_buf[index_buf[end]];
            bool next_subtract = is_subtract_abr(v);
            if (next_subtract != need_subtract) {
                break;
            }
            end += prim_size;
        }
        if (need_subtract != cur_subtract) {
            glBlendEquation(
                need_subtract ? GL_FUNC_REVERSE_SUBTRACT : GL_FUNC_ADD);
            cur_subtract = need_subtract;
        }
        glDrawElements(
            GL_TRIANGLES, end - start, GL_UNSIGNED_SHORT,
            (const GLvoid*)((uintptr_t)start * sizeof(unsigned short)));
        start = end;
    }
    if (cur_subtract) {
        glBlendEquation(GL_FUNC_ADD);
    }
    SyncScaledVramToNative();
    Draw_ResetBuffer();
}
