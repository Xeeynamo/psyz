/*
 * $PSLibId: Run-time Library Release 4.0$
 */
#if defined(APD_SAVE) || defined(APD_LOAD)
static char* progname = "etc/card/card.cpe";
#endif
/*
 * File:card.c
 * memory card manager Ver. 1.31    970418
 */

#include <psyz.h>
#include <kernel.h>
#include <sys/file.h>
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libetc.h>
#include <libapi.h>
#include <string.h>
#include <stdio.h>
#include <memory.h>
#include "cardio.h"
#include "hand1.h"
#include "hand2.h"
#include "hand3.h"

char* version = "Ver. 1.31  970418A";

unsigned long ev0, ev1, ev2, ev3;
unsigned long ev10, ev11, ev12, ev13;

#define FMAX 9
char* disp_msg[] = {
    "1.file list",
    "2.info",
    "3.format",
    "4.unformat(quick)",
    "5.unformat(complete)",
    "6.create 15 files",
    "7.easy format test",
    "8.make card data",
    "9.exit"};

typedef struct {
    DRAWENV draw; /* drawing environment */
    DISPENV disp; /* display environment */
} DB;
DB db[2];
DB* cdb;

static int makecard(char* fnam);
static void init_prim();
void _draw();
void _pre_draw();
int _card_event();
int _card_event_x();
void _clear_event();
void _clear_event_x();
int et_format(long chan);
int unformat(long chan);
int q_unformat(long chan);
int dir_file(char* drv, struct DIRENTRY* d);
u_long GetPad();
/************************************************************************=
***

*************************************************************************=
**/
int main(void) {
    static RECT bg = {0, 0, 640, 480};
    long redraw, exec, menu;
    char buf[256];
    long i, ret;
    long padd;

    ResetCallback();
    ResetGraph(0);
    SetGraphDebug(0);

#ifdef APD_SAVE
    APDSaveInit(0, progname);
#elif defined(APD_LOAD)
    if (APDLoadInit(0, progname))
        return 0;
#endif
    PadInit(0);
    InitCARD(1);
    StartCARD();
    ChangeClearPAD(0);
    _bu_init();

    FntLoad(640, 0);
    FntOpen(16, 16, 288, 200, 0, 512);

    SetDefDrawEnv(&db[0].draw, 0, 0, 320, 240);
    SetDefDrawEnv(&db[1].draw, 0, 240, 320, 240);
    SetDefDispEnv(&db[0].disp, 0, 240, 320, 240);
    SetDefDispEnv(&db[1].disp, 0, 0, 320, 240);

    init_prim(&db[0]);
    init_prim(&db[1]);

#ifdef APD_LOAD
    APDSetCnt(VSync(0));
#else
    VSync(0);
#endif
    ClearImage(&bg, 0, 0, 0);
    SetDispMask(1);

    menu = 0;
    exec = 0;
    redraw = 0;

    EnterCriticalSection();
    ev0 = OpenEvent(SwCARD, EvSpIOE, EvMdNOINTR, NULL);
    ev1 = OpenEvent(SwCARD, EvSpERROR, EvMdNOINTR, NULL);
    ev2 = OpenEvent(SwCARD, EvSpTIMOUT, EvMdNOINTR, NULL);
    ev3 = OpenEvent(SwCARD, EvSpNEW, EvMdNOINTR, NULL);
    ev10 = OpenEvent(HwCARD, EvSpIOE, EvMdNOINTR, NULL);
    ev11 = OpenEvent(HwCARD, EvSpERROR, EvMdNOINTR, NULL);
    ev12 = OpenEvent(HwCARD, EvSpTIMOUT, EvMdNOINTR, NULL);
    ev13 = OpenEvent(HwCARD, EvSpNEW, EvMdNOINTR, NULL);
    EnableEvent(ev0);
    EnableEvent(ev1);
    EnableEvent(ev2);
    EnableEvent(ev3);
    EnableEvent(ev10);
    EnableEvent(ev11);
    EnableEvent(ev12);
    EnableEvent(ev13);
    ExitCriticalSection();

    _draw();

    while (1) {
        exec = 0;
        padd = GetPad();
        if (padd == PADLup && menu > 0)
            menu--;
        else if (padd == PADLdown && menu < FMAX - 1)
            menu++;
        else if (padd == PADstart)
            exec = menu + 1;
        else if (redraw == 0)
            redraw = 1;
        else
            continue;

        FntPrint("\n    MEMORY CARD MANAGER \n");
        FntPrint("    %s\n\n", version);
        for (i = 0; i < FMAX; i++) {
            if (menu == i)
                strcpy(buf, "* ");
            else
                strcpy(buf, "  ");
            strcat(buf, disp_msg[i]);
            FntPrint("  %s\n", buf);
        }
        FntPrint("\n    KEY : Up&Down&Start\n");

        if (exec == 2) {
            _pre_draw();
            _draw();
            FntPrint("\nTESTING\n");
            _pre_draw();
            _draw();
            _card_info(0x00);
            ret = _card_event();
            FntPrint("\nSTATUS OF MEMORY CARD PORT-0\n\n");
            switch (ret) {
            case 0:
                FntPrint("CARD EXIST\n");
                break;
            case 2:
                FntPrint("NO CARD\n");
                goto skip;
            case 3:
                FntPrint("NEW CARD\n");
                _clear_event_x();
                _card_clear(0x00);
                ret = _card_event_x();
                break;
            case 1:
            default:
                FntPrint("ERROR\n");
                break;
            }
            _clear_event();
            _card_load(0x00); /* load file system */
            ret = _card_event();
            switch (ret) {
            case 0:
                FntPrint("FORMATTED\n");
                break;
            case 2:
                FntPrint("NO CARD\n");
                break;
            case 3:
                FntPrint("UNFORMATTED\n");
                break;
            case 1:
            default:
                FntPrint("ERROR\n");
                break;
            }
            FntPrint("\nPUSH ANY KEY");
            _pre_draw();
            _draw();
            while (GetPad() == 0)
                ;
            while (GetPad() != 0)
                ;
            redraw = 0;
        } else if (exec == 3) {
            _pre_draw();
            _draw();
            FntPrint("\nFORMAT OK? (select/else)");
            _pre_draw();
            _draw();
            while ((padd = GetPad()) == 0)
                ;
            if (padd != PADselect)
                FntPrint("\nCANCEL!\n");
            else {
                FntPrint("\nFORMATTING\n");
                _pre_draw();
                _draw();
                ret = format("bu00:");
                if (ret == 1)
                    FntPrint("\nDONE\n");
                else
                    FntPrint("\nERROR\n");
            }
            FntPrint("\nPUSH ANY KEY");
            _pre_draw();
            _draw();
            while (GetPad() == 0)
                ;
            while (GetPad() != 0)
                ;
            redraw = 0;
        } else if (exec == 4 || exec == 5) {
            _pre_draw();
            _draw();
            FntPrint("\nUNFORMAT OK? (select/else)");
            _pre_draw();
            _draw();
            while ((padd = GetPad()) == 0)
                ;
            if (padd != PADselect)
                FntPrint("\nCANCEL!\n");
            else {
                FntPrint("\nUNFORMATTING\n");
                if (exec == 4)
                    FntPrint("It will take about one minute.");
                _pre_draw();
                _draw();
                if (exec == 4)
                    ret = q_unformat(0x00);
                else
                    ret = unformat(0x00);
                if (ret == 1)
                    FntPrint("\nDONE\n");
                else
                    FntPrint("\nERROR\n");
            }
            FntPrint("\nPUSH ANY KEY");
            _pre_draw();
            _draw();
            while (GetPad() == 0)
                ;
            while (GetPad() != 0)
                ;
            redraw = 0;
        } else if (exec == 1) {
            struct DIRENTRY d[15];
            _pre_draw();
            _draw();
            FntPrint("\nLISTING\n");
            _pre_draw();
            _draw();
            ret = dir_file("bu00:", &d[0]);
            _pre_draw();
            _draw();
            if (ret == 0)
                FntPrint("\n  NO FILE\n");
            else {
                FntPrint("\n  %d FILE(S)\n\n", ret);
                for (i = 0; i < ret; i++) {
                    FntPrint("  %2d:%s\n", i + 1, d[i].name);
                }
            }
            FntPrint("\nPUSH ANY KEY");
            _pre_draw();
            _draw();
            while (GetPad() == 0)
                ;
            while (GetPad() != 0)
                ;
            redraw = 0;
        } else if (exec == 6) {
            long fd;
            _pre_draw();
            _draw();
            FntPrint("\nCREATE FILES OK? (select/else)");
            _pre_draw();
            _draw();
            while ((padd = GetPad()) == 0)
                ;
            if (padd != PADselect)
                FntPrint("\nCANCEL!\n");
            else {
                FntPrint("\nCREATING 15 FILES\n");
                _pre_draw();
                _draw();
                _card_info(0x00);
                ret = _card_event();
                if (!(ret == 1 || ret == 2)) {
                    if (ret == 3) {
                        _clear_event_x();
                        _card_clear(0x00);
                        ret = _card_event_x();
                    }
                    _clear_event();
                    _card_load(0x00);
                    ret = _card_event();
                    if (ret == 3) {
                        FntPrint("\nFORMATTING");
                        _pre_draw();
                        _draw();
                        ret = format("bu00:");
                        if (ret == 1)
                            FntPrint("\nDONE\n");
                        else {
                            FntPrint("\nERROR IN FOORMATTING\n");
                            goto skip;
                        }
                        FntPrint("\nCREATING 15 FILES\n");
                        _pre_draw();
                        _draw();
                    }
                    for (i = ret = 0; i < 15; i++) {
                        strcpy(buf, "bu00:SCE");
                        buf[8] = ((i + 1) > 9) ? '1' : '0';
                        buf[9] = '0' + (i + 1) % 10;
                        buf[10] = '\0';
                        if ((fd = open(buf, O_CREAT | 0x10000)) != -1)
                            ret++;
                        close(fd);
                    }
                    FntPrint("\n%d FILE(S) CREATED\n", ret);
                } else {
                    FntPrint("\nCANNOT CREATE ANY FILE\n");
                }
            }
        skip:
            FntPrint("\nPUSH ANY KEY");
            _pre_draw();
            _draw();
            while (GetPad() == 0)
                ;
            while (GetPad() != 0)
                ;
            redraw = 0;
        } else if (exec == 8) {
            _pre_draw();
            _draw();
            strcpy(buf, "\nMAKE CARD DATA OK? (SELECT/ELSE)\n");
            FntPrint(buf);
            _pre_draw();
            _draw();
            while ((padd = GetPad()) == 0)
                ;
            if (padd == PADselect) {
                FntPrint(buf);
                FntPrint("\nDATA WRITING...\n");
                _pre_draw();
                _draw();
                ret = makecard("bu00:HAND");
                FntPrint(buf);
                FntPrint("\nDATA WRITING...%s\n", ret ? "DONE" : "ERROR");
            } else
                FntPrint("\nCANCEL!\n");
            FntPrint("\nPUSH ANY KEY");
            _pre_draw();
            _draw();
            while (GetPad() == 0)
                ;
            while (GetPad() != 0)
                ;
            redraw = 0;
        } else if (exec == 9) {
            DrawSync(0);
            StopCARD();
            EnterCriticalSection();
            CloseEvent(ev0);
            CloseEvent(ev1);
            CloseEvent(ev2);
            CloseEvent(ev3);
            CloseEvent(ev10);
            CloseEvent(ev11);
            CloseEvent(ev12);
            CloseEvent(ev13);
            ExitCriticalSection();
            PadStop();
#ifdef APD_SAVE
            APDSaveStop();
#elif defined(APD_LOAD)
            APDLoadStop(VSync(-1));
#endif
            StopCallback();
            return 0;
        } else if (exec == 7) {
            _pre_draw();
            _draw();
            ret = et_format(0x00);
            FntPrint("\nRESULT OF FORMAT TEST\n");
            if (ret == 1)
                FntPrint("FORMATTED\n");
            else if (ret == -1)
                FntPrint("ERROR\n");
            else
                FntPrint("UNFORMATTED\n");
            FntPrint("\nPUSH ANY KEY");
            _pre_draw();
            _draw();
            while (GetPad() == 0)
                ;
            while (GetPad() != 0)
                ;
            redraw = 0;
        }
        _pre_draw();
        _draw();
    }
}

u_long GetPad() {
    static u_long old = 0;
    u_long w;

#if defined(APD_SAVE)
    w = APDSave(PadRead(0));
#elif defined(APD_LOAD)
    w = APDLoad(0);
#else
    w = PadRead(0);
#endif
    if (w != old)
        return (old = w);
    return 0;
}

void _pre_draw() {
    DrawSync(0);
#ifdef APD_LOAD
    APDSetCnt(VSync(0));
#else
    VSync(0);
#endif
    FntFlush(-1);
}

void _draw() {
    cdb = (cdb == db) ? db + 1 : db;
    PutDrawEnv(&cdb->draw);
    PutDispEnv(&cdb->disp);
}

int _card_event() {
    while (1) {
        if (TestEvent(ev0) == 1) { /* IOE */
            return 0;
        }
        if (TestEvent(ev1) == 1) { /* ERROR */
            return 1;
        }
        if (TestEvent(ev2) == 1) { /* TIMEOUT */
            return 2;
        }
        if (TestEvent(ev3) == 1) { /* NEW CARD */
            return 3;
        }
    }
}

void _clear_event() {
    TestEvent(ev0);
    TestEvent(ev1);
    TestEvent(ev2);
    TestEvent(ev3);
}

int _card_event_x() {
    while (1) {
        if (TestEvent(ev10) == 1) { /* IOE */
            return 0;
        }
        if (TestEvent(ev11) == 1) { /* ERROR */
            return 1;
        }
        if (TestEvent(ev12) == 1) { /* TIMEOUT */
            return 2;
        }
        if (TestEvent(ev13) == 1) { /* NEW CARD */
            return 3;
        }
    }
}

void _clear_event_x() {
    TestEvent(ev10);
    TestEvent(ev11);
    TestEvent(ev12);
    TestEvent(ev13);
}

static void init_prim(DB* db) {
    db->draw.isbg = 1;
    /*	setRGB0(&db->draw, 60, 120, 120);*/
    setRGB0(&db->draw, 0, 0, 64);
}

int unformat(long chan) {
    long i;
    char buf[128];

    _pre_draw();
    _draw();
    for (i = 0; i < 128; i++)
        buf[i] = 0xff;
    for (i = 0; i < 1024; i++) {
        _clear_event_x();
        _new_card();
        _card_write(chan, i, &buf[0]);
        if (_card_event_x() != 0)
            return 0;
        FntPrint("\nUNFORMATTING\n");
        FntPrint("It will take about one minute.");
        FntPrint("\n\ncount = %d", 1024 - i);
        _pre_draw();
        _draw();
    }
    return 1;
}

int q_unformat(long chan) {
    char buf[128];

    buf[0] = buf[1] = 0xff;
    _clear_event_x();
    _new_card();
    _card_write(chan, 0, &buf[0]);
    if (_card_event_x() != 0)
        return 0;
    return 1;
}

int dir_file(char* drv, struct DIRENTRY* d) {
    long i;
    char key[128];

    strcpy(key, drv);
    strcat(key, "*");

    i = 0;
    if (firstfile(key, d) == d) {
        do {
            i++;
            d++;
        } while (nextfile(d) == d);
    }
    return i;
}

int et_format(long chan) {
    char buf[128];

    bzero(&buf[0], 128);
    _clear_event_x();
    _new_card();
    _card_read(chan, 0, &buf[0]);
    if (_card_event_x() != 0)
        return -1;
    if (buf[0] == 'M' && buf[1] == 'C')
        return 1;
    return 0;
}

typedef struct {
    long id;
    long flag;
    long cbnum;
    short cx;
    short cy;
    short cw;
    short ch;
    char clut[32];
    long pbnum;
    short px;
    short py;
    short pw;
    short ph;
    char image[2];
} _TIM4;
static _CARD HEAD;
static char bufs[8192];

int makecard(char* fnam) {
    long fd;
    long totalbytes;

    int i, j;
    unsigned char* cp;

    totalbytes = 8192;

    for (i = 0; i < totalbytes; i++)
        bufs[i] = 0;

    HEAD.Magic[0] = 'S';
    HEAD.Magic[1] = 'C';
    HEAD.Type = 0x13;
    HEAD.BlockEntry = (totalbytes / 8192) + (totalbytes % 8192 ? 1 : 0);
    strcpy(HEAD.Title, "=82=B6=82=E1=82=F1=82=AF=82=F1=81@=82=DB=82=F1=81@=82=="
                       "AE=81[=81A=82=BF=82=E5=82=AB=81A=82=CF=81[");
    memcpy(HEAD.Clut, ((_TIM4*)hand1)->clut, 32);
    memcpy(HEAD.Icon[0], ((_TIM4*)hand1)->image, 128);
    memcpy(HEAD.Icon[1], ((_TIM4*)hand2)->image, 128);
    memcpy(HEAD.Icon[2], ((_TIM4*)hand3)->image, 128);
    memcpy(bufs, &HEAD, sizeof(HEAD));

    if ((fd = open(fnam, O_CREAT | HEAD.BlockEntry << 16)) == -1) {
        printf("%s open error.\n", fnam);
        return 0;
    }
    printf("Discript: %d\n", fd);
    close(fd);

    if ((fd = open(fnam, O_WRONLY)) == -1) {
        printf("%s open error.\n", fnam);
        return 0;
    }
    if ((i = write(fd, bufs, totalbytes)) != totalbytes) {
        printf("write error. %d, %d\n", i, totalbytes);
        close(fd);
        return 0;
    }
    close(fd);

    return 1; /* complete */
}
