#include <psyz.h>

#include "ztest.h"

#ifdef __psx__
#include <libapi.h>
#include <libetc.h>
#include <libgpu.h>

void psyz_heap_init(void* base, unsigned int size);
extern unsigned long __bss_end[];

// nugget's startup sets up no allocator; everything between .bss and the
// stack is free, keeping 64KB for the stack to grow into.
static void ps1_init(void) {
    const unsigned long stack_top = 0x801FFF00;
    unsigned char* heap = (unsigned char*)__bss_end;
    unsigned int status = *(volatile unsigned int*)0x1F801814;
    SetVideoMode(status & (1u << 20) ? MODE_PAL : MODE_NTSC);
    psyz_heap_init(heap, stack_top - 64 * 1024 - (unsigned long)heap);
}
#endif

int main(int argc, char** argv) {
    int rc;
#ifdef __psx__
    ps1_init();
#endif
#ifdef __PSP__
    Psyz_VideoSetAspectMode(PSYZ_ASPECT_SQUARE);
#endif
#ifdef IS_PPSSPP_EMU
    ztest_add_tag("ppsspp");
#endif
#ifdef __ANDROID__
    // adb shell has no window system or audio session of its own.
    setenv("SDL_VIDEODRIVER", "offscreen", 0);
    setenv("SDL_AUDIO_DRIVER", "dummy", 0);
#endif
    rc = ztest_main(argc, argv);
#ifdef __psx__
    // Leaves the GPU and interrupts quiet so the next upload does not fault.
    ResetGraph(3);
    StopCallback();
#endif
    return rc;
}
