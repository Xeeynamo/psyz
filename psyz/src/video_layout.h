// Private header shared by the video backends to resolve the native canvas.
// Units: game pixels for fb/canvas/screen, display pixels for present.

#ifndef PSYZ_VIDEO_LAYOUT_H
#define PSYZ_VIDEO_LAYOUT_H

#include <psyz/types.h>
#include <psyz/video.h>
#if defined(__has_include)
#if __has_include(<sys/param.h>)
#include <sys/param.h>
#endif
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#define VIDEO_LAYOUT_MAX_CANVAS 1024
#define VIDEO_LAYOUT_MAX_REQUEST 8192

typedef struct {
    PsyzSize display; // zero when not known yet
    PsyzSize fb;      // PS1 display mode
    PsyzSize request; // 0x0 for the default, or a size
    PsyzAnchor anchor;
    PsyzAspectMode aspect;
    int range_w;         // PS1 horizontal display range in dots, zero for 256
    PsyzSize max_canvas; // zero for VIDEO_LAYOUT_MAX_CANVAS
    int max_canvas_px;   // canvas area budget, zero for no limit
} VideoLayoutIn;

typedef struct {
    int native;
    PsyzSize canvas;
    PsyzRect screen;  // canvas in game coordinates, fb top-left at 0,0
    PsyzRect present; // canvas on the display
} VideoLayout;

static inline int VideoLayout_IsRequestValid(PsyzSize size, PsyzSize max) {
    if (size.w == 0 && size.h == 0) {
        return 1;
    }
    return size.w >= 1 && size.h >= 1 && size.w <= max.w && size.h <= max.h;
}

static inline int VideoLayout_IsValidAnchor(PsyzAnchor anchor) {
    return (unsigned int)anchor <= PSYZ_ANCHOR_TOP_LEFT;
}

// Display pixels covered by the fb width when one game row is one pixel tall.
// DISPLAY is 4:3 over 256 dots (2560 dot clocks), whatever the line count.
static inline unsigned int VideoLayout_ShownWidth(
    const VideoLayoutIn* in, PsyzSize fb) {
    const unsigned int h = in->range_w > 0 ? in->range_w : 256;
    if (in->aspect == PSYZ_ASPECT_SQUARE) {
        return fb.w;
    }
    return MAX(1, (fb.h * h + 96) / 192);
}

static inline PsyzSize VideoLayout_LimitCanvas(
    const VideoLayoutIn* in, PsyzSize fb, PsyzSize c) {
    const int px = in->max_canvas_px;
    PsyzSize max = in->max_canvas;
    if (max.w <= 0 || max.h <= 0) {
        max.w = VIDEO_LAYOUT_MAX_CANVAS;
        max.h = VIDEO_LAYOUT_MAX_CANVAS;
    }
    c.w = MAX(MIN(c.w, max.w), 1);
    c.h = MAX(MIN(c.h, max.h), 1);
    if (px > 0 && c.w * c.h > px) {
        c.w = MIN(c.w, MAX(fb.w, (px / c.h) & ~7));
        if (c.w * c.h > px) {
            c.h = MAX(1, px / c.w);
        }
    }
    return c;
}

static inline PsyzRect VideoLayout_Present(
    PsyzSize display, PsyzSize area, PsyzSize fit) {
    int m, w = fit.w, h = fit.h;
    if (display.w <= 0 || display.h <= 0) {
        return Psyz_RectFromSize((area.w - w) >> 1, (area.h - h) >> 1, w, h);
    }
    m = MIN(display.w / area.w, display.h / area.h);
    if (m >= 1) {
        w *= m;
        h *= m;
    } else if (display.w * area.h <= display.h * area.w) {
        w = w * display.w / area.w;
        h = h * display.w / area.w;
    } else {
        w = w * display.h / area.h;
        h = h * display.h / area.h;
    }
    return Psyz_RectFromSize((display.w - w) >> 1, (display.h - h) >> 1, w, h);
}

static inline void VideoLayout_Resolve(
    const VideoLayoutIn* in, VideoLayout* out) {
    PsyzSize fb, area, c, fit;
    unsigned int shown;
    int k, ox, oy;
    fb.w = MAX(in->fb.w, 1);
    fb.h = MAX(in->fb.h, 1);
    area = fb;
    c = fb;
    fit = fb;
    out->native = in->request.w != 0;
    if (out->native) {
        shown = VideoLayout_ShownWidth(in, fb);
        area = in->request;
        k = MAX(MIN(area.h / fb.h, (int)(area.w / shown)), 1);
        c.w = (int)((area.w * fb.w / (k * shown) + 7) & ~7);
        c.h = (area.h / k + 7) & ~7;
        c = VideoLayout_LimitCanvas(in, fb, c);
        fit.w = (int)((c.w * k * shown + (fb.w >> 1)) / fb.w);
        fit.h = c.h * k;
    }
    ox = in->anchor & PSYZ_ANCHOR_LEFT ? 0 : (c.w - fb.w) >> 1;
    oy = in->anchor & PSYZ_ANCHOR_TOP ? 0 : (c.h - fb.h) >> 1;
    out->canvas = c;
    out->screen = Psyz_RectFromSize(-ox, -oy, c.w, c.h);
    out->present = VideoLayout_Present(in->display, area, fit);
}

#endif
