// etc_pads -- PS1 controller diagnostic (PSY-Q 4.0 compatible).
// Uses the legacy InitTAP/StartTAP BIOS-backed API from libtap, which
// transparently handles both direct connections and SCPH-1070 multitap
// fan-out (4 slots per port). Buffers carry the raw on-the-wire bytes that
// we decode per known device id. No rumble (would require PSY-Q 4.2 libpad).

#include <psyz.h>
#include <libgpu.h>
#include <libetc.h>
#include <stdio.h>

// psyq libtap entry points. Forward-declared to avoid pulling libapi.h,
// which conflicts with psyz's libapi.h on ioctl/lseek types.
int InitTAP(char* bufA, long lenA, char* bufB, long lenB);
void StartTAP(void);
void ChangeClearPAD(long a);

#define SCREEN_W 320
#define SCREEN_H 240
#define OT_LEN 1
#define OT_SIZE (1 << OT_LEN)

typedef struct DB {
    DRAWENV draw;
    DISPENV disp;
    OT_TYPE ot[OT_SIZE];
} DB;

static DB db[2];
static DB* cdb;
static short cur_buf = 0;

u_long _ramsize = 0x00200000;
u_long _stacksize = 0x00004000;

// InitTAP receive buffers, one per physical port. Buffer layout per psyq:
//
// Direct connection (no multitap, or no device on the port at all):
//   [0]   = online status (0x00 = OK, 0xFF = disconnected)
//   [1]   = device type byte (low nibble of 5Axxh, e.g. 0x41 digital pad)
//   [2..] = halfword 1 LSB, halfword 1 MSB, then halfwords 2..N
//
// Multitap connected:
//   [0]    = status (0x00)
//   [1]    = 0x80  (multitap marker)
//   [2..9]   slot A frame: status, type, then up to 6 payload bytes
//   [10..17] slot B frame
//   [18..25] slot C frame
//   [26..33] slot D frame
static char padbuf[2][34];

typedef enum {
    KIND_NONE,
    KIND_DIGITAL,
    KIND_ANALOG_PAD,   // 5A73
    KIND_ANALOG_STICK, // 5A53
    KIND_DS2,          // 5A79 / 5A7x
    KIND_MOUSE,        // 5A12
    KIND_MULTITAP,     // 5A80 (header only)
    KIND_NEGCON,       // 5A23
    KIND_LIGHTGUN,     // 5A31 / 5A63
    KIND_JOGCON,       // 5AE3
    KIND_KEYBOARD,     // 5A96 / 5AE8
    KIND_CONFIG,       // 5AF3
    KIND_UNKNOWN,
} DeviceKind;

#define PORTS 2
#define SLOTS_PER_PORT 4
#define SLOTS (PORTS * SLOTS_PER_PORT)

typedef struct {
    int present;
    DeviceKind kind;
    u_short id;
    u_char raw[8];
    int raw_len;
    u_short buttons;
    u_short prev_buttons;
    u_char rx, ry, lx, ly;
    signed char mx, my;
    int mx_acc, my_acc;
    int last_active_frame;
    int multitap; // true if this port has a multitap attached
} PortState;

static PortState ports[SLOTS];
static int active_port = -1;
static int frame_counter = 0;

// Index 0..3 = P1 slots A..D, index 4..7 = P2 slots A..D.
static int slot_port(int s) { return s / SLOTS_PER_PORT; }
static int slot_sub(int s) { return s % SLOTS_PER_PORT; }
static int port_base(int p) { return p * SLOTS_PER_PORT; }

static const char* port_label(int s) {
    static char buf[8];
    int port = slot_port(s) + 1;
    int sub = slot_sub(s);
    // Show sub letter only when the port has a multitap attached.
    if (ports[port_base(slot_port(s))].multitap) {
        sprintf(buf, "P%d-%c", port, 'A' + sub);
    } else {
        sprintf(buf, "P%d", port);
    }
    return buf;
}

static const char* kind_name(DeviceKind k) {
    switch (k) {
    case KIND_DIGITAL:
        return "Digital Pad";
    case KIND_ANALOG_PAD:
        return "Analog Pad (red)";
    case KIND_ANALOG_STICK:
        return "Analog Stick (green)";
    case KIND_DS2:
        return "Dualshock 2";
    case KIND_MOUSE:
        return "Mouse";
    case KIND_MULTITAP:
        return "Multitap";
    case KIND_NEGCON:
        return "NegCon";
    case KIND_LIGHTGUN:
        return "Lightgun";
    case KIND_JOGCON:
        return "Jogcon";
    case KIND_KEYBOARD:
        return "Keyboard";
    case KIND_CONFIG:
        return "Config Mode";
    case KIND_NONE:
        return "Disconnected";
    default:
        return "Unknown";
    }
}

static char kind_icon(DeviceKind k) {
    switch (k) {
    case KIND_DIGITAL:
        return 'D';
    case KIND_ANALOG_PAD:
        return 'A';
    case KIND_ANALOG_STICK:
        return 'S';
    case KIND_DS2:
        return '2';
    case KIND_MOUSE:
        return 'M';
    case KIND_MULTITAP:
        return 'T';
    case KIND_NEGCON:
        return 'N';
    case KIND_LIGHTGUN:
        return 'G';
    case KIND_JOGCON:
        return 'J';
    case KIND_KEYBOARD:
        return 'K';
    case KIND_CONFIG:
        return 'C';
    case KIND_NONE:
        return '.';
    default:
        return '?';
    }
}

static DeviceKind kind_from_type(u_char t) {
    switch (t & 0xF0) {
    case 0x10:
        return KIND_MOUSE;
    case 0x20:
        return KIND_NEGCON;
    case 0x30:
        return KIND_LIGHTGUN;
    case 0x40:
        return KIND_DIGITAL;
    case 0x50:
        return KIND_ANALOG_STICK;
    case 0x60:
        return KIND_LIGHTGUN;
    case 0x70:
        if (t == 0x73)
            return KIND_ANALOG_PAD;
        return KIND_DS2;
    case 0x80:
        return KIND_MULTITAP;
    case 0x90:
        return KIND_KEYBOARD;
    case 0xE0:
        if (t == 0xE3)
            return KIND_JOGCON;
        return KIND_KEYBOARD;
    case 0xF0:
        if (t == 0xF3)
            return KIND_CONFIG;
        return KIND_NONE;
    default:
        return KIND_UNKNOWN;
    }
}

// Decode a single 2+payload slot frame at `src` into slot index `s`.
// `cap` is the largest readable size from `src` (8 for multitap sub-slots,
// 34 for a direct connection).
static void decode_slot(int s, const u_char* src, int cap) {
    PortState* st = &ports[s];
    st->prev_buttons = st->buttons;

    if (cap < 2) {
        st->kind = KIND_NONE;
        st->present = 0;
        st->raw_len = 0;
        st->buttons = 0;
        st->id = 0xFFFF;
        return;
    }
    u_char status = src[0];
    u_char type = src[1];

    if (status == 0xFF || (status == 0x00 && type == 0x00)) {
        st->kind = KIND_NONE;
        st->present = 0;
        st->raw_len = 0;
        st->buttons = 0;
        st->id = 0xFFFF;
        return;
    }
    st->id = (u_short)(0x5A00 | type);
    st->kind = kind_from_type(type);
    st->present = 1;

    int payload = ((type & 0x0F) + 1) * 2;
    if (payload > cap - 2)
        payload = cap - 2;
    if (payload < 0)
        payload = 0;
    if (payload > (int)sizeof(st->raw))
        payload = sizeof(st->raw);
    st->raw_len = payload;
    for (int i = 0; i < payload; i++) {
        st->raw[i] = src[2 + i];
    }

    if (st->kind == KIND_DIGITAL || st->kind == KIND_ANALOG_PAD ||
        st->kind == KIND_ANALOG_STICK || st->kind == KIND_DS2) {
        u_short btn_raw = 0;
        if (payload >= 2) {
            btn_raw = (u_short)st->raw[0] | ((u_short)st->raw[1] << 8);
        }
        st->buttons = (u_short)~btn_raw;

        if (payload >= 6 &&
            (st->kind == KIND_ANALOG_PAD || st->kind == KIND_ANALOG_STICK ||
             st->kind == KIND_DS2)) {
            st->rx = st->raw[2];
            st->ry = st->raw[3];
            st->lx = st->raw[4];
            st->ly = st->raw[5];
        } else {
            st->rx = st->ry = st->lx = st->ly = 0x80;
        }
    } else if (st->kind == KIND_MOUSE) {
        u_short btn_raw = 0;
        if (payload >= 2) {
            btn_raw = (u_short)st->raw[0] | ((u_short)st->raw[1] << 8);
        }
        st->buttons = (u_short)~btn_raw;
        if (payload >= 4) {
            st->mx = (signed char)st->raw[2];
            st->my = (signed char)st->raw[3];
        } else {
            st->mx = st->my = 0;
        }
        st->mx_acc += st->mx;
        st->my_acc += st->my;
    } else {
        st->buttons = 0;
    }
}

// Read a port's full 34-byte buffer and fan it out to its 1 or 4 slot states.
static void decode_port_buffer(int port) {
    const u_char* buf = (const u_char*)padbuf[port];
    int base = port_base(port);
    u_char status = buf[0];
    u_char type = buf[1];

    if (status == 0x00 && type == 0x80) {
        // Multitap: parent slot represents the tap itself, sub-slots A..D
        // are 8-byte windows starting at offset 2/10/18/26.
        ports[port].multitap = 1;
        for (int sub = 0; sub < SLOTS_PER_PORT; sub++) {
            decode_slot(base + sub, buf + 2 + sub * 8, 8);
        }
    } else {
        ports[port].multitap = 0;
        decode_slot(base, buf, 34);
        for (int sub = 1; sub < SLOTS_PER_PORT; sub++) {
            PortState* st = &ports[base + sub];
            st->kind = KIND_NONE;
            st->present = 0;
            st->raw_len = 0;
            st->prev_buttons = st->buttons;
            st->buttons = 0;
            st->id = 0xFFFF;
        }
    }
}

static int port_has_input(int p) {
    PortState* st = &ports[p];
    if (!st->present)
        return 0;
    if (st->buttons && st->buttons != st->prev_buttons)
        return 1;
    if (st->kind == KIND_ANALOG_PAD || st->kind == KIND_ANALOG_STICK ||
        st->kind == KIND_DS2) {
        int dx = (int)st->lx - 0x80;
        int dy = (int)st->ly - 0x80;
        int dx2 = (int)st->rx - 0x80;
        int dy2 = (int)st->ry - 0x80;
        if (dx < 0)
            dx = -dx;
        if (dy < 0)
            dy = -dy;
        if (dx2 < 0)
            dx2 = -dx2;
        if (dy2 < 0)
            dy2 = -dy2;
        if (dx > 0x30 || dy > 0x30 || dx2 > 0x30 || dy2 > 0x30)
            return 1;
    }
    if (st->kind == KIND_MOUSE && (st->mx || st->my || st->buttons))
        return 1;
    return 0;
}

static void pick_active(void) {
    for (int s = 0; s < SLOTS; s++) {
        if (ports[s].kind == KIND_MULTITAP)
            continue;
        if (port_has_input(s)) {
            ports[s].last_active_frame = frame_counter;
            active_port = s;
        }
    }
    if (active_port < 0 || !ports[active_port].present ||
        ports[active_port].kind == KIND_MULTITAP) {
        for (int s = 0; s < SLOTS; s++) {
            if (ports[s].present && ports[s].kind != KIND_MULTITAP) {
                active_port = s;
                return;
            }
        }
        active_port = -1;
    }
}

// ---- Rendering -----------------------------------------------------------

static const char* button_name(int bit) {
    switch (bit) {
    case 0:
        return "Select";
    case 1:
        return "L3";
    case 2:
        return "R3";
    case 3:
        return "Start";
    case 4:
        return "Up";
    case 5:
        return "Right";
    case 6:
        return "Down";
    case 7:
        return "Left";
    case 8:
        return "L2";
    case 9:
        return "R2";
    case 10:
        return "L1";
    case 11:
        return "R1";
    case 12:
        return "Triangle";
    case 13:
        return "Circle";
    case 14:
        return "Cross";
    case 15:
        return "Square";
    default:
        return "?";
    }
}

static void draw_slot_strip(void) {
    FntPrint("Slots: ");
    for (int s = 0; s < SLOTS; s++) {
        if (s == SLOTS_PER_PORT)
            FntPrint(" ");
        char c = kind_icon(ports[s].kind);
        if (s == active_port && ports[s].present) {
            FntPrint("[%c]", c);
        } else {
            FntPrint(" %c ", c);
        }
    }
    FntPrint("\n");
}

static void draw_raw(PortState* st) {
    FntPrint("ID  : %04Xh\n", st->id);
    FntPrint("Raw : ");
    for (int i = 0; i < st->raw_len && i < 8; i++) {
        FntPrint("%02X ", st->raw[i]);
    }
    FntPrint("\n");
}

static void draw_buttons(PortState* st) {
    int any = 0;
    FntPrint("Btn :");
    for (int i = 0; i < 16; i++) {
        if (st->buttons & (1 << i)) {
            FntPrint(" %s", button_name(i));
            any = 1;
        }
    }
    if (!any)
        FntPrint(" (none)");
    FntPrint("\n");
}

static void draw_axes(PortState* st) {
    FntPrint(
        "RX=%02X RY=%02X LX=%02X LY=%02X\n", st->rx, st->ry, st->lx, st->ly);
}

static void draw_mouse(PortState* st) {
    FntPrint("Btn :%s%s\n", (st->buttons & MOUSEleft) ? " Left" : "",
             (st->buttons & MOUSEright) ? " Right" : "");
    FntPrint("dX=%4d dY=%4d  cursor=(%d,%d)\n", (int)st->mx, (int)st->my,
             st->mx_acc, st->my_acc);
}

static void draw_view(void) {
    if (active_port < 0) {
        FntPrint("\n\n\n     Waiting for controller...\n");
        return;
    }
    PortState* st = &ports[active_port];
    FntPrint("PORT %s  %s\n", port_label(active_port), kind_name(st->kind));
    draw_raw(st);
    if (st->kind == KIND_MOUSE) {
        draw_mouse(st);
    } else {
        draw_buttons(st);
        if (st->kind == KIND_ANALOG_PAD || st->kind == KIND_ANALOG_STICK ||
            st->kind == KIND_DS2) {
            draw_axes(st);
        }
    }
}

static void draw_help(void) {
    FntPrint("\n");
    FntPrint("Press input on any pad to switch view.\n");
}

// ---- Main loop -----------------------------------------------------------

static void init_graphics(void) {
    ResetGraph(0);
    SetDefDrawEnv(&db[0].draw, 0, 0, SCREEN_W, SCREEN_H);
    SetDefDrawEnv(&db[1].draw, 0, SCREEN_H, SCREEN_W, SCREEN_H);
    SetDefDispEnv(&db[0].disp, 0, SCREEN_H, SCREEN_W, SCREEN_H);
    SetDefDispEnv(&db[1].disp, 0, 0, SCREEN_W, SCREEN_H);
    db[0].draw.isbg = 1;
    db[1].draw.isbg = 1;
    setRGB0(&db[0].draw, 20, 20, 40);
    setRGB0(&db[1].draw, 20, 20, 40);
    SetVideoMode(MODE_NTSC);
    PutDrawEnv(&db[0].draw);
    PutDispEnv(&db[0].disp);
    ClearOTag(db[0].ot, OT_SIZE);
    ClearOTag(db[1].ot, OT_SIZE);
    SetDispMask(1);
    cdb = &db[0];
}

static void display(void) {
    cur_buf ^= 1;
    cdb = &db[cur_buf];
    DrawSync(0);
    VSync(0);
    PutDrawEnv(&cdb->draw);
    PutDispEnv(&cdb->disp);
    ClearOTag(cdb->ot, OT_SIZE);
    FntFlush(-1);
    DrawOTag(&cdb->ot[0]);
}

int main(void) {
    init_graphics();
    FntLoad(960, 256);
    SetDumpFnt(FntOpen(4, 8, 312, 224, 0, 1024));

    for (int s = 0; s < SLOTS; s++) {
        ports[s].rx = ports[s].ry = ports[s].lx = ports[s].ly = 0x80;
        ports[s].id = 0xFFFF;
    }

    InitTAP(padbuf[0], sizeof(padbuf[0]), padbuf[1], sizeof(padbuf[1]));
    StartTAP();
    // Prevent the kernel from auto-clearing the pad buffers in the default
    // ChangeClearPAD(1) state so our reads always see the latest frame.
    ChangeClearPAD(0);

    while (1) {
        for (int p = 0; p < PORTS; p++) {
            decode_port_buffer(p);
        }
        pick_active();
        draw_slot_strip();
        draw_view();
        draw_help();
        display();
        frame_counter++;
    }
    return 0;
}
