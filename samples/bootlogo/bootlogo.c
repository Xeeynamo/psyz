#include <psyz.h>
#include <libetc.h>
#include <libgpu.h>

static int exit_requested = 0;
int endless_or_skip_on_buttons_pressed(int time) {
    int pad = PadRead(0);
    if (pad & PADstart) {
        exit_requested = 1;
        return 1;
    }
    if (pad & (PADRup | PADRdown | PADRleft | PADRright | PADselect)) {
        return 1;
    }
    return 0;
}

int main() {
    PsyzBootlogoConfig config = {0};
    config.width = 640;
    config.height = 480;
    config.skip_cb = endless_or_skip_on_buttons_pressed;
    while (!exit_requested) {
        Psyz_Bootlogo(config);
    }

    ResetGraph(3);
    StopCallback();
    return 0;
}
