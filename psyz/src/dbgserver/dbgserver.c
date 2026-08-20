// barebone HTTP 1.1 debug server

// winsock2.h must be included before psyz.h (via psyz/types.h) so its
// u_char/u_short/u_long typedefs win over the BSD-style ones in types.h
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET dbg_socket_t;
#define DBG_INVALID_SOCKET INVALID_SOCKET
#define dbg_closesocket closesocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
typedef int dbg_socket_t;
#define DBG_INVALID_SOCKET (-1)
#define dbg_closesocket close
#endif

#include <psyz.h>
#include <common.h>
#include <psyz/video.h>
#include <psyz/input.h>
#include <psyz/system.h>
#include <psyz/log.h>
#include <SDL3/SDL.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#define STBI_WRITE_NO_STDIO
#include "stb_image_write.h"

#undef open
#undef close
#undef read
#undef write

#define DBG_MAX_REQUEST 4096
#define DBG_MAX_SEGMENTS 64
#define DBG_RECV_TIMEOUT_MS 1000
#define DBG_EXEC_TIMEOUT_MS 500

#if defined(_WIN32)
#define DBG_PLATFORM_NAME "windows"
#elif defined(__APPLE__)
#define DBG_PLATFORM_NAME "macos"
#else
#define DBG_PLATFORM_NAME "linux"
#endif

typedef enum {
    DBG_CMD_STATS,
    DBG_CMD_SCREENSHOT,
    DBG_CMD_VRAM,
    DBG_CMD_CONFIG_GET,
    DBG_CMD_CONFIG_SET,
    DBG_CMD_INPUT_QUEUE,
    DBG_CMD_INPUT_CLEAR,
} DbgCommandKind;

typedef struct {
    int set_vsync;
    int vsync;

    int set_dither;
    int dither;

    int set_aspect;
    int aspect;

    int set_internal_resolution;
    unsigned internal_resolution;
} DbgConfigArgs;

typedef struct {
    int port;
    uint16_t mask;
    int frames;
} DbgInputArgs;

typedef struct {
    DbgCommandKind kind;
    union {
        DbgConfigArgs config;
        DbgInputArgs input;
    } args;

    int ok;
    unsigned char* frame_pixels;
    int frame_w, frame_h;
    PsyzVideoStats stats;
    char json[1024];
} DbgCommand;

typedef struct {
    uint16_t mask;
    int frames_left;
} InputSegment;

typedef struct {
    InputSegment segments[DBG_MAX_SEGMENTS];
    int head, count;
} InputFifo;

static InputFifo g_input_fifo[2];
static unsigned long long g_total_frames;

static int InputFifoPush(int port, uint16_t mask, int frames) {
    if (port < 0 || port > 1 || frames <= 0) {
        return -1;
    }
    InputFifo* fifo = &g_input_fifo[port];
    if (fifo->count >= DBG_MAX_SEGMENTS) {
        return -1;
    }
    int idx = (fifo->head + fifo->count) % DBG_MAX_SEGMENTS;
    fifo->segments[idx].mask = mask;
    fifo->segments[idx].frames_left = frames;
    fifo->count++;
    return 0;
}

static void InputFifoClear(int port) {
    g_input_fifo[port].head = 0;
    g_input_fifo[port].count = 0;
}

static void BuildInjectedFrame(int port, uint16_t mask, char* buf) {
    memset(buf, 0, PSYZ_PAD_BUF_LEN);
    PsyzControllerKind kind = Psyz_PadsSetKind(port, 0, PSYZ_CTRL_QUERY_KIND);
    if (kind == PSYZ_CTRL_DISCONNECTED || kind == PSYZ_CTRL_ERROR) {
        memset(buf, 0xFF, PSYZ_PAD_BUF_LEN);
        return;
    }
    buf[0] = 0x00;
    buf[1] = (char)kind;
    buf[2] = (char)(~(mask >> 8) & 0xFF);
    buf[3] = (char)(~mask & 0xFF);
    if (kind != PSYZ_CTRL_DIGITAL_PAD) {
        buf[4] = buf[5] = buf[6] = buf[7] = (char)0x80;
    }
}

static void DbgFrameTick(void) {
    g_total_frames++;
    for (int port = 0; port < 2; port++) {
        InputFifo* fifo = &g_input_fifo[port];
        if (fifo->count == 0) {
            continue;
        }
        InputSegment* seg = &fifo->segments[fifo->head];
        char frame[PSYZ_PAD_BUF_LEN];
        BuildInjectedFrame(port, seg->mask, frame);
        Psyz_PadsSet(port, frame, sizeof(frame));
        seg->frames_left--;
        if (seg->frames_left <= 0) {
            fifo->head = (fifo->head + 1) % DBG_MAX_SEGMENTS;
            fifo->count--;
        }
    }
}

typedef struct {
    char method[8];
    char path[256];
    char query[512];
} HttpRequest;

static int ParseRequestLine(const char* req, int req_len, HttpRequest* out) {
    const char* line_end = memchr(req, '\r', req_len);
    if (!line_end) {
        return -1;
    }
    const char* sp1 = memchr(req, ' ', line_end - req);
    if (!sp1) {
        return -1;
    }
    int method_len = (int)(sp1 - req);
    if (method_len >= (int)sizeof(out->method)) {
        return -1;
    }
    memcpy(out->method, req, method_len);
    out->method[method_len] = 0;

    const char* path_start = sp1 + 1;
    const char* sp2 = memchr(path_start, ' ', line_end - path_start);
    if (!sp2) {
        return -1;
    }
    int full_path_len = (int)(sp2 - path_start);
    const char* qmark = memchr(path_start, '?', full_path_len);
    int path_len = qmark ? (int)(qmark - path_start) : full_path_len;
    if (path_len >= (int)sizeof(out->path)) {
        return -1;
    }
    memcpy(out->path, path_start, path_len);
    out->path[path_len] = 0;

    out->query[0] = 0;
    if (qmark) {
        int query_len = (int)(sp2 - (qmark + 1));
        if (query_len >= (int)sizeof(out->query)) {
            query_len = (int)sizeof(out->query) - 1;
        }
        memcpy(out->query, qmark + 1, query_len);
        out->query[query_len] = 0;
    }
    return 0;
}

static int QueryGet(
    const char* query, const char* key, char* out, int out_len) {
    int key_len = (int)strlen(key);
    const char* p = query;
    while (*p) {
        const char* eq = strchr(p, '=');
        const char* amp = strchr(p, '&');
        const char* seg_end = amp ? amp : p + strlen(p);
        if (eq && eq < seg_end && (eq - p) == key_len &&
            memcmp(p, key, key_len) == 0) {
            const char* v = eq + 1;
            int i = 0;
            while (v < seg_end && i < out_len - 1) {
                if (*v == '%' && v + 2 < seg_end) {
                    char hex[3] = {v[1], v[2], 0};
                    out[i++] = (char)strtol(hex, NULL, 16);
                    v += 3;
                } else if (*v == '+') {
                    out[i++] = ' ';
                    v++;
                } else {
                    out[i++] = *v++;
                }
            }
            out[i] = 0;
            return 0;
        }
        if (!amp) {
            break;
        }
        p = amp + 1;
    }
    return -1;
}

static int SendAll(dbg_socket_t sock, const char* buf, int len) {
    int sent = 0;
    while (sent < len) {
        int n = (int)send(sock, buf + sent, len - sent, 0);
        if (n <= 0) {
            return -1;
        }
        sent += n;
    }
    return 0;
}

static int SendF(dbg_socket_t sock, const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) {
        return -1;
    }
    if (n >= (int)sizeof(buf)) {
        n = (int)sizeof(buf) - 1; // truncated; send what fits
    }
    return SendAll(sock, buf, n);
}

static int SendHeader(dbg_socket_t sock, int status, const char* content_type) {
    return SendF(sock,
                 "HTTP/1.1 %d %s\r\n"
                 "Content-Type: %s\r\n"
                 "Transfer-Encoding: chunked\r\n"
                 "Connection: close\r\n"
                 "\r\n",
                 status, status == 200 ? "OK" : "Error", content_type);
}

static int SendChunk(dbg_socket_t sock, const void* data, int len) {
    if (len <= 0) {
        return 0; // a zero-length chunk would terminate the response
    }
    if (SendF(sock, "%x\r\n", (unsigned)len) != 0 ||
        SendAll(sock, (const char*)data, len) != 0) {
        return -1;
    }
    return SendAll(sock, "\r\n", 2);
}

static int SendEnd(dbg_socket_t sock) { return SendAll(sock, "0\r\n\r\n", 5); }

static int SendChunkF(dbg_socket_t sock, const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) {
        return -1;
    }
    if (n >= (int)sizeof(buf)) {
        n = (int)sizeof(buf) - 1;
    }
    return SendChunk(sock, buf, n);
}

static void RespondRaw(dbg_socket_t sock, int status, const char* content_type,
                       const void* body, int body_len) {
    if (SendHeader(sock, status, content_type) != 0) {
        return;
    }
    if (SendChunk(sock, body, body_len) != 0) {
        return;
    }
    SendEnd(sock);
}

static void RespondJson(dbg_socket_t sock, const char* json) {
    RespondRaw(sock, 200, "application/json", json, (int)strlen(json));
}

static void RespondError(dbg_socket_t sock, int status, const char* msg) {
    if (SendHeader(sock, status, "application/json") != 0) {
        return;
    }
    if (SendChunkF(sock, "{\"error\":\"%s\"}", msg) != 0) {
        return;
    }
    SendEnd(sock);
}

typedef struct {
    unsigned char* buf;
    int len, cap;
} PngWriteCtx;

static void PngWriteCb(void* context, void* data, int size) {
    PngWriteCtx* ctx = (PngWriteCtx*)context;
    if (ctx->len + size > ctx->cap) {
        ctx->len = -1;
        return;
    }
    memcpy(ctx->buf + ctx->len, data, size);
    ctx->len += size;
}

// pixel format must be RGB888. The output buffer is allocated once from the
// worst-case deflate expansion (stored blocks: 5 bytes of header per 64KB,
// plus one filter byte per scanline) so PngWriteCb never has to grow it.
static int DbgEncodePng(const unsigned char* pixels, int w, int h,
                        unsigned char** out_png, int* out_len) {
    int raw = w * h * 3 + h;
    int cap = raw + raw / 1024 + 4096;
    PngWriteCtx ctx = {0};
    ctx.buf = (unsigned char*)malloc(cap);
    if (!ctx.buf) {
        return -1;
    }
    ctx.cap = cap;
    if (!stbi_write_png_to_func(PngWriteCb, &ctx, w, h, 3, pixels, w * 3) ||
        ctx.len < 0) {
        free(ctx.buf);
        return -1;
    }
    *out_png = ctx.buf;
    *out_len = ctx.len;
    return 0;
}

// name <-> enum tables for the video config modes. The first entry is the
// fallback ModeName reports for any value not listed.
typedef struct {
    const char* name;
    int value;
} ModeEntry;

static const ModeEntry g_vsync_modes[] = {
    {"auto", PSYZ_VSYNC_AUTO},
    {"on", PSYZ_VSYNC_ON},
    {"off", PSYZ_VSYNC_OFF},
    {"limitless", PSYZ_VSYNC_LIMITLESS},
};
static const ModeEntry g_dither_modes[] = {
    {"auto", PSYZ_DITHER_AUTO},
    {"off", PSYZ_DITHER_OFF},
};
static const ModeEntry g_aspect_modes[] = {
    {"display", PSYZ_ASPECT_DISPLAY},
    {"square", PSYZ_ASPECT_SQUARE},
};

static const char* ModeName(const ModeEntry* table, int count, int value) {
    for (int i = 0; i < count; i++) {
        if (table[i].value == value) {
            return table[i].name;
        }
    }
    return table[0].name;
}

static int ModeFromName(
    const ModeEntry* table, int count, const char* s, int* out) {
    for (int i = 0; i < count; i++) {
        if (!strcmp(s, table[i].name)) {
            *out = table[i].value;
            return 0;
        }
    }
    return -1;
}

static void CmdStats(DbgCommand* cmd) {
    PsyzVideoStats stats;
    if (Psyz_VideoStats(&stats) != 0) {
        cmd->ok = 0;
        return;
    }
    cmd->stats = stats;
    cmd->ok = 1;
}

static void CmdCapture(DbgCommand* cmd, unsigned char* (*alloc)(int*, int*)) {
    cmd->frame_pixels = alloc(&cmd->frame_w, &cmd->frame_h);
    cmd->ok = cmd->frame_pixels != NULL;
}

static void FormatConfigJson(char* out, int out_len) {
    PsyzSize disp = Psyz_VideoGetDisplaySize();
    snprintf(
        out, out_len,
        "{\"vsync\":\"%s\",\"dither\":\"%s\",\"aspect\":\"%s\",\n"
        "\"internal_resolution\":%u,\"display_size\":{\"w\":%d,\"h\":%d}}\n",
        ModeName(g_vsync_modes, LEN(g_vsync_modes), Psyz_VideoGetVsyncMode()),
        ModeName(
            g_dither_modes, LEN(g_dither_modes), Psyz_VideoGetDitheringMode()),
        ModeName(
            g_aspect_modes, LEN(g_aspect_modes), Psyz_VideoGetAspectMode()),
        Psyz_VideoGetInternalResolution(), disp.w, disp.h);
}

static void CmdConfigGet(DbgCommand* cmd) {
    FormatConfigJson(cmd->json, sizeof(cmd->json));
    cmd->ok = 1;
}

static void CmdConfigSet(DbgCommand* cmd) {
    DbgConfigArgs* a = &cmd->args.config;
    if (a->set_vsync && Psyz_VideoSetVsyncMode((PsyzVsyncMode)a->vsync) != 0) {
        cmd->ok = 0;
        return;
    }
    if (a->set_dither &&
        Psyz_VideoSetDitheringMode((PsyzDitherMode)a->dither) != 0) {
        cmd->ok = 0;
        return;
    }
    if (a->set_aspect &&
        Psyz_VideoSetAspectMode((PsyzAspectMode)a->aspect) != 0) {
        cmd->ok = 0;
        return;
    }
    if (a->set_internal_resolution &&
        Psyz_VideoSetInternalResolution(a->internal_resolution) != 0) {
        cmd->ok = 0;
        return;
    }
    FormatConfigJson(cmd->json, sizeof(cmd->json));
    cmd->ok = 1;
}

static void CmdInputQueue(DbgCommand* cmd) {
    DbgInputArgs* a = &cmd->args.input;
    if (InputFifoPush(a->port, a->mask, a->frames) != 0) {
        cmd->ok = 0;
        return;
    }
    snprintf(cmd->json, sizeof(cmd->json),
             "{\"queued_segments\":%d,\"current_frame\":%llu}\n",
             g_input_fifo[a->port].count, g_total_frames);
    cmd->ok = 1;
}

static void CmdInputClear(DbgCommand* cmd) {
    InputFifoClear(0);
    InputFifoClear(1);
    char release[PSYZ_PAD_BUF_LEN];
    for (int port = 0; port < 2; port++) {
        BuildInjectedFrame(port, 0, release);
        Psyz_PadsSet(port, release, sizeof(release));
    }
    strcpy(cmd->json, "{\"ok\":true}");
    cmd->ok = 1;
}

static void DbgDispatchCommand(DbgCommand* cmd) {
    switch (cmd->kind) {
    case DBG_CMD_STATS:
        CmdStats(cmd);
        break;
    case DBG_CMD_SCREENSHOT:
        CmdCapture(cmd, Psyz_VideoAllocCapturedFrame);
        break;
    case DBG_CMD_VRAM:
        CmdCapture(cmd, Psyz_VideoAllocVramDump);
        break;
    case DBG_CMD_CONFIG_GET:
        CmdConfigGet(cmd);
        break;
    case DBG_CMD_CONFIG_SET:
        CmdConfigSet(cmd);
        break;
    case DBG_CMD_INPUT_QUEUE:
        CmdInputQueue(cmd);
        break;
    case DBG_CMD_INPUT_CLEAR:
        CmdInputClear(cmd);
        break;
    }
}

static SDL_Mutex* g_queue_mutex;
static SDL_Semaphore* g_done_sem;
static DbgCommand* g_pending_cmd;

// Runs `cmd` on the game thread, blocking until it completes.
// Returns -1 if game is not responding.
static int ExecOrTimeout(DbgCommand* cmd, dbg_socket_t sock) {
    SDL_LockMutex(g_queue_mutex);
    g_pending_cmd = cmd;
    SDL_UnlockMutex(g_queue_mutex);
    if (!SDL_WaitSemaphoreTimeout(g_done_sem, DBG_EXEC_TIMEOUT_MS)) {
        SDL_LockMutex(g_queue_mutex);
        g_pending_cmd = NULL;
        SDL_UnlockMutex(g_queue_mutex);
        RespondError(sock, 503, "VSync callback not being kicked");
        return -1;
    }
    return 0;
}

static PsyzVSyncCb g_prev_vsync_cb;
static void DbgFrameHook(void) {
    DbgFrameTick();

    SDL_LockMutex(g_queue_mutex);
    DbgCommand* cmd = g_pending_cmd;
    g_pending_cmd = NULL;
    SDL_UnlockMutex(g_queue_mutex);

    if (cmd) {
        DbgDispatchCommand(cmd);
        SDL_SignalSemaphore(g_done_sem);
    }

    if (g_prev_vsync_cb) {
        g_prev_vsync_cb();
    }
}

typedef struct {
    const char* name;
    uint16_t mask;
} ButtonEntry;

static const ButtonEntry g_buttons[] = {
    {"select", 1 << 8},   {"l3", 1 << 9},     {"r3", 1 << 10},
    {"start", 1 << 11},   {"up", 1 << 12},    {"right", 1 << 13},
    {"down", 1 << 14},    {"left", 1 << 15},  {"l2", 1 << 0},
    {"r2", 1 << 1},       {"l1", 1 << 2},     {"r1", 1 << 3},
    {"triangle", 1 << 4}, {"circle", 1 << 5}, {"cross", 1 << 6},
    {"square", 1 << 7},
};

static uint16_t ParseButtonMask(const char* buttons) {
    uint16_t mask = 0;
    char buf[256];
    strncpy(buf, buttons, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    char* tok = strtok(buf, ",");
    while (tok) {
        for (int i = 0; i < (int)(sizeof(g_buttons) / sizeof(g_buttons[0]));
             i++) {
            if (strcmp(tok, g_buttons[i].name) == 0) {
                mask |= g_buttons[i].mask;
                break;
            }
        }
        tok = strtok(NULL, ",");
    }
    return mask;
}

static void EpMetrics(const HttpRequest* hr, dbg_socket_t sock) {
    (void)hr;
    DbgCommand cmd = {.kind = DBG_CMD_STATS};
    if (ExecOrTimeout(&cmd, sock) != 0) {
        return;
    }
    if (!cmd.ok) {
        RespondError(sock, 503, "stats unavailable");
        return;
    }
    const PsyzVideoStats* stats = &cmd.stats;
    if (SendHeader(sock, 200, "text/plain; version=1.0.0") != 0) {
        return;
    }
    if (SendChunkF(
            sock,
            "# HELP psyz_frame_time_microseconds Last frame time.\n"
            "# TYPE psyz_frame_time_microseconds gauge\n"
            "psyz_frame_time_microseconds %.3f\n"
            "# HELP psyz_draw_time_microseconds Last draw time excluding vsync "
            "wait.\n"
            "# TYPE psyz_draw_time_microseconds gauge\n"
            "psyz_draw_time_microseconds %.3f\n"
            "# HELP psyz_target_frame_time_microseconds Target frame time.\n"
            "# TYPE psyz_target_frame_time_microseconds gauge\n"
            "psyz_target_frame_time_microseconds %.3f\n"
            "# HELP psyz_frames_total Total frames rendered.\n"
            "# TYPE psyz_frames_total counter\n"
            "psyz_frames_total %llu\n"
            "# HELP psyz_using_driver_vsync 1 if using driver VSync, 0 if "
            "using the limiter.\n"
            "# TYPE psyz_using_driver_vsync gauge\n"
            "psyz_using_driver_vsync %d\n",
            stats->last_frame_time_us, stats->last_draw_time_us,
            stats->target_frame_time_us, stats->total_frames,
            stats->using_driver_vsync) != 0) {
        return;
    }
    SendEnd(sock);
}

static void CapturePng(DbgCommandKind kind, dbg_socket_t sock) {
    DbgCommand cmd = {.kind = kind};
    if (ExecOrTimeout(&cmd, sock) != 0) {
        return;
    }
    if (!cmd.ok) {
        RespondError(sock, 503, "frame capture unavailable");
        return;
    }
    unsigned char* png = NULL;
    int png_len = 0;
    int rc = DbgEncodePng(
        cmd.frame_pixels, cmd.frame_w, cmd.frame_h, &png, &png_len);
    free(cmd.frame_pixels);
    if (rc != 0) {
        RespondError(sock, 500, "png encode failed");
        return;
    }
    RespondRaw(sock, 200, "image/png", png, png_len);
    free(png);
}

static void EpScreenshot(const HttpRequest* hr, dbg_socket_t sock) {
    (void)hr;
    CapturePng(DBG_CMD_SCREENSHOT, sock);
}

static void EpVram(const HttpRequest* hr, dbg_socket_t sock) {
    (void)hr;
    CapturePng(DBG_CMD_VRAM, sock);
}

static void EpConfigGet(const HttpRequest* hr, dbg_socket_t sock) {
    (void)hr;
    DbgCommand cmd = {.kind = DBG_CMD_CONFIG_GET};
    if (ExecOrTimeout(&cmd, sock) != 0) {
        return;
    }
    RespondJson(sock, cmd.json);
}

static void EpConfigSet(const HttpRequest* hr, dbg_socket_t sock) {
    DbgCommand cmd = {.kind = DBG_CMD_CONFIG_SET};
    DbgConfigArgs* a = &cmd.args.config;
    const struct {
        const char* key;
        const ModeEntry* table;
        int count;
        int* set_flag;
        int* value;
    } modes[] = {
        {"vsync", g_vsync_modes, LEN(g_vsync_modes), &a->set_vsync, &a->vsync},
        {"dither", g_dither_modes, LEN(g_dither_modes), &a->set_dither,
         &a->dither},
        {"aspect", g_aspect_modes, LEN(g_aspect_modes), &a->set_aspect,
         &a->aspect},
    };

    char val[32];
    for (int i = 0; i < (int)(sizeof(modes) / sizeof(modes[0])); i++) {
        if (QueryGet(hr->query, modes[i].key, val, sizeof(val)) != 0) {
            continue;
        }
        if (ModeFromName(modes[i].table, modes[i].count, val, modes[i].value) !=
            0) {
            char msg[64];
            snprintf(msg, sizeof(msg), "invalid %s value", modes[i].key);
            RespondError(sock, 400, msg);
            return;
        }
        *modes[i].set_flag = 1;
    }

    if (QueryGet(hr->query, "internal_resolution", val, sizeof(val)) == 0) {
        cmd.args.config.set_internal_resolution = 1;
        cmd.args.config.internal_resolution = (unsigned)strtoul(val, NULL, 10);
    }
    if (ExecOrTimeout(&cmd, sock) != 0) {
        return;
    }
    if (!cmd.ok) {
        RespondError(sock, 400, "invalid config value");
        return;
    }
    RespondJson(sock, cmd.json);
}

static void EpInputQueue(const HttpRequest* hr, dbg_socket_t sock) {
    DbgCommand cmd = {.kind = DBG_CMD_INPUT_QUEUE};
    char val[256];
    cmd.args.input.port = 0;
    if (QueryGet(hr->query, "port", val, sizeof(val)) == 0) {
        cmd.args.input.port = atoi(val);
    }
    cmd.args.input.mask = 0;
    if (QueryGet(hr->query, "buttons", val, sizeof(val)) == 0) {
        cmd.args.input.mask = ParseButtonMask(val);
    }
    cmd.args.input.frames = 1;
    if (QueryGet(hr->query, "frames", val, sizeof(val)) == 0) {
        cmd.args.input.frames = atoi(val);
    }
    if (ExecOrTimeout(&cmd, sock) != 0) {
        return;
    }
    if (!cmd.ok) {
        RespondError(sock, 400, "invalid input params");
        return;
    }
    RespondJson(sock, cmd.json);
}

static void EpInputClear(const HttpRequest* hr, dbg_socket_t sock) {
    (void)hr;
    DbgCommand cmd = {.kind = DBG_CMD_INPUT_CLEAR};
    if (ExecOrTimeout(&cmd, sock) != 0) {
        return;
    }
    RespondJson(sock, cmd.json);
}

typedef void (*DbgEndpointFn)(const HttpRequest* hr, dbg_socket_t sock);

// One method of one endpoint. A NULL `fn` means the method is unsupported;
// `description` and `params` are what GET / reports, so the index can never
// drift from what is actually routed. `params` is NULL when the endpoint
// takes no query string.
typedef struct {
    DbgEndpointFn fn;
    const char* description;
    const char* params;
} DbgHandler;

typedef struct {
    const char* path;
    DbgHandler get;
    DbgHandler post;
} DbgRoute;

static void EpRoot(const HttpRequest* hr, dbg_socket_t sock);
static const DbgRoute g_routes[] = {
    {"/", {EpRoot, "this document", NULL}, {NULL, NULL, NULL}},
    {"/metrics",
     {EpMetrics, "prometheus text metrics", NULL},
     {NULL, NULL, NULL}},
    {"/screenshot",
     {EpScreenshot, "capture displayed frame as PNG", NULL},
     {NULL, NULL, NULL}},
    {"/vram",
     {EpVram, "dump full VRAM plane as PNG", NULL},
     {NULL, NULL, NULL}},
    {"/config",
     {EpConfigGet, "read video config", NULL},
     {EpConfigSet, "apply video config",
      "vsync,dither,aspect,internal_resolution"}},
    {"/input",
     {NULL, NULL, NULL},
     {EpInputQueue, "queue scripted input", "port,buttons,frames"}},
    {"/input/clear",
     {NULL, NULL, NULL},
     {EpInputClear, "drop pending input, release buttons", NULL}},
};

#define DBG_ROUTE_COUNT ((int)(sizeof(g_routes) / sizeof(g_routes[0])))

static void EpRoot(const HttpRequest* hr, dbg_socket_t sock) {
    (void)hr;
    if (SendHeader(sock, 200, "application/json") != 0) {
        return;
    }
    if (SendChunkF(
            sock,
            "{\"service\":\"psyz-debug\",\"version\":1,\"platform\":\"%s\",\n"
            "\"capabilities\":{\"internal_resolution\":true},\n"
            "\"endpoints\":[\n",
            DBG_PLATFORM_NAME) != 0) {
        return;
    }

    int written = 0;
    for (int i = 0; i < DBG_ROUTE_COUNT; i++) {
        const DbgRoute* route = &g_routes[i];
        const DbgHandler* handlers[2] = {&route->get, &route->post};
        const char* methods[2] = {"GET", "POST"};
        for (int m = 0; m < 2; m++) {
            const DbgHandler* h = handlers[m];
            if (!h->fn) {
                continue;
            }
            const char* sep = written++ ? ",\n" : "";
            int rc;
            if (h->params) {
                rc = SendChunkF(
                    sock,
                    "%s{\"method\":\"%s\",\"path\":\"%s\","
                    "\"params\":\"%s\",\"description\":\"%s\"}",
                    sep, methods[m], route->path, h->params, h->description);
            } else {
                rc = SendChunkF(sock,
                                "%s{\"method\":\"%s\",\"path\":\"%s\","
                                "\"description\":\"%s\"}",
                                sep, methods[m], route->path, h->description);
            }
            if (rc != 0) {
                return;
            }
        }
    }
    if (SendChunk(sock, "\n]}\n", 4) != 0) {
        return;
    }
    SendEnd(sock);
}

static void DbgHandleRequest(const char* req, int req_len, dbg_socket_t sock) {
    if (req_len > DBG_MAX_REQUEST) {
        RespondError(sock, 413, "request too large");
        return;
    }
    HttpRequest hr;
    if (ParseRequestLine(req, req_len, &hr) != 0) {
        RespondError(sock, 400, "malformed request line");
        return;
    }

    int is_get = !strcmp(hr.method, "GET");
    int is_post = !strcmp(hr.method, "POST");

    for (int i = 0; i < DBG_ROUTE_COUNT; i++) {
        const DbgRoute* route = &g_routes[i];
        if (strcmp(hr.path, route->path) != 0) {
            continue;
        }
        DbgEndpointFn fn = NULL;
        if (is_get) {
            fn = route->get.fn;
        } else if (is_post) {
            fn = route->post.fn;
        }
        if (!fn) {
            RespondError(sock, 405, "method not allowed");
            return;
        }
        fn(&hr, sock);
        return;
    }

    RespondError(sock, 404, "unknown endpoint");
}

static dbg_socket_t g_listen_sock = DBG_INVALID_SOCKET;
static SDL_Thread* g_thread;
static volatile int g_stop_requested;
static int g_bound_port = -1;

static void SetSocketTimeout(dbg_socket_t sock, int ms) {
#ifdef _WIN32
    DWORD timeout = (DWORD)ms;
    setsockopt(
        sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
}

static const char* FindHeaderEnd(const char* buf, int len) {
    for (int i = 0; i + 4 <= len; i++) {
        if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' &&
            buf[i + 3] == '\n') {
            return buf + i;
        }
    }
    return NULL;
}

static void ServeConnection(dbg_socket_t client) {
    static char req_buf[DBG_MAX_REQUEST + 1];
    int total = 0;
    for (;;) {
        if (total >= DBG_MAX_REQUEST) {
            break;
        }
        int n = (int)recv(client, req_buf + total, DBG_MAX_REQUEST - total, 0);
        if (n <= 0) {
            break;
        }
        total += n;
        if (total >= 4 && FindHeaderEnd(req_buf, total)) {
            break;
        }
    }
    if (total <= 0) {
        return;
    }

    DbgHandleRequest(req_buf, total, client);
}

static int AcceptLoop(void* data) {
    (void)data;
    while (!g_stop_requested) {
        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        dbg_socket_t client =
            accept(g_listen_sock, (struct sockaddr*)&addr, &addr_len);
        if (client == DBG_INVALID_SOCKET) {
            if (g_stop_requested) {
                break;
            }
            continue;
        }
        SetSocketTimeout(client, DBG_RECV_TIMEOUT_MS);
        ServeConnection(client);
        dbg_closesocket(client);
    }
    return 0;
}

static int PlatformNetInit(void) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return -1;
    }
#endif
    return 0;
}

static void PlatformNetShutdown(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

int Psyz_DebugServer(int port) {
    if (g_thread) {
        WARNF("debug server already running");
        return -1;
    }
    if (PlatformNetInit() != 0) {
        ERRORF("failed to initialize networking");
        return -1;
    }

    g_listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_listen_sock == DBG_INVALID_SOCKET) {
        ERRORF("failed to create socket");
        PlatformNetShutdown();
        return -1;
    }

    int reuse = 1;
    setsockopt(g_listen_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse,
               sizeof(reuse));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((unsigned short)port);
    if (bind(g_listen_sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        ERRORF("failed to bind debug server to port %d", port);
        dbg_closesocket(g_listen_sock);
        g_listen_sock = DBG_INVALID_SOCKET;
        PlatformNetShutdown();
        return -1;
    }
    if (listen(g_listen_sock, 1) != 0) {
        ERRORF("failed to listen on debug server socket");
        dbg_closesocket(g_listen_sock);
        g_listen_sock = DBG_INVALID_SOCKET;
        PlatformNetShutdown();
        return -1;
    }

    struct sockaddr_in bound = {0};
    socklen_t bound_len = sizeof(bound);
    getsockname(g_listen_sock, (struct sockaddr*)&bound, &bound_len);
    g_bound_port = ntohs(bound.sin_port);

    SetSocketTimeout(g_listen_sock, 100);

    g_queue_mutex = SDL_CreateMutex();
    g_done_sem = SDL_CreateSemaphore(0);

    memset(g_input_fifo, 0, sizeof(g_input_fifo));
    g_total_frames = 0;

    g_stop_requested = 0;
    g_thread = SDL_CreateThread(AcceptLoop, "psyz-dbgserver", NULL);
    if (!g_thread) {
        ERRORF("failed to create debug server thread");
        dbg_closesocket(g_listen_sock);
        g_listen_sock = DBG_INVALID_SOCKET;
        PlatformNetShutdown();
        return -1;
    }

    g_prev_vsync_cb = Psyz_SetVSyncCb(DbgFrameHook);
    INFOF("debug server listening on 127.0.0.1:%d", g_bound_port);
    return g_bound_port;
}

void Psyz_DebugServerStop(void) {
    if (!g_thread) {
        return;
    }
    g_stop_requested = 1;
    Psyz_SetVSyncCb(g_prev_vsync_cb);
    g_prev_vsync_cb = NULL;

    if (g_listen_sock != DBG_INVALID_SOCKET) {
        dbg_closesocket(g_listen_sock);
        g_listen_sock = DBG_INVALID_SOCKET;
    }
    SDL_WaitThread(g_thread, NULL);
    g_thread = NULL;

    SDL_DestroyMutex(g_queue_mutex);
    g_queue_mutex = NULL;
    SDL_DestroySemaphore(g_done_sem);
    g_done_sem = NULL;

    PlatformNetShutdown();
    g_bound_port = -1;
}
