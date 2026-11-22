#include <libetc.h>
#include <libgpu.h>
#include <psyz.h>
#include "ztest.h"

#define ASSERT_FRAME(name)                                                     \
    zexpect_image_eq(name, zimage_frontbuffer(), zimage_r5g5b5_exact)

static void Submit(void* packet) {
    termPrim(packet);
    DrawOTag(packet);
}

static void SetMask(int force, int check) {
    DR_STP command;
    setlen(&command, 1);
    command.code[0] = 0xE6000000 | force | (check << 1);
    Submit(&command);
}

static void Tile(int x, int y, int w, int h, int r, int g, int b, int blend) {
    TILE tile;
    setTile(&tile);
    setXY0(&tile, x, y);
    setWH(&tile, w, h);
    setRGB0(&tile, r, g, b);
    setSemiTrans(&tile, blend);
    Submit(&tile);
}

static void Read(RECT rect, u_short* pixels) {
    StoreImage(&rect, (u_long*)pixels);
    DrawSync(0);
}

static u_short Pixel(int x, int y) {
    u_short pixels[2];
    Read((RECT){x, y, 2, 1}, pixels);
    return pixels[0];
}

ZTEST_SETUP(gpu_mask) {
    DRAWENV draw;
    DISPENV disp;
    RECT all = {0, 0, 1024, 512};
    ResetGraph(0);
    Psyz_VideoSetInternalResolution(1);
    Psyz_VideoSetResolution((PsyzSize){0, 0});
    Psyz_VideoSetDitheringMode(PSYZ_DITHER_OFF);
    SetDefDrawEnv(&draw, 0, 0, 256, 240);
    SetDefDispEnv(&disp, 0, 0, 256, 240);
    draw.dtd = 0;
    PutDrawEnv(&draw);
    PutDispEnv(&disp);
    SetDispMask(1);
    ClearImage(&all, 0, 0, 0);
    SetMask(0, 0);
    DrawSync(0);
}

ZTEST_TEARDOWN(gpu_mask) {
    SetMask(0, 0);
    DrawSync(0);
    Psyz_VideoSetInternalResolution(1);
    Psyz_VideoSetResolution((PsyzSize){0, 0});
}

ZTEST(gpu_mask, force_check_and_clear) {
    SetMask(1, 0);
    Tile(16, 16, 16, 16, 0, 255, 0, 0);
    SetMask(0, 1);
    Tile(24, 16, 16, 16, 255, 0, 0, 0);
    zexpect_u16_eq(0x83E0, Pixel(28, 20));
    zexpect_u16_eq(0x001F, Pixel(36, 20));
    SetMask(0, 0);
    Tile(16, 16, 16, 16, 0, 0, 255, 0);
    zexpect_u16_eq(0x7C00, Pixel(20, 20));
    SetMask(0, 1);
    Tile(16, 16, 16, 16, 255, 0, 0, 0);
    zexpect_u16_eq(0x001F, Pixel(20, 20));
}

ZTEST(gpu_mask, overlapping_primitives_in_one_batch) {
    SetMask(1, 1);
    TILE tiles[3];
    const int xs[] = {16, 24, 28};
    for (int i = 0; i < 3; i++) {
        setTile(&tiles[i]);
        setXY0(&tiles[i], xs[i], 16);
        setWH(&tiles[i], 16, 16);
        setRGB0(
            &tiles[i], i == 1 ? 255 : 0, i == 0 ? 255 : 0, i == 2 ? 255 : 0);
        if (i < 2)
            catPrim(&tiles[i], &tiles[i + 1]);
        else
            termPrim(&tiles[i]);
    }
    DrawOTag((OT_TYPE*)tiles);
    zexpect_u16_eq(0x83E0, Pixel(28, 20));
    zexpect_u16_eq(0x801F, Pixel(36, 20));
    zexpect_u16_eq(0xFC00, Pixel(42, 20));
    ASSERT_FRAME("mask_batch_overlap");
}

ZTEST(gpu_mask, texture_transparency_and_mask_bit) {
    u_short palette[256] = {0, 0x001F, 0x8000, 0x801F};
    RECT clut = {0, 300, 256, 1};
    LoadImage(&clut, (u_long*)palette);
    for (int depth = 0; depth < 3; depth++) {
        u_short source[4] = {0, 0x001F, 0x8000, 0x801F};
        RECT tex = {512, 256, 4, 1};
        if (depth == 0) {
            source[0] = 0x3210;
        } else if (depth == 1) {
            source[0] = 0x0100;
            source[1] = 0x0302;
        }
        LoadImage(&tex, (u_long*)source);
        DrawSync(0);
        for (int force = 0; force < 2; force++) {
            int y = 16 + (depth * 2 + force) * 4;
            SetMask(0, 0);
            Tile(16, y, 4, 1, 0, 255, 0, 0);
            DR_MODE mode;
            SetDrawMode(&mode, 0, 0, GetTPage(depth, 0, 512, 256), NULL);
            Submit(&mode);
            SetMask(force, 0);
            SPRT sprite;
            setSprt(&sprite);
            setShadeTex(&sprite, 1);
            setXY0(&sprite, 16, y);
            setUV0(&sprite, 0, 0);
            setWH(&sprite, 4, 1);
            sprite.clut = GetClut(0, 300);
            Submit(&sprite);
            u_short actual[4];
            Read((RECT){16, y, 4, 1}, actual);
            zexpect_u16_eq(0x03E0, actual[0]);
            zexpect_u16_eq(0x001F | (force << 15), actual[1]);
            zexpect_u16_eq(0x8000, actual[2]);
            zexpect_u16_eq(0x801F, actual[3]);
        }
        SetMask(0, 0);
    }
}

ZTEST(gpu_mask, blend_alpha_is_not_the_mask_bit) {
    const u_short expected[] = {0x0108, 0x0210, 0x0200, 0x0204};
    u_short texels[2] = {0x8010, 0x8010};
    RECT tex = {512, 256, 2, 1};
    LoadImage(&tex, (u_long*)texels);
    for (int textured = 0; textured < 2; textured++) {
        for (int abr = 0; abr < 4; abr++) {
            int y = 16 + (textured * 4 + abr) * 4;
            Tile(16, y, 2, 1, 0, 128, 0, 0);
            DR_MODE mode;
            SetDrawMode(&mode, 0, 0, GetTPage(2, abr, 512, 256), NULL);
            Submit(&mode);
            if (textured) {
                SPRT sprite;
                setSprt(&sprite);
                setSemiTrans(&sprite, 1);
                setShadeTex(&sprite, 1);
                setXY0(&sprite, 16, y);
                setUV0(&sprite, 0, 0);
                setWH(&sprite, 2, 1);
                sprite.clut = 0;
                Submit(&sprite);
            } else {
                Tile(16, y, 2, 1, 128, 0, 0, 1);
            }
            zexpect_u16_eq(expected[abr] | (textured << 15), Pixel(16, y));
            SetMask(0, 1);
            Tile(16, y, 2, 1, 0, 0, 255, 0);
            zexpect_u16_eq(
                textured ? expected[abr] | 0x8000 : 0x7C00, Pixel(16, y));
            SetMask(0, 0);
        }
    }
}

ZTEST(gpu_mask, uploads_and_copies_preserve_protected_halfwords) {
    u_short source[] = {0x001F, 0x801F, 0x001F, 0x801F};
    u_short destination[] = {0x03E0, 0x03E0, 0x83E0, 0x83E0};
    RECT src = {512, 256, 4, 1}, dst = {64, 64, 4, 1};
    for (int copy = 0; copy < 2; copy++) {
        for (int force = 0; force < 2; force++) {
            for (int check = 0; check < 2; check++) {
                SetMask(0, 0);
                LoadImage(&src, (u_long*)source);
                LoadImage(&dst, (u_long*)destination);
                SetMask(force, check);
                if (copy) {
                    MoveImage(&src, dst.x, dst.y);
                } else {
                    LoadImage(&dst, (u_long*)source);
                }
                u_short actual[4];
                Read(dst, actual);
                for (int i = 0; i < 4; i++) {
                    u_short expected = check && (destination[i] & 0x8000)
                                           ? destination[i]
                                           : source[i] | (force << 15);
                    zexpect_u16_eq(expected, actual[i]);
                }
            }
        }
    }
}

ZTEST(gpu_mask, upload_sees_pending_drawing) {
    u_short red[2] = {0x001F, 0x001F};
    RECT dst = {16, 16, 2, 1};
    SetMask(1, 1);
    Tile(16, 16, 2, 1, 0, 255, 0, 0);
    LoadImage(&dst, (u_long*)red);
    zexpect_u16_eq(0x83E0, Pixel(16, 16));
}

ZTEST(gpu_mask, copy_to_same_position_sets_mask) {
    u_short red[2] = {0x001F, 0x001F};
    RECT dst = {16, 16, 2, 1};
    LoadImage(&dst, (u_long*)red);
    SetMask(1, 0);
    MoveImage(&dst, dst.x, dst.y);
    zexpect_u16_eq(0x801F, Pixel(16, 16));
}

ZTEST(gpu_mask, fill_ignores_mask_without_changing_settings) {
    SetMask(1, 0);
    Tile(16, 16, 16, 16, 0, 255, 0, 0);
    SetMask(1, 1);
    // ClearImage also emits E6(0); send a raw GP0(02) to isolate fill behavior.
    struct {
        OT_TYPE tag;
        u_int code[3];
    } fill;
    setlen(&fill, 3);
    fill.code[0] = 0x020000FF;
    fill.code[1] = 16 | (16 << 16);
    fill.code[2] = 16 | (16 << 16);
    Submit(&fill);
    zexpect_u16_eq(0x001F, Pixel(20, 20));
    Tile(16, 16, 16, 16, 0, 0, 255, 0);
    zexpect_u16_eq(0xFC00, Pixel(20, 20));
}

ZTEST(gpu_mask, internal_resolution_and_native_canvas) {
    for (int native = 0; native < 2; native++) {
        Psyz_VideoSetResolution((PsyzSize){native ? 320 : 0, native ? 240 : 0});
        Psyz_VideoSetInternalResolution(2);
        VSync(0);
        SetMask(1, 0);
        Tile(48, 48, 16, 16, 0, 255, 0, 0);
        SetMask(0, 1);
        Tile(48, 48, 16, 16, 255, 0, 0, 0);
        zexpect_u16_eq(0x83E0, Pixel(52, 52));
        u_short red[2] = {0x001F, 0x001F};
        RECT dst = {52, 52, 2, 1};
        LoadImage(&dst, (u_long*)red);
        zexpect_u16_eq(0x83E0, Pixel(52, 52));
        SetMask(0, 0);
    }
}

ZTEST(gpu_mask, reset_clears_mask_settings) {
    SetMask(1, 1);
    Tile(16, 16, 16, 16, 0, 255, 0, 0);
    DrawSync(0);
    Psyz_GpuDisplayCommand(0);
    Tile(16, 16, 16, 16, 255, 0, 0, 0);
    zexpect_u16_eq(0x001F, Pixel(20, 20));
}

ZTEST(gpu_mask, overlapping_copy_applies_masks_row_by_row) {
    u_short initial[] = {
        0x001F, 0x801F, 0x001F, 0x801F, 0x03E0, 0x03E0,
        0x83E0, 0x83E0, 0x7C00, 0xFC00, 0x7C00, 0xFC00,
    };
    u_short expected[] = {
        0x001F, 0x801F, 0x001F, 0x801F, 0x801F, 0x801F,
        0x83E0, 0x83E0, 0x801F, 0xFC00, 0x83E0, 0xFC00,
    };
    RECT rect = {16, 16, 4, 3}, source = {16, 16, 4, 2};
    LoadImage(&rect, (u_long*)initial);
    SetMask(1, 1);
    MoveImage(&source, 16, 17);
    u_short actual[12];
    Read(rect, actual);
    for (int i = 0; i < 12; i++) {
        zexpect_u16_eq(expected[i], actual[i]);
    }
}

ZTEST(gpu_mask, mixed_texture_masks_preserve_primitive_order) {
    const u_short blended[] = {0x0108, 0x0210, 0x0200, 0x0204};
    u_short texels[] = {0x0010, 0x8010, 0, 0x8000};
    RECT texture = {512, 256, 4, 1};
    LoadImage(&texture, (u_long*)texels);
    for (int abr = 0; abr < 4; abr++) {
        int y = 16 + abr * 4;
        SetMask(0, 0);
        Tile(16, y, 4, 1, 0, 128, 0, 0);
        DR_MODE mode;
        SetDrawMode(&mode, 0, 0, GetTPage(2, abr, 512, 256), NULL);
        Submit(&mode);
        SetMask(0, 1);
        SPRT sprite;
        setSprt(&sprite);
        setShadeTex(&sprite, 1);
        setSemiTrans(&sprite, 1);
        setXY0(&sprite, 16, y);
        setUV0(&sprite, 0, 0);
        setWH(&sprite, 4, 1);
        sprite.clut = 0;
        TILE overlay;
        setTile(&overlay);
        setXY0(&overlay, 16, y);
        setWH(&overlay, 4, 1);
        setRGB0(&overlay, 0, 0, 255);
        catPrim(&sprite, &overlay);
        termPrim(&overlay);
        DrawOTag((OT_TYPE*)&sprite);
        u_short actual[4];
        Read((RECT){16, y, 4, 1}, actual);
        zexpect_u16_eq(0x7C00, actual[0]);
        zexpect_u16_eq(blended[abr] | 0x8000, actual[1]);
        zexpect_u16_eq(0x7C00, actual[2]);
        zexpect_u16_eq(abr == 0 ? 0x8100 : 0x8200, actual[3]);
    }
}

ZTEST(gpu_mask, render_target_and_size_changes_preserve_masks) {
    const PsyzSize sizes[] = {{0, 0}, {320, 240}, {480, 272}, {0, 0}};
    const int scales[] = {1, 2, 1, 3};
    SetMask(1, 0);
    Tile(16, 16, 16, 16, 0, 255, 0, 0);
    DrawSync(0);
    for (int i = 0; i < 4; i++) {
        Psyz_VideoSetResolution(sizes[i]);
        Psyz_VideoSetInternalResolution(scales[i]);
        VSync(0);
        SetMask(0, 1);
        Tile(16, 16, 32, 16, 255, 0, 0, 0);
        zexpect_u16_eq(0x83E0, Pixel(20, 20));
        zexpect_u16_eq(0x001F, Pixel(40, 20));
    }
}

ZTEST(gpu_mask, alternating_framebuffers_do_not_share_stencil) {
    Psyz_VideoSetResolution((PsyzSize){320, 240});
    VSync(0);
    for (int i = 0; i < 4; i++) {
        int y = (i & 1) * 256;
        DRAWENV draw;
        DISPENV disp;
        SetDefDrawEnv(&draw, 0, y, 256, 240);
        SetDefDispEnv(&disp, 0, y, 256, 240);
        draw.dtd = 0;
        PutDrawEnv(&draw);
        PutDispEnv(&disp);
        if (i < 2) {
            SetMask(1, 0);
            Tile(16, 16, 16, 16, i == 0 ? 0 : 255, i == 0 ? 255 : 0, 0, 0);
        }
        SetMask(0, 1);
        Tile(16, 16, 32, 16, 0, 0, 255, 0);
        zexpect_u16_eq(i & 1 ? 0x801F : 0x83E0, Pixel(20, y + 20));
        zexpect_u16_eq(0x7C00, Pixel(40, y + 20));
        ASSERT_FRAME(i & 1 ? "mask_framebuffer_1" : "mask_framebuffer_0");
    }
}

ZTEST(gpu_mask, transfers_refresh_the_next_draw_mask) {
    RECT source = {512, 256, 4, 1};
    RECT target = {16, 16, 4, 1};
    u_short uploaded[] = {0x801F, 0x001F, 0x801F, 0x001F};
    u_short copied[] = {0x001F, 0x801F, 0x001F, 0x801F};
    LoadImage(&source, (u_long*)copied);
    for (int native = 0; native < 2; native++) {
        Psyz_VideoSetResolution((PsyzSize){native ? 320 : 0, native ? 240 : 0});
        VSync(0);
        SetMask(0, 0);
        LoadImage(&target, (u_long*)uploaded);
        SetMask(0, 1);
        Tile(16, 16, 4, 1, 0, 255, 0, 0);
        u_short actual[4];
        Read(target, actual);
        for (int i = 0; i < 4; i++)
            zexpect_u16_eq(i & 1 ? 0x03E0 : 0x801F, actual[i]);
        SetMask(0, 0);
        MoveImage(&source, 16, 16);
        SetMask(0, 1);
        Tile(16, 16, 4, 1, 0, 0, 255, 0);
        Read(target, actual);
        for (int i = 0; i < 4; i++)
            zexpect_u16_eq(i & 1 ? 0x801F : 0x7C00, actual[i]);
    }
}

ZTEST(gpu_mask, protected_upload_preserves_native_canvas_overhang) {
    static u_short green[480 * 272], red[480 * 272];
    RECT image = {-80, -16, 480, 272};
    DRAWENV draw;
    DISPENV disp;
    SetDefDrawEnv(&draw, 0, 0, 320, 240);
    SetDefDispEnv(&disp, 0, 0, 320, 240);
    draw.dtd = 0;
    PutDrawEnv(&draw);
    PutDispEnv(&disp);
    for (int i = 0; i < 480 * 272; i++) {
        green[i] = 0x83E0;
        red[i] = 0x001F;
    }
    for (int scale = 1; scale <= 2; scale++) {
        Psyz_VideoSetResolution((PsyzSize){480, 272});
        Psyz_VideoSetInternalResolution(scale);
        VSync(0);
        SetMask(0, 0);
        LoadImage(&image, (u_long*)green);
        SetMask(0, 1);
        LoadImage(&image, (u_long*)red);
        DrawSync(0);
        ASSERT_FRAME(
            scale == 1 ? "mask_canvas_overhang_1x" : "mask_canvas_overhang_2x");
    }
}

ZTEST(gpu_mask, protected_upload_preserves_high_resolution_edges) {
    Psyz_VideoSetResolution((PsyzSize){320, 240});
    Psyz_VideoSetInternalResolution(2);
    VSync(0);
    SetMask(1, 0);
    POLY_F3 triangle;
    setPolyF3(&triangle);
    setXY3(&triangle, 16, 16, 32, 16, 16, 32);
    setRGB0(&triangle, 0, 255, 0);
    Submit(&triangle);
    DrawSync(0);
    u_short red[16 * 16];
    for (int i = 0; i < 16 * 16; i++)
        red[i] = 0x001F;
    RECT rect = {16, 16, 16, 16};
    SetMask(0, 1);
    LoadImage(&rect, (u_long*)red);
    DrawSync(0);
    ASSERT_FRAME("mask_high_resolution_edges");
}
