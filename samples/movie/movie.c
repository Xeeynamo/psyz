#include <libcd.h>
#include <libetc.h>
#include <libgpu.h>
#include <libpress.h>
#include <libspu.h>
#include <stdio.h>
#include <string.h>

#define WIDTH 320
#define HEIGHT 192
#define FRAMES 60
#define RING_SECTORS 32
#define RUN_WORDS 32769
#define STRIP_WORDS (HEIGHT * 12)

static unsigned int ring[RING_SECTORS * 512];
static unsigned int runlevel[2][RUN_WORDS];
static unsigned int pixels[WIDTH / 16 * STRIP_WORDS];
static DISPENV display[2];
static RECT strip;
static volatile int output_busy;
static int slice;

extern volatile int StCdIntrFlag;
void StCdInterrupt(void);

volatile unsigned int bunny_capture[8];

static void output_callback(void) {
    if (StCdIntrFlag) {
        StCdInterrupt();
        StCdIntrFlag = 0;
    }
    LoadImage(&strip, (u_long*)(pixels + slice * STRIP_WORDS));
    if (++slice == WIDTH / 16) {
        output_busy = 0;
        return;
    }
    strip.x += 24;
    DecDCTout((u_long*)(pixels + slice * STRIP_WORDS), STRIP_WORDS);
}

static int next_frame(int slot, unsigned expected) {
    u_long *data, *header;
    unsigned start = VSync(-1);
    while (StGetNext(&data, &header)) {
        if ((unsigned)(VSync(-1) - start) > 600)
            return -1;
        VSync(0);
    }
    StHEADER info = *(StHEADER*)header;
    bunny_capture[4] = info.frameCount;
    bunny_capture[5] = info.width | (info.height << 16);
    int error = info.frameCount != expected || info.width != WIDTH ||
                info.height != HEIGHT;
    if (!error && DecDCTBufSize(data) >= RUN_WORDS)
        error = 1;
    if (!error)
        error = DecDCTvlc(data, (u_long*)runlevel[slot]);
    bunny_capture[6] = error;
    if (error)
        printf("frame expected %u got %u, %ux%u, VLC status %d\n", expected,
               info.frameCount, info.width, info.height, error);
    StFreeRing(data);
    return error ? -1 : 0;
}

static int play(unsigned loop, const CdlLOC* location) {
    CdlFILTER filter = {1, 1, 0};
    CdlATV mix = {128, 0, 128, 0};
    int slot = 0, page = 0;
    DecDCTReset(1);
    DecDCToutCallback(output_callback);
    StSetRing((u_long*)ring, RING_SECTORS);
    StSetStream(1, 1, FRAMES + 1, NULL, NULL);
    CdMix(&mix);
    if (!CdControl(CdlSetfilter, (u_char*)&filter, NULL) ||
        !CdControlB(CdlSeekL, (u_char*)location, NULL))
        return -1;
    bunny_capture[0] = 0x42424e59;
    bunny_capture[1] = loop;
    bunny_capture[2] = 0;
    bunny_capture[3] = 1;
    unsigned started = VSync(-1);
    if (!CdRead2(CdlModeSpeed | CdlModeRT | CdlModeSF | CdlModeStream) ||
        next_frame(slot, 1))
        return -1;
    for (unsigned frame = 1; frame <= FRAMES; ++frame) {
        strip.x = 0;
        strip.y = page * 240 + 24;
        strip.w = 24;
        strip.h = HEIGHT;
        slice = 0;
        output_busy = 1;
        DecDCTReset(1);
        DecDCTin((u_long*)runlevel[slot], 1);
        DecDCTout((u_long*)pixels, STRIP_WORDS);

        if (frame < FRAMES && next_frame(slot ^ 1, frame + 1))
            return -1;
        unsigned wait_start = VSync(-1);
        while (output_busy) {
            if ((unsigned)(VSync(-1) - wait_start) > 120)
                return -1;
        }
        if (DecDCTinSync(0) || DecDCToutSync(0))
            return -1;
        DrawSync(0);
        while ((unsigned)(VSync(-1) - started) < frame * 2)
            VSync(0);
        PutDispEnv(&display[page]);
        bunny_capture[2] = frame;
        VSync(0);
        slot ^= 1;
        page ^= 1;
    }
    bunny_capture[3] = 0;
    CdControlB(CdlPause, NULL, NULL);
    DecDCToutCallback(NULL);
    StUnSetRing();
    DecDCTReset(1);
    return 0;
}

int main(void) {
    ResetGraph(0);
    SetVideoMode(MODE_NTSC);

    DecDCTReset(0);
    RECT clear = {0, 0, 480, 480};
    ClearImage(&clear, 0, 0, 0);
    DrawSync(0);
    for (int page = 0; page < 2; ++page) {
        SetDefDispEnv(&display[page], 0, page * 240, WIDTH, 240);
        display[page].isrgb24 = 1;
    }
    PutDispEnv(&display[0]);
    SetDispMask(1);
    SpuInit();
    SpuCommonAttr common;
    memset((u_char*)&common, 0, sizeof(common));
    common.mask = SPU_COMMON_MVOLL | SPU_COMMON_MVOLR | SPU_COMMON_CDVOLL |
                  SPU_COMMON_CDVOLR | SPU_COMMON_CDMIX;
    common.mvol.left = common.mvol.right = 0x3fff;
    common.cd.volume.left = common.cd.volume.right = 0x7fff;
    common.cd.mix = SPU_ON;
    SpuSetCommonAttr(&common);
    if (!CdInit())
        return 1;
    CdlFILE file;
    if (!CdSearchFile(&file, "\\BUNNY.STR;1")) {
        printf("BUNNY.STR missing\n");
        return 1;
    }
    for (unsigned loop = 1;; ++loop) {
        if (play(loop, &file.pos)) {
            bunny_capture[3] = 0;
            CdControlB(CdlPause, NULL, NULL);
            DecDCToutCallback(NULL);
            StUnSetRing();
            printf("movie playback failed, loop %u frame %u\n", loop,
                   bunny_capture[2]);
            return 1;
        }
        printf("loop %u: %u frames played\n", loop, FRAMES);
    }
    return 0;
}
