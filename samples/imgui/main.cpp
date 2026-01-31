#include <psyz.h>
#include <psyz/overlay.h>
#include <psyz/overlay_sdl3_gl.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__APPLE__)
#include "glad/glad.h"
#else
#include <SDL3/SDL_opengl.h>
#include <GLES3/gl3.h>
#endif

static ImGuiContext* imgui_context;

static bool show_gpu_stats = true;
static int current_vsync_mode = PSYZ_VSYNC_AUTO;
static void ShowGpuStats(bool* show) {
    if (!show || !*show) {
        return;
    }
    Psyz_GpuStats stats;
    if (Psyz_GetGpuStats(&stats) < 0) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("GPU Stats", show)) {
        double fps = stats.last_frame_time_us > 0
                         ? 1000000.0 / stats.last_frame_time_us
                         : 0;
        double target_fps = stats.target_frame_time_us > 0
                                ? 1000000.0 / stats.target_frame_time_us
                                : 0;

        ImGui::Text("Frame Time: %.2f ms", stats.last_frame_time_us / 1000.0);
        ImGui::Text("Draw Time: %.2f ms", stats.last_draw_time_us / 1000.0);
        ImGui::Text("FPS: %.2f", fps);
        ImGui::Text("Target FPS: %.2f", target_fps);
        ImGui::Text("Total Frames: %llu", stats.total_frames);
        ImGui::Text("VSync: %s",
                    stats.using_driver_vsync ? "Driver" : "Manual Limiter");

        const char* vsync_modes[] = {"Auto", "On", "Off"};
        if (ImGui::Combo("VSync Mode", &current_vsync_mode, vsync_modes, 3)) {
            Psyz_SetVsyncMode((Psyz_VsyncMode)current_vsync_mode);
        }
    }
    ImGui::End();
}

static bool show_vram = true;
static GLuint vram_texture = 0;
static int vram_width = 0;
static int vram_height = 0;

static void ShowCapturedFrame(bool* show) {
    if (!show || !*show) {
        return;
    }
    int width, height;
    unsigned char* data = Psyz_AllocAndCaptureFrame(&width, &height);
    if (!data) {
        return;
    }

    if (vram_texture == 0) {
        glGenTextures(1, &vram_texture);
    }

    glBindTexture(GL_TEXTURE_2D, vram_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, data);
    vram_width = width;
    vram_height = height;
    free(data);

    ImGui::SetNextWindowSize(
        ImVec2((float)width + 16, (float)height + 40), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("VRAM", show)) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float scale = (avail.x < avail.y * ((float)vram_width / vram_height))
                          ? avail.x / vram_width
                          : avail.y / vram_height;
        if (scale > 1.0f)
            scale = 1.0f;
        ImGui::Image((ImTextureID)(intptr_t)vram_texture,
                     ImVec2(vram_width * scale, vram_height * scale));
    }
    ImGui::End();
}

static void imgui_frame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ShowGpuStats(&show_gpu_stats);
    ShowCapturedFrame(&show_vram);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

static void imgui_event(const SDL_Event* event) {
    ImGui_ImplSDL3_ProcessEvent(event);
}

static void imgui_init(SDL_Window* window, SDL_GLContext glContext) {
    imgui_context = ImGui::CreateContext(nullptr);
    ImGui_ImplSDL3_InitForOpenGL(window, glContext);
    ImGui_ImplOpenGL3_Init("#version 330 core");
}

static void imgui_destroy() {
    if (vram_texture) {
        glDeleteTextures(1, &vram_texture);
        vram_texture = 0;
    }
    if (imgui_context) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext(imgui_context);
        imgui_context = nullptr;
    }
}

extern "C" int psx_main();
int main() {
    Psyz_OverlayInit_SDL3GL(imgui_init);
    Psyz_OverlayEvent_SDL3(imgui_event);
    Psyz_OverlayFrameCB(imgui_frame);
    Psyz_OverlayDestroyCB(imgui_destroy);

    return psx_main();
}
