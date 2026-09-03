#include <libcd.h>
#include <libetc.h>
#include <libgpu.h>
#include <libgte.h>
#include <stdio.h>
#include <string.h>

#ifdef PSYZ_PC
#include <psyz.h>
#endif

#define SCREEN_W 320
#define SCREEN_H 240

#define READ_MODE (CdlModeSpeed | CdlModeSize1)

static DISPENV disp;
static DRAWENV draw;

static u_char payload[2048];
static u_char header[12];
static int read_lba[2];
static int read_ok[2];
static u_char read_stat[2];
static u_char play_stat;
static char volume_id[33];
static int music_started;

static u_char drive_status(void) {
    u_char result[8];
    result[0] = 0;
    CdControlB(CdlNop, 0, result);
    return result[0];
}

static void wait_frames(int frames) {
    while (frames-- > 0) {
        VSync(0);
    }
}

static int read_sector(int lba, int slot) {
    CdlLOC loc;
    u_char mode = READ_MODE;
    int tries;

    CdControl(CdlSetmode, &mode, 0);

    CdIntToPos(lba, &loc);
    CdControl(CdlSetloc, (u_char*)&loc, 0);

    for (tries = 0; tries < 60; tries++) {
        if (CdControl(CdlReadN, 0, 0) != 0) {
            break;
        }
        VSync(0);
    }
    for (tries = 0; tries < 2000000; tries++) {
        if (CdReady(1, 0) == CdlDataReady) {
            break;
        }
    }
    if (tries == 2000000) {
        read_ok[slot] = 0;
        read_lba[slot] = -1;
        return 0;
    }

    CdGetSector(header, 3);
    CdGetSector(payload, 0x200);
    read_stat[slot] = drive_status();
    CdControl(CdlPause, 0, 0);

    memcpy((char*)&loc, (char*)header, sizeof(loc));
    read_lba[slot] = CdPosToInt(&loc);
    read_ok[slot] =
        (payload[1] == 'C' && payload[2] == 'D' && payload[3] == '0' &&
         payload[4] == '0' && payload[5] == '1');
    return read_ok[slot];
}

static void play_track(int track) {
    u_char mode = CdlModeDA;
    u_char param = track;
    u_char result[8];
    CdlLOC loc;

    CdControl(CdlSetmode, &mode, 0);
    if (CdControlB(CdlGetTD, &param, result) == 0) {
        return;
    }
    loc.minute = result[1];
    loc.second = result[2];
    loc.sector = 0;
    loc.track = 0;
    CdControl(CdlSetloc, (u_char*)&loc, 0);
    CdControl(CdlPlay, 0, 0);
    music_started = 1;
}

static void draw_report(void) {
    DrawSync(0);
    VSync(0);
    PutDrawEnv(&draw);
    PutDispEnv(&disp);
    FntPrint("PSYZ CD SAMPLE\n\n");
    FntPrint("SECTOR 16 %s LBA=%d STAT=%02x\n", read_ok[0] ? "OK" : "FAIL",
             read_lba[0], read_stat[0]);
    FntPrint("VOLUME  %s\n\n", volume_id);
    FntPrint("CDDA PLAY STAT=%02x %s\n\n", play_stat,
             (play_stat & CdlStatPlay) ? "PLAYING" : "NOT PLAYING");
    FntPrint("SECTOR 17 %s LBA=%d STAT=%02x\n", read_ok[1] ? "OK" : "FAIL",
             read_lba[1], read_stat[1]);
    FntPrint("AFTER READ  %s\n",
             (read_stat[1] & CdlStatPlay) ? "STILL PLAYING (BAD)"
                                          : "MUSIC STOPPED (OK)");
    FntFlush(-1);
}

#ifdef PSYZ_PC
static void print_report(void) {
    printf("SECTOR 16 %s LBA=%d STAT=%02x\n", read_ok[0] ? "OK" : "FAIL",
           read_lba[0], read_stat[0]);
    printf("VOLUME %s\n", volume_id);
    printf("CDDA PLAY STAT=%02x %s\n", play_stat,
           (play_stat & CdlStatPlay) ? "PLAYING" : "NOT PLAYING");
    printf("SECTOR 17 %s LBA=%d STAT=%02x\n", read_ok[1] ? "OK" : "FAIL",
           read_lba[1], read_stat[1]);
    printf("AFTER READ %s\n",
           (read_stat[1] & CdlStatPlay) ? "STILL PLAYING (BAD)"
                                        : "MUSIC STOPPED (OK)");
}
#endif

#ifdef PSYZ_PC
int main(int argc, char** argv) {
    int retry;

    Psyz_CdSetDiskPath(argc > 1 ? argv[1] : "disc/cd_read.cue");
#else
int main(void) {
    int retry;
#endif

    ResetGraph(0);
    SetDefDispEnv(&disp, 0, 0, SCREEN_W, SCREEN_H);
    SetDefDrawEnv(&draw, 0, 0, SCREEN_W, SCREEN_H);
    draw.isbg = 1;
    setRGB0(&draw, 0, 0, 40);
    PutDrawEnv(&draw);
    PutDispEnv(&disp);
    FntLoad(960, 256);
    SetDumpFnt(FntOpen(16, 16, SCREEN_W - 32, SCREEN_H - 32, 0, 512));
    SetDispMask(1);

    for (retry = 0; retry < 16 && CdInit() == 0; retry++) {
        VSync(0);
    }

    if (read_sector(16, 0)) {
        memcpy(volume_id, &payload[40], 32);
        volume_id[32] = 0;
    } else {
        strcpy(volume_id, "<read failed>");
    }
    draw_report();

    play_track(2);
    wait_frames(120);
    play_stat = drive_status();
    draw_report();
    wait_frames(60);

    read_sector(17, 1);
    wait_frames(60);
#ifdef PSYZ_PC
    print_report();
#endif

#ifdef PSYZ_PC
    while (!Psyz_QuitRequested()) {
#else
    while (1) {
#endif
        draw_report();
    }
    return 0;
}
