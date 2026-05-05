// FONT EXAMPLE
// Displays text on the screen using the built in GPU routines
// NOTE: This example uses double buffering.

#include <libgpu.h>
#include <libgs.h>
#include <libgte.h>
#include <libetc.h>
#include <stdio.h>
#include <stdlib.h>

#define IS_PAL

#define OT_LENGTH 1       // the ordertable length
#define PACKETMAX 18      // the maximum number of objects on the screen
#define SCREEN_WIDTH 256  // screen width
#define SCREEN_HEIGHT 240 // screen height (240 NTSC, 256 PAL)

GsOT myOT[2];                         // ordering table header
GsOT_TAG myOT_TAG[2][1 << OT_LENGTH]; // ordering table unit
PACKET GPUPacketArea[2][PACKETMAX];   // GPU packet data

u_long _ramsize = 0x00200000;   // force 2 megabytes of RAM
u_long _stacksize = 0x00004000; // force 16 kilobytes of stack

static short CurrentBuffer = 0; // holds the current buffer number

// inits the GPU
static void initGraphics(int is_pal) {
    if (is_pal) {
        SetVideoMode(MODE_PAL);
        ResetGraph(0);
        GsInitGraph(SCREEN_WIDTH, SCREEN_HEIGHT, GsINTER | GsOFSGPU, 1, 0);
    } else {
        SetVideoMode(MODE_NTSC);
        ResetGraph(0);
        GsInitGraph(SCREEN_WIDTH, SCREEN_HEIGHT, GsNONINTER | GsOFSGPU, 1, 0);
    }

    // tell the GPU to draw from the top left coordinates of the framebuffer
    GsDefDispBuff(0, 0, 0, SCREEN_HEIGHT);

    // init the ordertables
    myOT[0].length = OT_LENGTH;
    myOT[1].length = OT_LENGTH;
    myOT[0].org = myOT_TAG[0];
    myOT[1].org = myOT_TAG[1];

    // clear the ordertables
    GsClearOt(0, 0, &myOT[0]);
    GsClearOt(0, 0, &myOT[1]);
}

// updates the GPU (IE: VRAM/SGRAM/framebuffer)
static void display() {
    // refresh the font
    FntFlush(-1);

    // get the current buffer
    CurrentBuffer = GsGetActiveBuff();

    // setup the packet workbase
    GsSetWorkBase((PACKET*)GPUPacketArea[CurrentBuffer]);

    // clear the ordering table
    GsClearOt(0, 0, &myOT[CurrentBuffer]);

    // wait for all drawing to finish
    DrawSync(0);

    // wait for v_blank interrupt
    VSync(0);

    // flip the double buffers
    GsSwapDispBuff();

    // clear the ordering table with a background color (R,G,B)
    GsSortClear(50, 50, 50, &myOT[CurrentBuffer]);

    // draw the ordering table
    GsDrawOt(&myOT[CurrentBuffer]);
}

int main() {
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
