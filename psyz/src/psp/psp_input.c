// Sony PSP input backend: maps the PSP controls to a PS1 pad on port 0.
// Port 1 always reports as disconnected. The single analog stick feeds the
// PS1 left stick when the game selects an analog controller kind.
#include <pspctrl.h>
#include <psyz.h>
#include <psyz/log.h>
#include <libetc.h>
#include <string.h>

void MyPadInit(int mode) {
    (void)mode;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
}

static u_long ReadButtons(unsigned int keys) {
    u_long pressed = 0;
    if (keys & PSP_CTRL_TRIANGLE) {
        pressed |= PADRup;
    }
    if (keys & PSP_CTRL_CROSS) {
        pressed |= PADRdown;
    }
    if (keys & PSP_CTRL_SQUARE) {
        pressed |= PADRleft;
    }
    if (keys & PSP_CTRL_CIRCLE) {
        pressed |= PADRright;
    }
    if (keys & PSP_CTRL_UP) {
        pressed |= PADLup;
    }
    if (keys & PSP_CTRL_DOWN) {
        pressed |= PADLdown;
    }
    if (keys & PSP_CTRL_LEFT) {
        pressed |= PADLleft;
    }
    if (keys & PSP_CTRL_RIGHT) {
        pressed |= PADLright;
    }
    if (keys & PSP_CTRL_LTRIGGER) {
        pressed |= PADL1;
    }
    if (keys & PSP_CTRL_RTRIGGER) {
        pressed |= PADR1;
    }
    if (keys & PSP_CTRL_START) {
        pressed |= PADstart;
    }
    if (keys & PSP_CTRL_SELECT) {
        pressed |= PADselect;
    }
    return pressed;
}

void Psyz_PadsPoll(void) {
    char frame[PSYZ_PAD_BUF_LEN];
    SceCtrlData pad;

    memset(&pad, 0, sizeof(pad));
    pad.Lx = 0x80;
    pad.Ly = 0x80;
    sceCtrlPeekBufferPositive(&pad, 1);

    // see sdl3_common.h BuildPadFrame: PSY-Q's high byte (d-pad,
    // Start/Select) belongs in wire byte 2 and the low byte (face buttons,
    // L1/R1) in wire byte 3, both active-low
    u_long pressed = ReadButtons(pad.Buttons);
    PsyzControllerKind kind = Psyz_PadsSetKind(0, 0, PSYZ_CTRL_QUERY_KIND);
    memset(frame, 0, sizeof(frame));
    frame[0] = 0x00;
    frame[1] = (char)kind;
    switch (kind) {
    case PSYZ_CTRL_ANALOG_PAD:
    case PSYZ_CTRL_ANALOG_STICK:
        frame[2] = (char)(~(pressed >> 8) & 0xFF);
        frame[3] = (char)(~pressed & 0xFF);
        // the PSP has one stick: it drives the PS1 left stick, the right
        // one stays centered
        frame[4] = (char)0x80;
        frame[5] = (char)0x80;
        frame[6] = (char)pad.Lx;
        frame[7] = (char)pad.Ly;
        break;
    case PSYZ_CTRL_DIGITAL_PAD:
        frame[2] = (char)(~(pressed >> 8) & 0xFF);
        frame[3] = (char)(~pressed & 0xFF);
        break;
    case PSYZ_CTRL_DISCONNECTED:
        memset(frame, 0xFF, sizeof(frame));
        break;
    default:
        LOG_ONCE("unsupported controller kind on the PSP, using digital");
        frame[1] = (char)PSYZ_CTRL_DIGITAL_PAD;
        frame[2] = (char)(~(pressed >> 8) & 0xFF);
        frame[3] = (char)(~pressed & 0xFF);
        break;
    }
    Psyz_PadsSet(0, frame, sizeof(frame));

    memset(frame, 0xFF, sizeof(frame));
    Psyz_PadsSet(1, frame, sizeof(frame));
}
