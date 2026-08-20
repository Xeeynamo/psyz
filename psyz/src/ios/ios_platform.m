#import <UIKit/UIKit.h>

#include <SDL3/SDL.h>
#include <libetc.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "ios_platform.h"

#ifndef PSYZ_IOS_PREF_ORG
#define PSYZ_IOS_PREF_ORG "PSYZ"
#endif

#ifndef PSYZ_IOS_PREF_APP
#define PSYZ_IOS_PREF_APP "Memory Cards"
#endif

static SDL_AtomicInt touch_mask;
static SDL_AtomicInt touch_controls_active;

typedef struct PsyzTouchLayout {
    CGRect dpad;
    CGPoint face_up;
    CGPoint face_right;
    CGPoint face_down;
    CGPoint face_left;
    CGFloat face_radius;
    CGRect l2;
    CGRect l1;
    CGRect r1;
    CGRect r2;
    CGRect select;
    CGRect start;
    CGFloat unit;
} PsyzTouchLayout;

static BOOL PointInCircle(CGPoint p, CGPoint c, CGFloat radius) {
    const CGFloat dx = p.x - c.x;
    const CGFloat dy = p.y - c.y;
    return dx * dx + dy * dy <= radius * radius;
}

@interface PsyzTouchControlsView : UIView
@property(nonatomic, strong) NSMutableSet<UITouch*>* activeTouches;
- (void)resetTouches;
@end

@implementation PsyzTouchControlsView

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.backgroundColor = UIColor.clearColor;
        self.opaque = NO;
        self.contentMode = UIViewContentModeRedraw;
        self.multipleTouchEnabled = YES;
        self.userInteractionEnabled = YES;
        self.autoresizingMask = UIViewAutoresizingFlexibleWidth |
                                UIViewAutoresizingFlexibleHeight;
        self.activeTouches = [NSMutableSet set];
    }
    return self;
}

- (void)safeAreaInsetsDidChange {
    [super safeAreaInsetsDidChange];
    [self setNeedsDisplay];
}

- (PsyzTouchLayout)touchLayout {
    CGRect safe = UIEdgeInsetsInsetRect(self.bounds, self.safeAreaInsets);
    const CGFloat min_dim = MIN(CGRectGetWidth(safe), CGRectGetHeight(safe));
    const CGFloat unit = MAX(46.0, MIN(72.0, min_dim * 0.17));
    const CGFloat edge = unit * 0.20;
    const CGFloat bottom = CGRectGetMaxY(safe) - edge;

    PsyzTouchLayout l = {0};
    l.unit = unit;

    const CGFloat dpad_size = unit * 2.15;
    l.dpad = CGRectMake(CGRectGetMinX(safe) + edge,
                        bottom - dpad_size,
                        dpad_size,
                        dpad_size);

    const CGPoint face_center = {
        CGRectGetMaxX(safe) - edge - unit * 1.05,
        bottom - unit * 1.05,
    };
    const CGFloat face_offset = unit * 0.70;
    l.face_radius = unit * 0.43;
    l.face_up = CGPointMake(face_center.x, face_center.y - face_offset);
    l.face_right = CGPointMake(face_center.x + face_offset, face_center.y);
    l.face_down = CGPointMake(face_center.x, face_center.y + face_offset);
    l.face_left = CGPointMake(face_center.x - face_offset, face_center.y);

    const CGFloat shoulder_w = unit * 1.22;
    const CGFloat shoulder_h = unit * 0.50;
    const CGFloat shoulder_y = CGRectGetMinY(safe) + edge;
    l.l2 = CGRectMake(CGRectGetMinX(safe) + edge, shoulder_y,
                      shoulder_w, shoulder_h);
    l.l1 = CGRectOffset(l.l2, shoulder_w + edge * 0.6, 0);
    l.r2 = CGRectMake(CGRectGetMaxX(safe) - edge - shoulder_w,
                      shoulder_y, shoulder_w, shoulder_h);
    l.r1 = CGRectOffset(l.r2, -(shoulder_w + edge * 0.6), 0);

    const CGFloat center_w = unit * 1.16;
    const CGFloat center_h = unit * 0.46;
    const CGFloat center_gap = unit * 0.24;
    const CGFloat center_y = bottom - center_h;
    const CGFloat center_x = CGRectGetMidX(safe);
    l.select = CGRectMake(center_x - center_gap * 0.5 - center_w,
                          center_y, center_w, center_h);
    l.start = CGRectMake(center_x + center_gap * 0.5,
                         center_y, center_w, center_h);
    return l;
}

- (unsigned int)maskForPoint:(CGPoint)p layout:(PsyzTouchLayout)l {
    unsigned int mask = 0;

    if (CGRectContainsPoint(l.dpad, p)) {
        const CGFloat dx = p.x - CGRectGetMidX(l.dpad);
        const CGFloat dy = p.y - CGRectGetMidY(l.dpad);
        const CGFloat dead = l.unit * 0.18;
        if (dx < -dead) {
            mask |= PADLleft;
        } else if (dx > dead) {
            mask |= PADLright;
        }
        if (dy < -dead) {
            mask |= PADLup;
        } else if (dy > dead) {
            mask |= PADLdown;
        }
    }

    if (PointInCircle(p, l.face_up, l.face_radius)) {
        mask |= PADRup; // triangle
    }
    if (PointInCircle(p, l.face_right, l.face_radius)) {
        mask |= PADRright; // circle
    }
    if (PointInCircle(p, l.face_down, l.face_radius)) {
        mask |= PADRdown; // cross
    }
    if (PointInCircle(p, l.face_left, l.face_radius)) {
        mask |= PADRleft; // square
    }

    if (CGRectContainsPoint(l.l1, p)) {
        mask |= PADn;
    }
    if (CGRectContainsPoint(l.r1, p)) {
        mask |= PADl;
    }
    if (CGRectContainsPoint(l.l2, p)) {
        mask |= PADo;
    }
    if (CGRectContainsPoint(l.r2, p)) {
        mask |= PADm;
    }
    if (CGRectContainsPoint(l.select, p)) {
        mask |= PADk;
    }
    if (CGRectContainsPoint(l.start, p)) {
        mask |= PADh;
    }
    return mask;
}

- (BOOL)pointInControlArea:(CGPoint)p layout:(PsyzTouchLayout)l {
    if (CGRectContainsPoint(l.dpad, p) || CGRectContainsPoint(l.l1, p) ||
        CGRectContainsPoint(l.l2, p) || CGRectContainsPoint(l.r1, p) ||
        CGRectContainsPoint(l.r2, p) || CGRectContainsPoint(l.select, p) ||
        CGRectContainsPoint(l.start, p)) {
        return YES;
    }
    return PointInCircle(p, l.face_up, l.face_radius) ||
           PointInCircle(p, l.face_right, l.face_radius) ||
           PointInCircle(p, l.face_down, l.face_radius) ||
           PointInCircle(p, l.face_left, l.face_radius);
}

- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent*)event {
    (void)event;
    if (self.hidden || self.alpha <= 0.01) {
        return NO;
    }
    return [self pointInControlArea:point layout:[self touchLayout]];
}

- (void)resetTouches {
    [self.activeTouches removeAllObjects];
    SDL_SetAtomicInt(&touch_mask, 0);
    [self setNeedsDisplay];
}

- (void)updateTouchMask {
    unsigned int mask = 0;
    PsyzTouchLayout l = [self touchLayout];
    for (UITouch* touch in self.activeTouches) {
        mask |= [self maskForPoint:[touch locationInView:self] layout:l];
    }
    SDL_SetAtomicInt(&touch_mask, (int)mask);
    [self setNeedsDisplay];
}

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    (void)event;
    [self.activeTouches unionSet:touches];
    [self updateTouchMask];
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    (void)touches;
    (void)event;
    [self updateTouchMask];
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    (void)event;
    [self.activeTouches minusSet:touches];
    [self updateTouchMask];
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    (void)event;
    [self.activeTouches minusSet:touches];
    [self updateTouchMask];
}

static void DrawLabelledControl(CGRect rect, NSString* text, BOOL active,
                                BOOL circle) {
    UIColor* fill = [UIColor colorWithWhite:1.0 alpha:active ? 0.40 : 0.20];
    [fill setFill];
    UIBezierPath* path = circle
                             ? [UIBezierPath bezierPathWithOvalInRect:rect]
                             : [UIBezierPath bezierPathWithRoundedRect:rect
                                                          cornerRadius:10.0];
    [path fill];

    UIFont* font = [UIFont boldSystemFontOfSize:MAX(11.0, rect.size.height * 0.34)];
    NSDictionary* attrs = @{
        NSFontAttributeName : font,
        NSForegroundColorAttributeName : [UIColor colorWithWhite:1.0 alpha:0.85],
    };
    CGSize size = [text sizeWithAttributes:attrs];
    CGPoint origin = CGPointMake(CGRectGetMidX(rect) - size.width * 0.5,
                                 CGRectGetMidY(rect) - size.height * 0.5);
    [text drawAtPoint:origin withAttributes:attrs];
}

- (void)drawRect:(CGRect)rect {
    (void)rect;
    const unsigned int active = (unsigned int)SDL_GetAtomicInt(&touch_mask);
    PsyzTouchLayout l = [self touchLayout];

    const CGFloat third = l.dpad.size.width / 3.0;
    CGRect up = CGRectMake(l.dpad.origin.x + third, l.dpad.origin.y,
                           third, third);
    CGRect down = CGRectOffset(up, 0, third * 2.0);
    CGRect left = CGRectMake(l.dpad.origin.x, l.dpad.origin.y + third,
                             third, third);
    CGRect right = CGRectOffset(left, third * 2.0, 0);
    DrawLabelledControl(up, @"▲", (active & PADLup) != 0, NO);
    DrawLabelledControl(right, @"▶", (active & PADLright) != 0, NO);
    DrawLabelledControl(down, @"▼", (active & PADLdown) != 0, NO);
    DrawLabelledControl(left, @"◀", (active & PADLleft) != 0, NO);

    const CGFloat d = l.face_radius * 2.0;
    DrawLabelledControl(CGRectMake(l.face_up.x - l.face_radius,
                                   l.face_up.y - l.face_radius, d, d),
                        @"△", (active & PADRup) != 0, YES);
    DrawLabelledControl(CGRectMake(l.face_right.x - l.face_radius,
                                   l.face_right.y - l.face_radius, d, d),
                        @"○", (active & PADRright) != 0, YES);
    DrawLabelledControl(CGRectMake(l.face_down.x - l.face_radius,
                                   l.face_down.y - l.face_radius, d, d),
                        @"×", (active & PADRdown) != 0, YES);
    DrawLabelledControl(CGRectMake(l.face_left.x - l.face_radius,
                                   l.face_left.y - l.face_radius, d, d),
                        @"□", (active & PADRleft) != 0, YES);

    DrawLabelledControl(l.l2, @"L2", (active & PADo) != 0, NO);
    DrawLabelledControl(l.l1, @"L1", (active & PADn) != 0, NO);
    DrawLabelledControl(l.r1, @"R1", (active & PADl) != 0, NO);
    DrawLabelledControl(l.r2, @"R2", (active & PADm) != 0, NO);
    DrawLabelledControl(l.select, @"SELECT", (active & PADk) != 0, NO);
    DrawLabelledControl(l.start, @"START", (active & PADh) != 0, NO);
}

@end

static PsyzTouchControlsView* touch_view;

static int IsMemoryCardPath(const char* src) {
    const size_t len = src ? strlen(src) : 0;
    return len >= 5 && src[0] == 'b' && src[1] == 'u' && src[4] == ':';
}

int Psyz_IosAdjustPath(char* dst, const char* src, int maxlen) {
    if (!dst || !src || maxlen <= 0 || !IsMemoryCardPath(src)) {
        return -1;
    }

    char* pref = SDL_GetPrefPath(PSYZ_IOS_PREF_ORG, PSYZ_IOS_PREF_APP);
    if (!pref) {
        return -1;
    }

    char slot[5] = {src[0], src[1], src[2], src[3], '\0'};
    int written = SDL_snprintf(dst, (size_t)maxlen, "%s%s", pref, slot);
    SDL_free(pref);
    if (written < 0 || written >= maxlen) {
        dst[maxlen - 1] = '\0';
        return (int)strlen(dst);
    }

    (void)mkdir(dst, 0755);

    const size_t used = strlen(dst);
    if (used + 1 < (size_t)maxlen) {
        dst[used] = '/';
        dst[used + 1] = '\0';
    }
    if (src[5] != '\0' && src[5] != '*') {
        SDL_strlcat(dst, src + 5, (size_t)maxlen);
    }
    return (int)strlen(dst);
}

void Psyz_IosAttachWindow(SDL_Window* window) {
    if (!window || touch_view) {
        return;
    }

    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    UIWindow* uiwindow = (__bridge UIWindow*)SDL_GetPointerProperty(
        props, SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, NULL);
    if (!uiwindow) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "PSY-Z: could not obtain UIKit window for touch controls");
        return;
    }

    UIView* parent = uiwindow.rootViewController.view;
    if (!parent) {
        parent = uiwindow;
    }

    touch_view = [[PsyzTouchControlsView alloc] initWithFrame:parent.bounds];
    touch_view.hidden = SDL_GetAtomicInt(&touch_controls_active) == 0;
    [parent addSubview:touch_view];
}

void Psyz_IosDetachWindow(void) {
    Psyz_IosResetTouchControls();
    if (touch_view) {
        [touch_view resetTouches];
        [touch_view removeFromSuperview];
        touch_view = nil;
    }
}

void Psyz_IosSetTouchControlsVisible(int visible) {
    SDL_SetAtomicInt(&touch_controls_active, visible ? 1 : 0);
    if (!visible) {
        Psyz_IosResetTouchControls();
    }
    if (touch_view) {
        if (!visible) {
            [touch_view resetTouches];
        }
        touch_view.hidden = visible ? NO : YES;
        [touch_view setNeedsDisplay];
    }
}

int Psyz_IosTouchControlsActive(void) {
    return touch_view && !touch_view.hidden;
}

unsigned int Psyz_IosReadTouchControls(void) {
    return (unsigned int)SDL_GetAtomicInt(&touch_mask);
}

static void SDLCALL ResetTouchViewOnMainThread(void* userdata) {
    (void)userdata;
    if (touch_view) {
        [touch_view resetTouches];
    }
}

void Psyz_IosResetTouchControls(void) {
    SDL_SetAtomicInt(&touch_mask, 0);
    if (!SDL_RunOnMainThread(ResetTouchViewOnMainThread, NULL, false)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "PSY-Z: failed to queue touch reset: %s", SDL_GetError());
    }
}
