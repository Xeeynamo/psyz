/* $PSLibId: Run-time Library Release 4.0$ */
#if defined(APD_SAVE) || defined(APD_LOAD)
static char* progname = "graphics/balls/balls.cpe";
#endif
/*				balls
 *
 *		��ʓ����o�E���h���镡���̃{�[����`�悷��
 *
 *		Copyright (C) 1993 by Sony Corporation
 *			All rights Reserved
 *
 *	 Version	Date		Design
 *	-----------------------------------------
 *	1.00		Aug,31,1993	suzu
 *	2.00		Nov,17,1993	suzu	(using 'libgpu)
 *	3.00		Dec.27.1993	suzu	(rewrite)
 *	3.01		Dec.27.1993	suzu	(for newpad)
 *	3.02		Aug.31.1994	noda    (for KANJI)
 *	4.00		May.22.1995	sachiko	(���{��̃R�����g��t���j
 *	4.01		Mar. 5.1997	sachiko	(added autopad�j
 */

#include <psyz.h>
#include <sys/types.h>
#include <libetc.h>
#include <libgte.h>
#include <libgpu.h>
#include <stdlib.h>

/*
 * Kaji Printf: �������v�����g���邽�߂̒�`
 */
//#define KANJI

/*#define DEBUG */
/*
 * Primitive Buffer: �v���~�e�B�u�֘A�̃o�b�t�@
 */
#define OTSIZE                                                                 \
    1 /* size of ordering table                                                \
 : �I�[�_�����O�e�[�u���̐� */
#define MAXOBJ                                                                 \
    4000 /* max sprite number :                                                \
�X�v���C�g�i�{�[���j���̏�� */
typedef struct {
    DRAWENV draw;         /* drawing environment : �`��� */
    DISPENV disp;         /* display environment : �\���� */
    OT_TYPE ot[OTSIZE];    /* ordering table:
                     : �I�[�_�����O�e�[�u�� */
    SPRT_16 sprt[MAXOBJ]; /* 16x16 fixed-size sprite:
                 16x16�Œ�T�C�Y�̃X�v���C�g */
} DB;

/*
 * Position Buffer: �X�v���C�g�̓����Ɋւ���o�b�t�@
 */
typedef struct {
    u_short x, y;   /* current point: ���݂̈ʒu */
    u_short dx, dy; /* verocity: ���x */
} POS;

/*
 * �\���̈�
 */
#define FRAME_X 320 /* frame size:�\���̈�T�C�Y(320x240)*/
#define FRAME_Y 240
#define WALL_X                                                                 \
    (FRAME_X - 16) /* reflection point                                         \
      : �X�v���C�g�̉��̈�T�C�Y */
#define WALL_Y (FRAME_Y - 16)

/* preset unchanged primitve members: �v���~�e�B�u�o�b�t�@�̏����ݒ� */
static void init_prim(DB* db);

/* parse controller: �R���g���[���̉�� */
static int pad_read(int n);

/* callback for VSync: V-Sync���̃R�[���o�b�N���[�`�� */
static void cbvsync(void);

/* intitialze position table : �{�[���̃X�^�[�g�ʒu�ƈړ������̐ݒ� */
static int init_point(POS* pos);

int main() {
    /* �{�[���̍��W�l�ƈړ��������i�[����o�b�t�@ */
    POS pos[MAXOBJ];

    /* double buffer: �_�u���o�b�t�@�̂��߂Q�p�ӂ���
     */
    DB db[2];

    /* current double buffer: ���݂̃_�u���o�b�t�@�o�b�t�@�̃A�h���X */
    DB* cdb;

    /* object number:
     * �\������X�v���C�g�̐��i�ŏ��͂P����j*/
    int nobj = 1;

    /* current OT: ���݂̂n�s�̃A�h���X */
    OT_TYPE* ot;

    SPRT_16* sp;      /* work */
    POS* pp;          /* work */
    int i, cnt, x, y; /* work */

    /* reset PAD: �R���g���[���̃��Z�b�g */
#if defined(APD_SAVE)
    APDSaveInit(0, progname);
#elif defined(APD_LOAD)
    if (APDLoadInit(0, progname))
        return 0;
#endif
#ifndef APD_LOAD
    PadInit(0);
#endif

    /* reset graphics sysmtem (0:cold,1:warm); �`��E�\�����̃��Z�b�g
     */
    ResetGraph(0);

    /* set debug mode (0:off,1:monitor,2:dump): �f�o�b�O���[�h�̐ݒ� */
    SetGraphDebug(0);

    /* set callback: V-sync���̃R�[���o�b�N�֐��̐ݒ�
     */
    VSyncCallback(cbvsync);

    /* inititlalize environment for double buffer
    :
    �`��E�\�������_�u���o�b�t�@�p�ɐݒ�
    (0,  0)-(320,240)�ɕ`�悵�Ă���Ƃ���(0,240)-(320,480)��\��(db[0])
    (0,240)-(320,480)�ɕ`�悵�Ă���Ƃ���(0,  0)-(320,240)��\��(db[1])
    */
    SetDefDrawEnv(&db[0].draw, 0, 0, 320, 240);
    SetDefDrawEnv(&db[1].draw, 0, 240, 320, 240);
    SetDefDispEnv(&db[0].disp, 0, 240, 320, 240);
    SetDefDispEnv(&db[1].disp, 0, 0, 320, 240);

    /* init font environment; �t�H���g�̐ݒ� */
#ifdef KANJI /* KANJI */
    KanjiFntOpen(160, 16, 256, 200, 704, 0, 768, 256, 0, 512);
#endif
    /* :��{�t�H���g�p�^�[�����t���[���o�b�t�@�Ƀ��[�h */
    FntLoad(960, 256);

    /* :�t�H���g�̕\���ʒu�̐ݒ� */
    SetDumpFnt(FntOpen(16, 16, 256, 200, 0, 512));

    /* initialize primitive buffer: �v���~�e�B�u�o�b�t�@�̏����ݒ�(db[0])*/
    init_prim(&db[0]);

    /* initialize primitive buffer: �v���~�e�B�u�o�b�t�@�̏����ݒ�(db[1])*/
    init_prim(&db[1]);

    /* set initial geometries: �{�[���̓����Ɋւ��鏉���ݒ� */
    init_point(pos);

    /* enable to display: �f�B�X�v���C�ւ̕\���J�n */
    SetDispMask(1); /* 0:inhibit,1:enable: �O�F�s��  �P�F�� */

    /* ; ���C�����[�v */
    while ((nobj = pad_read(nobj)) > 0) {
        /* swap double buffer ID: �_�u���o�b�t�@�|�C���^�̐؂�ւ�
         */
        cdb = (cdb == db) ? db + 1 : db;
#ifdef DEBUG
        /* dump DB environment */
        DumpDrawEnv(&cdb->draw);
        DumpDispEnv(&cdb->disp);
        DumpTPage(cdb->draw.tpage);
#endif

        /* clear ordering table: �I�[�_�����O�e�[�u���̃N���A */
        ClearOTag(cdb->ot, OTSIZE);

        /* update sprites :
           �{�[���̈ʒu���P���v�Z���Ăn�s�ɓo�^���Ă���
         */
        ot = cdb->ot;
        sp = cdb->sprt;
        pp = pos;
        for (i = 0; i < nobj; i++, sp++, pp++) {
            /* detect reflection:
               ���W�l�̍X�V����щ�ʏ�ł̈ʒu�̌v�Z */
            if ((x = (pp->x += pp->dx) % WALL_X * 2) >= WALL_X)
                x = WALL_X * 2 - x;
            if ((y = (pp->y += pp->dy) % WALL_Y * 2) >= WALL_Y)
                y = WALL_Y * 2 - y;

            /* update vertex: �v�Z�������W�l���Z�b�g */
            setXY0(sp, x, y);

            /* apend to OT: �n�s�֓o�^ */
            AddPrim(ot, sp);
        }
        /* wait for end of drawing: �`��̏I���҂� */
        DrawSync(0);

        /* cnt = VSync(1);	/* check for count */
        /* cnt = VSync(2);	/* wait for V-BLNK (1/30) */
#ifdef APD_LOAD
        cnt = APDSetCnt(VSync(0));
#else
        cnt = VSync(0); /* wait for V-BLNK (1/60) */
#endif

        /*: �_�u���o�b�t�@�̐ؑւ� */
        /* update display environment: �\�����̍X�V */
        PutDispEnv(&cdb->disp);

        /* update drawing environment: �`����̍X�V */
        PutDrawEnv(&cdb->draw);

        /*: �n�s�ɓo�^���ꂽ�v���~�e�B�u�̕`�� */
        DrawOTag(cdb->ot);
#ifdef DEBUG
        DumpOTag(cdb->ot);
#endif
        /*: �{�[���̐��ƌo�ߎ��Ԃ̃v�����g */
#ifdef KANJI
        KanjiFntPrint("�ʂ̐���%d\n", nobj);
        KanjiFntPrint("����=%d\n", cnt);
        KanjiFntFlush(-1);
#endif
        FntPrint("sprite = %d\n", nobj);
        FntPrint("total time = %d\n", cnt);
        FntFlush(-1);
    }
#ifndef APD_LOAD
    PadStop(); /*: �R���g���[���̃N���[�Y */
#endif
#if defined(APD_SAVE)
    APDSaveStop();
#elif defined(APD_LOAD)
    APDLoadStop(VSync(-1));
#endif
    StopCallback();

    return (0);
}

/*
 * Initialize drawing Primitives: �v���~�e�B�u�o�b�t�@�̏����ݒ�
 */
#include "balltex.h" /* �{�[���̃e�N�X�`���p�^�[���������Ă���t�@�C�� */

/* DB *db; �v���~�e�B�u�o�b�t�@*/
static void init_prim(DB* db) {
    u_short clut[32]; /* CLUT entry: �e�N�X�`�� CLUT */
    SPRT_16* sp;      /* work */
    int i;            /* work */

    /* set bg color: �w�i�F�̃Z�b�g */
    db->draw.isbg = 1;
    setRGB0(&db->draw, 60, 120, 120);

    /* load texture pattern: �e�N�X�`���̃��[�h */
    db->draw.tpage = LoadTPage((u_long*)ball16x16, 0, 0, 640, 0, 16, 16);
#ifdef DEBUG
    DumpTPage(db->draw.tpage);
#endif
    /* load texture CLUT: �e�N�X�`�� CLUT�̃��[�h */
    for (i = 0; i < 32; i++) {
        clut[i] = LoadClut((u_long*)ballcolor[i], 0, 480 + i);
#ifdef DEBUG
        DumpClut(clut[i]);
#endif
    }

    /* initialize sprite: �X�v���C�g�̏����� */
    for (sp = db->sprt, i = 0; i < MAXOBJ; i++, sp++) {
        /* set SPRT_16: 16x16�X�v���C�g�v���~�e�B�u�̏����� */
        SetSprt16(sp);

        /* semi-ambient is OFF: �����������I�t */
        SetSemiTrans(sp, 0);

        /* shaded texture is OFF: �V�F�[�f�B���O���s��Ȃ� */
        SetShadeTex(sp, 1);

        /* texture point is (0,0): u,v��(0,0)�ɐݒ� */
        setUV0(sp, 0, 0);

        /* set CLUT: CLUT �̃Z�b�g */
        sp->clut = clut[i % 32];
    }
}

/*
 * Initialize sprite position and verocity:
 : �{�[���̃X�^�[�g�ʒu�ƈړ��ʂ�ݒ肷��
 */

/* POS	*pos;		�{�[���̓����Ɋւ���\���� */
static int init_point(POS* pos) {
    int i;
    for (i = 0; i < MAXOBJ; i++) {
        pos->x = rand();            /*: �X�^�[�g���W �w */
        pos->y = rand();            /*: �X�^�[�g���W �x */
        pos->dx = (rand() % 4) + 1; /*: �ړ����� �w (1<=x<=4) */
        pos->dy = (rand() % 4) + 1; /*: �ړ����� �x (1<=y<=4) */
        pos++;
    }
    return 0;
}

/*
 * Read controll-pad: �R���g���[���̉��
 */
/* int n; �X�v���C�g�̐� */
static int pad_read(int n) {
    u_long padd;

    /*: �R���g���[���̓ǂݍ��� */
#if defined(APD_SAVE)
    padd = APDSave(PadRead(0));
#elif defined(APD_LOAD)
    padd = APDLoad(0);
#else
    padd = PadRead(0);
#endif

    if (padd & PADLup)
        n += 4; /*: ���̏\���L�[�A�b�v */
    if (padd & PADLdown)
        n -= 4; /*: ���̏\���L�[�_�E�� */

    if (padd & PADL1) /*: pause */
#if defined(APD_SAVE)
        while (APDSave(PadRead(0)) & PADL1)
            ;
#elif defined(APD_LOAD)
        while (APDLoad(0) & PADL1)
            ;
#else
        while (PadRead(0) & PADL1)
            ;
#endif

    if (padd & PADselect)
        return (-1); /*: �v���O�����̏I�� */

    limitRange(n, 1, MAXOBJ - 1); /*: n��1<=n<=(MAXOBJ-1)�̒l�ɂ��� */
    /* see libgpu.h: libgpu.h�ɋL�� */
    return (n);
}

/*
 * callback: �R�[���o�b�N
 */
static void cbvsync(void) {
    /* print absolute VSync count */
    FntPrint("V-BLNK(%d)\n", VSync(-1));
}
