// FONT EXAMPLE
// Displays text on the screen using the built-in GPU routines
// NOTE: This example uses double buffering.

#include <psyz.h>
#include <libgpu.h>
#include <libgs.h>
#include <libetc.h>
#include <stdio.h>

#define OT_LENGTH 1       // the ordertable length
#define PACKETMAX 18      // the maximum number of objects on the screen
#define SCREEN_WIDTH 640  // screen width
#define SCREEN_HEIGHT 480 // screen height (240 NTSC, 256 PAL)

#define OTSIZE (1 << OT_LENGTH)

typedef struct DB {
    DRAWENV draw;
    DISPENV disp;
    OT_TYPE ot[OTSIZE];
} DB;

DB db[2];
DB* cdb;

GsOT myOT[2];                         // ordering table header
GsOT_TAG myOT_TAG[2][1 << OT_LENGTH]; // ordering table unit
PACKET GPUPacketArea[2][PACKETMAX];   // GPU packet data

u_long _ramsize = 0x00200000;   // force 2 megabytes of RAM
u_long _stacksize = 0x00004000; // force 16 kilobytes of stack

static short CurrentBuffer = 0; // holds the current buffer number

// #define USE_REVERSE_OT

// inits the GPU
static void initGraphics(int is_pal) {
    SetDefDrawEnv(&db[0].draw, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDrawEnv(&db[1].draw, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDispEnv(&db[0].disp, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDispEnv(&db[1].disp, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    if (is_pal) {
        db[0].disp.isinter = 1;
        db[1].disp.isinter = 1;
        SetVideoMode(MODE_PAL);
    } else {
        SetVideoMode(MODE_NTSC);
    }
    ResetGraph(0);
    PutDrawEnv(&db[0].draw);
    PutDispEnv(&db[0].disp);

#ifdef USE_REVERSE_OT
    ClearOTagR(db[0].ot, OTSIZE);
    ClearOTagR(db[1].ot, OTSIZE);
#else
    ClearOTag(db[0].ot, OTSIZE);
    ClearOTag(db[1].ot, OTSIZE);
#endif

    SetDispMask(1);
}

// updates the GPU (IE: VRAM/SGRAM/framebuffer)
static void display() {
    // flip the double buffers
    cdb = (cdb == &db[0]) ? &db[1] : &db[0];

    // refresh the font
    FntFlush(-1);

    // clear the ordering table
#ifdef USE_REVERSE_OT
    ClearOTagR(cdb->ot, OTSIZE);
#else
    ClearOTag(cdb->ot, OTSIZE);
#endif

    // wait for all drawing to finish
    DrawSync(0);

    // wait for v_blank interrupt
    VSync(0);

    // clear the screen with a background color (R,G,B)
    ClearImage(&cdb->draw.clip, 50, 50, 50);

    // draw the ordering table
#ifdef USE_REVERSE_OT
    DrawOTag(&cdb->ot[OTSIZE - 1]);
#else
    DrawOTag(&cdb->ot[0]);
#endif
}

int psx_main() {
    int nFrame = 0;
    initGraphics(0);

    FntLoad(960, 256); // load the font from the BIOS into the framebuffer
    SetDumpFnt(
        // screen X,Y | max text length X,Y | autmatic
        // background clear 0,1 | max characters
        FntOpen(5, 16, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 512));

    printf("hello world!\n");
    while (1) {
        FntPrint("hello world!\n");
        FntPrint("%d", nFrame++);
        display();
    }
    return 0;
}
