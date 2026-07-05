#include <psyz.h>
#include <psyz/overlay.h>
#include <psyz/overlay_sdl3_gpu.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "../internal.h"
#include <SDL3/SDL.h>
#include "sdl3_common.h"

#include "shaders/psx_vert_spv.h"
#include "shaders/psx_frag_spv.h"
#include "shaders/clear_vert_spv.h"
#include "shaders/clear_frag_spv.h"
#include "shaders/psx_vert_msl.h"
#include "shaders/psx_frag_msl.h"
#include "shaders/clear_vert_msl.h"
#include "shaders/clear_frag_msl.h"

typedef struct {
    int x, y;
} Posi;

#define VRAM_BYTES (VRAM_W * VRAM_H * 4)

static SDL_GPUDevice* device = NULL;
static bool swapchain_ok = false;
static SDL_GPUTexture* vram_render = NULL;
static SDL_GPUTexture* vram_sample = NULL;
static SDL_GPUSampler* vram_sampler = NULL;
static SDL_GPUTexture* scaled_vram_render = NULL;
static unsigned internal_res = 1;
static unsigned set_internal_res = 1;
static SDL_GPUBuffer* vbuf = NULL;
static SDL_GPUBuffer* ibuf = NULL;
static SDL_GPUBuffer* clear_vbuf = NULL;
static SDL_GPUTransferBuffer* vtx_transfer = NULL;
static SDL_GPUTransferBuffer* tex_upload_transfer = NULL;
static SDL_GPUTransferBuffer* tex_download_transfer = NULL;
static SDL_GPUGraphicsPipeline* pipe_tri_add = NULL;
static SDL_GPUGraphicsPipeline* pipe_tri_sub = NULL;
static SDL_GPUGraphicsPipeline* pipe_clear = NULL;
static SDL_GPUCommandBuffer* pending_cmd = NULL;

static Posi display_area = {0, 0};
static Posi display_size = {256, 240};
static Posi cur_display_size = {-1, -1};
static Posi draw_offset = {0, 0};
static Posi draw_area_start = {0, 0};
static Posi draw_area_end = {0x10000, 0x10000};
static SDL_Rect scissor_rect = {0, 0, VRAM_W, VRAM_H};

static PsyzOverlayInitCB_SDL3GPU overlay_init_cb;
PsyzOverlayInitCB_SDL3GPU Psyz_OverlayInit_SDL3GPU(
    PsyzOverlayInitCB_SDL3GPU cb) {
    const PsyzOverlayInitCB_SDL3GPU prev = overlay_init_cb;
    overlay_init_cb = cb;
    return prev;
}

static PsyzOverlayRenderCB_SDL3GPU overlay_render_cb;
PsyzOverlayRenderCB_SDL3GPU Psyz_OverlayRender_SDL3GPU(
    PsyzOverlayRenderCB_SDL3GPU cb) {
    const PsyzOverlayRenderCB_SDL3GPU prev = overlay_render_cb;
    overlay_render_cb = cb;
    return prev;
}

static SDL_GPUCommandBuffer* AcquireCmd(void) {
    if (!pending_cmd) {
        pending_cmd = SDL_AcquireGPUCommandBuffer(device);
        if (!pending_cmd) {
            ERRORF("SDL_AcquireGPUCommandBuffer: %s", SDL_GetError());
        }
    }
    return pending_cmd;
}

static void SubmitCmd(void) {
    if (pending_cmd) {
        SDL_SubmitGPUCommandBuffer(pending_cmd);
        pending_cmd = NULL;
    }
}

static void SubmitCmdAndWait(void) {
    if (!pending_cmd) {
        return;
    }
    SDL_GPUFence* fence =
        SDL_SubmitGPUCommandBufferAndAcquireFence(pending_cmd);
    pending_cmd = NULL;
    if (fence) {
        SDL_WaitForGPUFences(device, true, &fence, 1);
        SDL_ReleaseGPUFence(device, fence);
    }
}

static SDL_GPUShader* CreateShader(
    const unsigned char* spirv, unsigned int spirv_len,
    const unsigned char* msl, unsigned int msl_len, SDL_GPUShaderStage stage,
    Uint32 num_samplers, Uint32 num_uniform_buffers) {
    const SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);
    SDL_GPUShaderCreateInfo info = {
        .stage = stage,
        .num_samplers = num_samplers,
        .num_uniform_buffers = num_uniform_buffers,
    };
    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        info.code = spirv;
        info.code_size = spirv_len;
        info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        info.entrypoint = "main";
    } else if (formats & SDL_GPU_SHADERFORMAT_MSL) {
        info.code = msl;
        info.code_size = msl_len;
        info.format = SDL_GPU_SHADERFORMAT_MSL;
        info.entrypoint = "main0";
    } else {
        ERRORF("no supported GPU shader format (formats=0x%x)", formats);
        return NULL;
    }
    SDL_GPUShader* shader = SDL_CreateGPUShader(device, &info);
    if (!shader) {
        ERRORF("SDL_CreateGPUShader: %s", SDL_GetError());
    }
    return shader;
}

static SDL_GPUGraphicsPipeline* CreatePsxPipeline(
    SDL_GPUShader* vs, SDL_GPUShader* fs, bool subtract) {
    const SDL_GPUVertexBufferDescription vb_desc = {
        .slot = 0,
        .pitch = sizeof(Vertex),
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    };
    const SDL_GPUVertexAttribute attribs[] = {
        {.location = 0,
         .buffer_slot = 0,
         .format = SDL_GPU_VERTEXELEMENTFORMAT_SHORT2,
         .offset = offsetof(Vertex, x)},
        {.location = 1,
         .buffer_slot = 0,
         .format = SDL_GPU_VERTEXELEMENTFORMAT_USHORT4,
         .offset = offsetof(Vertex, u)},
        {.location = 2,
         .buffer_slot = 0,
         .format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,
         .offset = offsetof(Vertex, r)},
    };
    const SDL_GPUBlendOp op =
        subtract ? SDL_GPU_BLENDOP_REVERSE_SUBTRACT : SDL_GPU_BLENDOP_ADD;
    const SDL_GPUColorTargetDescription target = {
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .blend_state =
            {
                .src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
                .dst_color_blendfactor =
                    SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                .color_blend_op = op,
                .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
                .dst_alpha_blendfactor =
                    SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                .alpha_blend_op = op,
                .enable_blend = true,
            },
    };
    const SDL_GPUGraphicsPipelineCreateInfo info = {
        .vertex_shader = vs,
        .fragment_shader = fs,
        .vertex_input_state =
            {
                .vertex_buffer_descriptions = &vb_desc,
                .num_vertex_buffers = 1,
                .vertex_attributes = attribs,
                .num_vertex_attributes = LENU(attribs),
            },
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .target_info =
            {
                .color_target_descriptions = &target,
                .num_color_targets = 1,
            },
    };
    SDL_GPUGraphicsPipeline* pipe =
        SDL_CreateGPUGraphicsPipeline(device, &info);
    if (!pipe) {
        ERRORF("SDL_CreateGPUGraphicsPipeline: %s", SDL_GetError());
    }
    return pipe;
}

// used only for a fast hardware ClearImage
static SDL_GPUGraphicsPipeline* CreateClearPipeline(
    SDL_GPUShader* vs, SDL_GPUShader* fs) {
    const SDL_GPUVertexBufferDescription vb_desc = {
        .slot = 0,
        .pitch = sizeof(Vertex),
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    };
    const SDL_GPUVertexAttribute attribs[] = {
        {.location = 0,
         .buffer_slot = 0,
         .format = SDL_GPU_VERTEXELEMENTFORMAT_SHORT2,
         .offset = offsetof(Vertex, x)},
        {.location = 1,
         .buffer_slot = 0,
         .format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,
         .offset = offsetof(Vertex, r)},
    };
    // blending disabled: the clear color (including alpha 0, the PS1 mask
    // bit) is written as-is, matching glClearColor(r, g, b, 0) + glClear
    const SDL_GPUColorTargetDescription target = {
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
    };
    const SDL_GPUGraphicsPipelineCreateInfo info = {
        .vertex_shader = vs,
        .fragment_shader = fs,
        .vertex_input_state =
            {
                .vertex_buffer_descriptions = &vb_desc,
                .num_vertex_buffers = 1,
                .vertex_attributes = attribs,
                .num_vertex_attributes = LENU(attribs),
            },
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP,
        .target_info =
            {
                .color_target_descriptions = &target,
                .num_color_targets = 1,
            },
    };
    SDL_GPUGraphicsPipeline* pipe =
        SDL_CreateGPUGraphicsPipeline(device, &info);
    if (!pipe) {
        ERRORF("SDL_CreateGPUGraphicsPipeline: %s", SDL_GetError());
    }
    return pipe;
}

static bool CreateGpuResources(void) {
    const SDL_GPUTextureCreateInfo render_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage =
            SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = VRAM_W,
        .height = VRAM_H,
        .layer_count_or_depth = 1,
        .num_levels = 1,
    };
    vram_render = SDL_CreateGPUTexture(device, &render_info);
    SDL_GPUTextureCreateInfo sample_info = render_info;
    sample_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    vram_sample = SDL_CreateGPUTexture(device, &sample_info);
    if (!vram_render || !vram_sample) {
        ERRORF("SDL_CreateGPUTexture: %s", SDL_GetError());
        return false;
    }

    const SDL_GPUSamplerCreateInfo sampler_info = {
        .min_filter = SDL_GPU_FILTER_NEAREST,
        .mag_filter = SDL_GPU_FILTER_NEAREST,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    vram_sampler = SDL_CreateGPUSampler(device, &sampler_info);
    if (!vram_sampler) {
        ERRORF("SDL_CreateGPUSampler: %s", SDL_GetError());
        return false;
    }

    const SDL_GPUBufferCreateInfo vbuf_info = {
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
        .size = MAX_VERTEX_COUNT * sizeof(Vertex),
    };
    vbuf = SDL_CreateGPUBuffer(device, &vbuf_info);
    const SDL_GPUBufferCreateInfo ibuf_info = {
        .usage = SDL_GPU_BUFFERUSAGE_INDEX,
        .size = MAX_INDEX_COUNT * sizeof(u16),
    };
    ibuf = SDL_CreateGPUBuffer(device, &ibuf_info);
    const SDL_GPUBufferCreateInfo clear_vbuf_info = {
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
        .size = 4 * sizeof(Vertex),
    };
    clear_vbuf = SDL_CreateGPUBuffer(device, &clear_vbuf_info);
    if (!vbuf || !ibuf || !clear_vbuf) {
        ERRORF("SDL_CreateGPUBuffer: %s", SDL_GetError());
        return false;
    }

    const SDL_GPUTransferBufferCreateInfo vtx_transfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size =
            MAX_VERTEX_COUNT * sizeof(Vertex) + MAX_INDEX_COUNT * sizeof(u16),
    };
    vtx_transfer = SDL_CreateGPUTransferBuffer(device, &vtx_transfer_info);
    const SDL_GPUTransferBufferCreateInfo upload_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = VRAM_BYTES,
    };
    tex_upload_transfer = SDL_CreateGPUTransferBuffer(device, &upload_info);
    const SDL_GPUTransferBufferCreateInfo download_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
        .size = VRAM_BYTES,
    };
    tex_download_transfer = SDL_CreateGPUTransferBuffer(device, &download_info);
    if (!vtx_transfer || !tex_upload_transfer || !tex_download_transfer) {
        ERRORF("SDL_CreateGPUTransferBuffer: %s", SDL_GetError());
        return false;
    }

    SDL_GPUShader* psx_vs =
        CreateShader(psx_vert_spv, psx_vert_spv_len, psx_vert_msl,
                     psx_vert_msl_len, SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);
    SDL_GPUShader* psx_fs =
        CreateShader(psx_frag_spv, psx_frag_spv_len, psx_frag_msl,
                     psx_frag_msl_len, SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1);
    SDL_GPUShader* clear_vs =
        CreateShader(clear_vert_spv, clear_vert_spv_len, clear_vert_msl,
                     clear_vert_msl_len, SDL_GPU_SHADERSTAGE_VERTEX, 0, 0);
    SDL_GPUShader* clear_fs =
        CreateShader(clear_frag_spv, clear_frag_spv_len, clear_frag_msl,
                     clear_frag_msl_len, SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0);
    bool shaders_ok = psx_vs && psx_fs && clear_vs && clear_fs;
    if (shaders_ok) {
        pipe_tri_add = CreatePsxPipeline(psx_vs, psx_fs, false);
        pipe_tri_sub = CreatePsxPipeline(psx_vs, psx_fs, true);
        pipe_clear = CreateClearPipeline(clear_vs, clear_fs);
    }
    if (psx_vs) {
        SDL_ReleaseGPUShader(device, psx_vs);
    }
    if (psx_fs) {
        SDL_ReleaseGPUShader(device, psx_fs);
    }
    if (clear_vs) {
        SDL_ReleaseGPUShader(device, clear_vs);
    }
    if (clear_fs) {
        SDL_ReleaseGPUShader(device, clear_fs);
    }
    return shaders_ok && pipe_tri_add && pipe_tri_sub && pipe_clear;
}

bool InitPlatform() {
    if (is_platform_initialized) {
        return is_platform_init_successful;
    }
    is_platform_initialized = true; // don't re-initialize on failures

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        ERRORF("SDL_Init: %s", SDL_GetError());
        return false;
    }

    device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL, false, NULL);
    if (!device) {
        ERRORF("SDL_CreateGPUDevice: %s", SDL_GetError());
        return false;
    }
    INFOF("SDL_GPU with driver %s", SDL_GetGPUDeviceDriver(device));

    sdl3_window = SDL_CreateWindow(
        window_title, DEFAULT_FRONT_W, DEFAULT_FRONT_H,
        SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE |
            SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!sdl3_window) {
        ERRORF("SDL_CreateWindow: %s", SDL_GetError());
        return false;
    }

    // useful for offscreen rendering with SDL_VIDEODRIVER=offscreen
    swapchain_ok = SDL_ClaimWindowForGPUDevice(device, sdl3_window);
    if (!swapchain_ok) {
        WARNF("no swapchain, presentation disabled: %s", SDL_GetError());
    }

    if (!CreateGpuResources()) {
        return false;
    }

    cur_tpage = 0;
    Sdl3Common_TimingInit();

    is_platform_init_successful = true;
    if (overlay_init_cb) {
        overlay_init_cb(sdl3_window, device);
    }
    return true;
}

static void PlatformBackend_SetDriverVsync(bool enable) {
    if (!device || !swapchain_ok) {
        return;
    }
    SDL_GPUPresentMode mode = SDL_GPU_PRESENTMODE_VSYNC;
    if (!enable) {
        if (SDL_WindowSupportsGPUPresentMode(
                device, sdl3_window, SDL_GPU_PRESENTMODE_IMMEDIATE)) {
            mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
        } else if (SDL_WindowSupportsGPUPresentMode(
                       device, sdl3_window, SDL_GPU_PRESENTMODE_MAILBOX)) {
            mode = SDL_GPU_PRESENTMODE_MAILBOX;
        }
    }
    SDL_SetGPUSwapchainParameters(
        device, sdl3_window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode);
}

static void UpdateScissor(void);

static SDL_GPUTexture* GetRenderTarget(void) {
    return internal_res <= 1 ? vram_render : scaled_vram_render;
}

static SDL_Rect vram_dirty = {0, 0, 0, 0};
static void MarkVramDirty(SDL_Rect r) {
    if (r.w <= 0 || r.h <= 0) {
        return;
    }
    int x0 = CLAMP(r.x, 0, VRAM_W);
    int y0 = CLAMP(r.y, 0, VRAM_H);
    int x1 = CLAMP(r.x + r.w, 0, VRAM_W);
    int y1 = CLAMP(r.y + r.h, 0, VRAM_H);
    if (x1 <= x0 || y1 <= y0) {
        return;
    }
    if (vram_dirty.w <= 0 || vram_dirty.h <= 0) {
        vram_dirty = (SDL_Rect){x0, y0, x1 - x0, y1 - y0};
        return;
    }
    int ux0 = x0 < vram_dirty.x ? x0 : vram_dirty.x;
    int uy0 = y0 < vram_dirty.y ? y0 : vram_dirty.y;
    int ux1 =
        x1 > vram_dirty.x + vram_dirty.w ? x1 : vram_dirty.x + vram_dirty.w;
    int uy1 =
        y1 > vram_dirty.y + vram_dirty.h ? y1 : vram_dirty.y + vram_dirty.h;
    vram_dirty = (SDL_Rect){ux0, uy0, ux1 - ux0, uy1 - uy0};
}
static void ResetVramDirty(void) { vram_dirty = (SDL_Rect){0, 0, 0, 0}; }

// mirror a native VRAM region into the scaled render target
static void SyncNativeVramToScaled(int x, int y, int w, int h) {
    if (internal_res <= 1 || !scaled_vram_render || w <= 0 || h <= 0) {
        return;
    }
    SDL_GPUCommandBuffer* cmd = AcquireCmd();
    if (!cmd) {
        return;
    }
    const SDL_GPUBlitInfo blit = {
        .source = {.texture = vram_render,
                   .x = (Uint32)x,
                   .y = (Uint32)y,
                   .w = (Uint32)w,
                   .h = (Uint32)h},
        .destination = {.texture = scaled_vram_render,
                        .x = (Uint32)(x * internal_res),
                        .y = (Uint32)(y * internal_res),
                        .w = (Uint32)(w * internal_res),
                        .h = (Uint32)(h * internal_res)},
        .load_op = SDL_GPU_LOADOP_LOAD,
        .filter = SDL_GPU_FILTER_NEAREST,
    };
    SDL_BlitGPUTexture(cmd, &blit);
}

static void SyncScaledVramToNative(void) {
    if (internal_res <= 1 || !scaled_vram_render) {
        return;
    }
    int x0 = CLAMP(draw_area_start.x, 0, VRAM_W);
    int y0 = CLAMP(draw_area_start.y, 0, VRAM_H);
    int x1 = CLAMP(draw_area_end.x + 1, 0, VRAM_W);
    int y1 = CLAMP(draw_area_end.y + 1, 0, VRAM_H);
    if (x1 <= x0 || y1 <= y0) {
        return;
    }
    SDL_GPUCommandBuffer* cmd = AcquireCmd();
    if (!cmd) {
        return;
    }
    const SDL_GPUBlitInfo blit = {
        .source = {.texture = scaled_vram_render,
                   .x = (Uint32)(x0 * internal_res),
                   .y = (Uint32)(y0 * internal_res),
                   .w = (Uint32)((x1 - x0) * internal_res),
                   .h = (Uint32)((y1 - y0) * internal_res)},
        .destination = {.texture = vram_render,
                        .x = (Uint32)x0,
                        .y = (Uint32)y0,
                        .w = (Uint32)(x1 - x0),
                        .h = (Uint32)(y1 - y0)},
        .load_op = SDL_GPU_LOADOP_LOAD,
        .filter = SDL_GPU_FILTER_NEAREST,
    };
    SDL_BlitGPUTexture(cmd, &blit);
    MarkVramDirty((SDL_Rect){x0, y0, x1 - x0, y1 - y0});
}

static bool CreateScaledVramTarget(unsigned n) {
    if (scaled_vram_render) {
        SDL_ReleaseGPUTexture(device, scaled_vram_render);
        scaled_vram_render = NULL;
    }
    const SDL_GPUTextureCreateInfo info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage =
            SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = VRAM_W * n,
        .height = VRAM_H * n,
        .layer_count_or_depth = 1,
        .num_levels = 1,
    };
    scaled_vram_render = SDL_CreateGPUTexture(device, &info);
    if (!scaled_vram_render) {
        ERRORF(
            "scaled VRAM texture creation failed (%dx): %s", n, SDL_GetError());
        return false;
    }
    return true;
}

// ensure scaled VRAM does not exceed max allowed texture size
static unsigned AdjustInternalRes(unsigned scale) {
    unsigned n = scale;
    const unsigned max_dim = 16384; // max theoretical texture size
    unsigned vram_dim = VRAM_W > VRAM_H ? VRAM_W : VRAM_H;
    while (n > 1 && vram_dim * n > max_dim) {
        n--;
    }
    if (n != scale) {
        WARNF("internal resolution %dx exceeds max texture size, "
              "fallback to internal resolution %dx",
              scale, n);
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
    if (n > 1 && !CreateScaledVramTarget(n)) {
        n = 1;
        set_internal_res = 1;
    }
    if (n > 1) {
        // upscale the whole native VRAM into the freshly created scaled target
        SDL_GPUCommandBuffer* cmd = AcquireCmd();
        if (cmd) {
            const SDL_GPUBlitInfo blit = {
                .source = {.texture = vram_render, .w = VRAM_W, .h = VRAM_H},
                .destination = {.texture = scaled_vram_render,
                                .w = VRAM_W * n,
                                .h = VRAM_H * n},
                .load_op = SDL_GPU_LOADOP_DONT_CARE,
                .filter = SDL_GPU_FILTER_NEAREST,
            };
            SDL_BlitGPUTexture(cmd, &blit);
        }
    } else if (scaled_vram_render) {
        SDL_ReleaseGPUTexture(device, scaled_vram_render);
        scaled_vram_render = NULL;
    }
    internal_res = n;
    UpdateScissor();
    INFOF("internal resolution set to %dx (%dx%d)", n, VRAM_W * n, VRAM_H * n);
}

static void PlatformBackend_Present(void) {
    if (!sdl3_window && !InitPlatform()) {
        return;
    }

    ApplyPendingInternalRes();

    SDL_GPUCommandBuffer* cmd = AcquireCmd();
    if (!cmd) {
        return;
    }
    if (swapchain_ok) {
        SDL_GPUTexture* swapchain = NULL;
        Uint32 sc_w = 0, sc_h = 0;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(
                cmd, sdl3_window, &swapchain, &sc_w, &sc_h)) {
            WARNF("SDL_WaitAndAcquireGPUSwapchainTexture: %s", SDL_GetError());
        }
        if (swapchain) {
            const Uint32 n = internal_res;
            SDL_GPUBlitRegion src = {
                .texture = GetRenderTarget(),
                .x = (Uint32)display_area.x * n,
                .y = (Uint32)display_area.y * n,
                .w = (Uint32)display_size.x * n,
                .h = (Uint32)display_size.y * n,
            };
            float game_aspect =
                GetCurrentGameAspectRatio(display_size.x, display_size.y);
            if (debug_show_vram) {
                src.x = 0;
                src.y = 0;
                src.w = VRAM_W * n;
                src.h = VRAM_H * n;
                game_aspect = (float)VRAM_W / (float)VRAM_H;
            }

            WndSize win = {(int)sc_w, (int)sc_h};
            SDL_Rect dst = FitGameToWindow(game_aspect, win);

            const SDL_GPUBlitInfo blit = {
                .source = src,
                .destination =
                    {
                        .texture = swapchain,
                        .x = (Uint32)dst.x,
                        .y = (Uint32)dst.y,
                        .w = (Uint32)dst.w,
                        .h = (Uint32)dst.h,
                    },
                // clear the swapchain to black first so the horizontal or
                // vertical bars around the game output are black
                .load_op = SDL_GPU_LOADOP_CLEAR,
                .clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
                .filter = SDL_GPU_FILTER_NEAREST,
            };
            SDL_BlitGPUTexture(cmd, &blit);
            if (overlay_frame_cb) {
                overlay_frame_cb();
            }
            if (overlay_render_cb) {
                SDL_GPUColorTargetInfo target = {
                    .texture = swapchain,
                    .load_op = SDL_GPU_LOADOP_LOAD,
                    .store_op = SDL_GPU_STOREOP_STORE,
                };
                SDL_GPURenderPass* pass =
                    SDL_BeginGPURenderPass(cmd, &target, 1, NULL);
                overlay_render_cb(cmd, pass);
                SDL_EndGPURenderPass(pass);
            }
        }
    } else if (overlay_frame_cb) {
        overlay_frame_cb();
    }
    finish_time = SDL_GetPerformanceCounter();
    SubmitCmd();
}

static void QuitPlatform(void) {
    if (overlay_destroy_cb) {
        overlay_destroy_cb();
    }
    if (device) {
        SDL_WaitForGPUIdle(device);
        if (pipe_tri_add) {
            SDL_ReleaseGPUGraphicsPipeline(device, pipe_tri_add);
            pipe_tri_add = NULL;
        }
        if (pipe_tri_sub) {
            SDL_ReleaseGPUGraphicsPipeline(device, pipe_tri_sub);
            pipe_tri_sub = NULL;
        }
        if (pipe_clear) {
            SDL_ReleaseGPUGraphicsPipeline(device, pipe_clear);
            pipe_clear = NULL;
        }
        if (vram_render) {
            SDL_ReleaseGPUTexture(device, vram_render);
            vram_render = NULL;
        }
        if (vram_sample) {
            SDL_ReleaseGPUTexture(device, vram_sample);
            vram_sample = NULL;
        }
        if (scaled_vram_render) {
            SDL_ReleaseGPUTexture(device, scaled_vram_render);
            scaled_vram_render = NULL;
        }
        if (vram_sampler) {
            SDL_ReleaseGPUSampler(device, vram_sampler);
            vram_sampler = NULL;
        }
        if (vbuf) {
            SDL_ReleaseGPUBuffer(device, vbuf);
            vbuf = NULL;
        }
        if (ibuf) {
            SDL_ReleaseGPUBuffer(device, ibuf);
            ibuf = NULL;
        }
        if (clear_vbuf) {
            SDL_ReleaseGPUBuffer(device, clear_vbuf);
            clear_vbuf = NULL;
        }
        if (vtx_transfer) {
            SDL_ReleaseGPUTransferBuffer(device, vtx_transfer);
            vtx_transfer = NULL;
        }
        if (tex_upload_transfer) {
            SDL_ReleaseGPUTransferBuffer(device, tex_upload_transfer);
            tex_upload_transfer = NULL;
        }
        if (tex_download_transfer) {
            SDL_ReleaseGPUTransferBuffer(device, tex_download_transfer);
            tex_download_transfer = NULL;
        }
        if (swapchain_ok) {
            SDL_ReleaseWindowFromGPUDevice(device, sdl3_window);
            swapchain_ok = false;
        }
        SDL_DestroyGPUDevice(device);
        device = NULL;
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
    internal_res = 1;
    QuitPlatform();
}

static bool DownloadVramRegionAsRGBA8888(int x, int y, int w, int h, u8* out) {
    SDL_GPUCommandBuffer* cmd = AcquireCmd();
    if (!cmd) {
        return false;
    }
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
    const SDL_GPUTextureRegion region = {
        .texture = vram_render,
        .x = (Uint32)x,
        .y = (Uint32)y,
        .w = (Uint32)w,
        .h = (Uint32)h,
        .d = 1,
    };
    const SDL_GPUTextureTransferInfo transfer = {
        .transfer_buffer = tex_download_transfer,
    };
    SDL_DownloadFromGPUTexture(copy, &region, &transfer);
    SDL_EndGPUCopyPass(copy);
    SubmitCmdAndWait();

    const u8* map =
        SDL_MapGPUTransferBuffer(device, tex_download_transfer, false);
    if (!map) {
        ERRORF("SDL_MapGPUTransferBuffer: %s", SDL_GetError());
        return false;
    }
    memcpy(out, map, (size_t)w * h * 4);
    SDL_UnmapGPUTransferBuffer(device, tex_download_transfer);
    return true;
}

unsigned char* Psyz_VideoAllocCapturedFrame(int* w, int* h) {
    const int channels = 3;
    if (!device || !vram_render) {
        *w = *h = 0;
        ERRORF("GPU device not initialized");
        return NULL;
    }

    *w = display_size.x;
    *h = display_size.y;
    unsigned char* pixels = malloc((*w) * (*h) * channels);
    u8* rgba = malloc((size_t)(*w) * (*h) * 4);
    if (!pixels || !rgba) {
        free(pixels);
        free(rgba);
        return NULL;
    }

    if (!DownloadVramRegionAsRGBA8888(
            display_area.x, display_area.y, *w, *h, rgba)) {
        free(pixels);
        free(rgba);
        return NULL;
    }
    const u8* src = rgba;
    unsigned char* dst = pixels;
    for (int i = 0; i < (*w) * (*h); i++) {
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst += 3;
        src += 4;
    }
    free(rgba);
    return pixels;
}

static void UpdateScissor() {
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

    scissor_rect.x = draw_area_start.x;
    scissor_rect.y = draw_area_start.y;
    scissor_rect.w = width;
    scissor_rect.h = height;
}

unsigned Psyz_VideoGetInternalResolution(void) { return internal_res; }

int Psyz_VideoSetInternalResolution(unsigned multiplier) {
    if (multiplier < 1) {
        return -1;
    }
    if (multiplier > PSYZ_INTERNAL_RES_MAX) {
        WARNF("internal resolution %dx exceeds maximum value of %dx",
              multiplier, PSYZ_INTERNAL_RES_MAX);
        return -1;
    }
    set_internal_res = multiplier;
    if (sdl3_window && is_platform_init_successful) {
        ApplyPendingInternalRes();
    }
    return 0;
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
    }
    if (!is_window_visible) {
        SDL_ShowWindow(sdl3_window);
        is_window_visible = true;
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
        SDL_GPUCommandBuffer* cmd = AcquireCmd();
        if (!cmd) {
            return;
        }
        const SDL_GPUColorTargetInfo target = {
            .texture = vram_render,
            .clear_color = {0, 0, 0, 1},
            .load_op = SDL_GPU_LOADOP_CLEAR,
            .store_op = SDL_GPU_STOREOP_STORE,
        };
        SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmd, &target, 1, NULL);
        SDL_EndGPURenderPass(pass);
        if (internal_res > 1 && scaled_vram_render) {
            const SDL_GPUColorTargetInfo scaled_target = {
                .texture = scaled_vram_render,
                .clear_color = {0, 0, 0, 1},
                .load_op = SDL_GPU_LOADOP_CLEAR,
                .store_op = SDL_GPU_STOREOP_STORE,
            };
            SDL_GPURenderPass* scaled_pass =
                SDL_BeginGPURenderPass(cmd, &scaled_target, 1, NULL);
            SDL_EndGPURenderPass(scaled_pass);
        }
    } else {
        ApplyDisplayPendingChanges();
    }
}

void Draw_DisplayArea(unsigned int x, unsigned int y) {
    display_area.x = (int)x;
    display_area.y = (int)y;

    Draw_FlushBuffer();
    ApplyDisplayPendingChanges();
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

// optimization to avoid sampling the VRAM on an untextured batch draw
static bool batch_has_texture = false;
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
    batch_has_texture |= isTextured;
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
}

void Draw_ClearImage(PS1_RECT* rect, u_char r, u_char g, u_char b) {
    if (rect->w == 0 || rect->h == 0) {
        return;
    }
    if (!sdl3_window && !InitPlatform()) {
        return;
    }
    SDL_GPUCommandBuffer* cmd = AcquireCmd();
    if (!cmd) {
        return;
    }

    Vertex quad[4] = {0};
    quad[0].x = rect->x;
    quad[0].y = rect->y;
    quad[1].x = (short)(rect->x + rect->w);
    quad[1].y = rect->y;
    quad[2].x = rect->x;
    quad[2].y = (short)(rect->y + rect->h);
    quad[3].x = (short)(rect->x + rect->w);
    quad[3].y = (short)(rect->y + rect->h);
    for (int i = 0; i < 4; i++) {
        quad[i].r = r;
        quad[i].g = g;
        quad[i].b = b;
        quad[i].a = 0;
    }

    Vertex* map = SDL_MapGPUTransferBuffer(device, vtx_transfer, true);
    if (!map) {
        ERRORF("SDL_MapGPUTransferBuffer: %s", SDL_GetError());
        return;
    }
    memcpy(map, quad, sizeof(quad));
    SDL_UnmapGPUTransferBuffer(device, vtx_transfer);

    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
    const SDL_GPUTransferBufferLocation src = {.transfer_buffer = vtx_transfer};
    const SDL_GPUBufferRegion dst = {
        .buffer = clear_vbuf, .size = sizeof(quad)};
    SDL_UploadToGPUBuffer(copy, &src, &dst, true);
    SDL_EndGPUCopyPass(copy);

    const SDL_GPUColorTargetInfo target = {
        .texture = vram_render,
        .load_op = SDL_GPU_LOADOP_LOAD,
        .store_op = SDL_GPU_STOREOP_STORE,
    };
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmd, &target, 1, NULL);
    SDL_BindGPUGraphicsPipeline(pass, pipe_clear);
    const SDL_GPUBufferBinding vb = {.buffer = clear_vbuf};
    SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
    SDL_DrawGPUPrimitives(pass, 4, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
    MarkVramDirty((SDL_Rect){rect->x, rect->y, rect->w, rect->h});
    SyncNativeVramToScaled(rect->x, rect->y, rect->w, rect->h);
}

void Draw_LoadImage(PS1_RECT* rect, u_long* p) {
    if (rect->w == 0 || rect->h == 0) {
        return;
    }
    if (!sdl3_window && !InitPlatform()) {
        return;
    }
    SDL_GPUCommandBuffer* cmd = AcquireCmd();
    if (!cmd) {
        return;
    }
    u8* map = SDL_MapGPUTransferBuffer(device, tex_upload_transfer, true);
    if (!map) {
        ERRORF("SDL_MapGPUTransferBuffer: %s", SDL_GetError());
        return;
    }
    ConvertRgb5551ToRgba8888((const u16*)p, map, (size_t)rect->w * rect->h);
    SDL_UnmapGPUTransferBuffer(device, tex_upload_transfer);

    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
    const SDL_GPUTextureTransferInfo src = {
        .transfer_buffer = tex_upload_transfer,
        .pixels_per_row = rect->w,
        .rows_per_layer = rect->h,
    };
    const SDL_GPUTextureRegion dst = {
        .texture = vram_render,
        .x = (Uint32)rect->x,
        .y = (Uint32)rect->y,
        .w = rect->w,
        .h = rect->h,
        .d = 1,
    };
    SDL_UploadToGPUTexture(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);
    MarkVramDirty((SDL_Rect){rect->x, rect->y, rect->w, rect->h});
    SyncNativeVramToScaled(rect->x, rect->y, rect->w, rect->h);
}

void Draw_StoreImage(PS1_RECT* rect, u_long* p) {
    if (rect->w == 0 || rect->h == 0) {
        return;
    }
    if (!device || !vram_render) {
        return;
    }
    Draw_FlushBuffer(); // flush primitives before operating with the VRAM

    const size_t pixels = (size_t)rect->w * rect->h;
    u8* rgba = malloc(pixels * 4);
    if (!rgba) {
        return;
    }
    if (DownloadVramRegionAsRGBA8888(
            rect->x, rect->y, rect->w, rect->h, rgba)) {
        ConvertRgba8888ToRgb5551(rgba, (u16*)p, pixels);
    }
    free(rgba);
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

    SDL_GPUCommandBuffer* cmd = AcquireCmd();
    if (!cmd) {
        return;
    }
    // handles overlapping same-texture copies
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
    const SDL_GPUTextureLocation src = {
        .texture = vram_render, .x = (Uint32)src_x, .y = (Uint32)src_y};
    const SDL_GPUTextureLocation bounce = {
        .texture = vram_sample, .x = (Uint32)src_x, .y = (Uint32)src_y};
    const SDL_GPUTextureLocation dst = {
        .texture = vram_render, .x = (Uint32)dst_x, .y = (Uint32)dst_y};
    SDL_CopyGPUTextureToTexture(
        copy, &src, &bounce, (Uint32)copy_w, (Uint32)copy_h, 1, false);
    SDL_CopyGPUTextureToTexture(
        copy, &bounce, &dst, (Uint32)copy_w, (Uint32)copy_h, 1, false);
    SDL_EndGPUCopyPass(copy);
    MarkVramDirty((SDL_Rect){dst_x, dst_y, copy_w, copy_h});
    MarkVramDirty((SDL_Rect){src_x, src_y, copy_w, copy_h});
    SyncNativeVramToScaled(dst_x, dst_y, copy_w, copy_h);
}

void Draw_ResetBuffer(void) {
    n_vertices = 0;
    n_indices = 0;
    vertex_cur = vertex_buf;
    index_cur = index_buf;
    batch_has_texture = false;
}

void Draw_FlushBuffer(void) {
    if (n_vertices == 0) {
        return;
    }
    SDL_GPUCommandBuffer* cmd = AcquireCmd();
    if (!cmd) {
        return;
    }

    u8* map = SDL_MapGPUTransferBuffer(device, vtx_transfer, true);
    if (!map) {
        ERRORF("SDL_MapGPUTransferBuffer: %s", SDL_GetError());
        return;
    }
    const Uint32 vtx_size = (Uint32)(sizeof(Vertex) * n_vertices);
    const Uint32 idx_size = (Uint32)(sizeof(*index_buf) * n_indices);
    const Uint32 idx_offset = MAX_VERTEX_COUNT * sizeof(Vertex);
    memcpy(map, vertex_buf, vtx_size);
    memcpy(map + idx_offset, index_buf, idx_size);
    SDL_UnmapGPUTransferBuffer(device, vtx_transfer);

    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
    const SDL_GPUTransferBufferLocation vtx_src = {
        .transfer_buffer = vtx_transfer};
    const SDL_GPUBufferRegion vtx_dst = {.buffer = vbuf, .size = vtx_size};
    SDL_UploadToGPUBuffer(copy, &vtx_src, &vtx_dst, true);
    const SDL_GPUTransferBufferLocation idx_src = {
        .transfer_buffer = vtx_transfer, .offset = idx_offset};
    const SDL_GPUBufferRegion idx_dst = {.buffer = ibuf, .size = idx_size};
    SDL_UploadToGPUBuffer(copy, &idx_src, &idx_dst, true);
    if (batch_has_texture && vram_dirty.w > 0 && vram_dirty.h > 0) {
        const SDL_GPUTextureLocation vram_src = {.texture = vram_render,
                                                 .x = (Uint32)vram_dirty.x,
                                                 .y = (Uint32)vram_dirty.y};
        const SDL_GPUTextureLocation vram_dst = {.texture = vram_sample,
                                                 .x = (Uint32)vram_dirty.x,
                                                 .y = (Uint32)vram_dirty.y};
        SDL_CopyGPUTextureToTexture(
            copy, &vram_src, &vram_dst, (Uint32)vram_dirty.w,
            (Uint32)vram_dirty.h, 1, false);
        ResetVramDirty();
    }
    SDL_EndGPUCopyPass(copy);

    const SDL_GPUColorTargetInfo target = {
        .texture = GetRenderTarget(),
        .load_op = SDL_GPU_LOADOP_LOAD,
        .store_op = SDL_GPU_STOREOP_STORE,
    };
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmd, &target, 1, NULL);
    SDL_Rect scaled_scissor = {
        scissor_rect.x * (int)internal_res, scissor_rect.y * (int)internal_res,
        scissor_rect.w * (int)internal_res, scissor_rect.h * (int)internal_res};
    SDL_SetGPUScissor(pass, &scaled_scissor);
    const SDL_GPUBufferBinding vb = {.buffer = vbuf};
    SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
    const SDL_GPUBufferBinding ib = {.buffer = ibuf};
    SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);
    const SDL_GPUTextureSamplerBinding sampler_binding = {
        .texture = vram_sample, .sampler = vram_sampler};
    SDL_BindGPUFragmentSamplers(pass, 0, &sampler_binding, 1);
    const float offset_ubo[4] = {
        (float)draw_offset.x, (float)draw_offset.y, 0, 0};
    SDL_PushGPUVertexUniformData(cmd, 0, offset_ubo, sizeof(offset_ubo));
    const int dither_ubo[4] = {GetCurrentDither(), 0, 0, 0};
    SDL_PushGPUFragmentUniformData(cmd, 0, dither_ubo, sizeof(dither_ubo));

    // every primitive (including lines, expanded to quads) is a triangle list
    const int prim_size = 3;
    int start = 0;
    bool cur_subtract = false;
    bool pipeline_bound = false;
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
        if (!pipeline_bound || need_subtract != cur_subtract) {
            SDL_GPUGraphicsPipeline* pipe =
                need_subtract ? pipe_tri_sub : pipe_tri_add;
            SDL_BindGPUGraphicsPipeline(pass, pipe);
            cur_subtract = need_subtract;
            pipeline_bound = true;
        }
        SDL_DrawGPUIndexedPrimitives(
            pass, (Uint32)(end - start), 1, (Uint32)start, 0, 0);
        start = end;
    }
    SDL_EndGPURenderPass(pass);
    if (internal_res <= 1) {
        MarkVramDirty(scissor_rect);
    }
    SyncScaledVramToNative();
    Draw_ResetBuffer();
}
