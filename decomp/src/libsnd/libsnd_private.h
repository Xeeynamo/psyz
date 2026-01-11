// SPDX-License-Identifier: MIT
#ifndef LIBSND_INTERNAL_H
#define LIBSND_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <common.h>
#include <libspu.h>
#include <libsnd.h>

#define NUM_SPU_CHANNELS 24

#ifndef SndSsMarkCallbackProc_DEF
typedef void (*SndSsMarkCallbackProc)(short seq_no, short sep_no, short data);
#endif

struct Unk {
    u16 unk0;
    u16 unk2;
    u16 unk4;
    u16 unk6;
    u16 unk8;
    s16 unkA;
    s16 unkC;
    u16 unkE;
    s16 unk10;
};

struct SeqStruct {
    /* 0x00 */ u8 unk0;
    /* 0x01 */ u8 pad1[3];
    /* 0x04 */ u8* read_pos;
    /* 0x08 */ u8* next_sep_pos;
    /* 0x0C */ u8* loop_pos;
    /* 0x10 */ u8 unk10;
    /* 0x11 */ u8 unk11;
    /* 0x12 */ u8 channel;
    /* 0x13 */ u8 unk13;
    /* 0x14 */ u8 unk14;
    /* 0x15 */ u8 unk15;
    /* 0x16 */ u8 unk16;
    /* 0x17 */ u8 unk17;
    /* 0x18 */ s32 unk18;
    /* 0x1C */ s32 unk1C;
    /* 0x20 */ s16 unk20;
    /* 0x22 */ u8 unk22;
    /* 0x23 */ u8 unk23;
    /* 0x24 */ s32 unk24;
    /* 0x28 */ u8 unk28;
    /* 0x29 */ u8 unk29;
    /* 0x2A */ u8 unk2a;
    /* 0x2B */ u8 unk2b;
    /* 0x2C */ u8 programs[16];
    /* 0x3C */ u8 unk3C;
    /* 0x3D */ u8 pad3D;
    /* 0x3E */ s16 unk3E;
    /* 0x40 */ s16 unk40;
    /* 0x42 */ s16 unk42;
    /* 0x44 */ s16 unk44;
    /* 0x46 */ s16 unk46;
    /* 0x48 */ s16 unk48;
    /* 0x4A */ s16 unk4A;
    /* 0x4C */ s16 unk4C;
    /* 0x4E */ s16 unk4E;
    /* 0x50 */ s16 unk50;
    /* 0x52 */ s16 unk52;
    /* 0x54 */ s16 unk54;
    /* 0x56 */ s16 unk56;
    /* 0x58 */ s16 unk58;
    /* 0x5A */ s16 unk5A;
    /* 0x5C */ s16 unk5C;
    /* 0x5E */ s16 unk5E;
    /* 0x60 */ s32 unk60;
    /* 0x64 */ s32 unk64;
    /* 0x68 */ s32 unk68;
    /* 0x6C */ s16 unk6C;
    /* 0x6E */ s16 unk6E;
    /* 0x70 */ s16 unk70;
    /* 0x72 */ s16 unk72;
    /* 0x74 */ u16 unk74;
    /* 0x76 */ u16 unk76;
    /* 0x78 */ s16 unk78;
    /* 0x7A */ s16 unk7A;
    /* 0x7C */ s32 unk7c;
    /* 0x80 */ u32 unk80;
    /* 0x84 */ s32 unk84;
    /* 0x88 */ s32 delta_value;
    /* 0x8C */ s32 unk8c;
    /* 0x90 */ s32 unk90;
    /* 0x94 */ u32 unk94;
    /* 0x98 */ u32 unk98;
    /* 0x9C */ s32 unk9C;
    /* 0xA0 */ u32 unkA0;
    /* 0xA4 */ u32 unkA4;
    /* 0xA8 */ s32 unkA8;
    /* 0xAC */ s32 unkAC;
};

struct SndSeqTickEnv {
    s32 unk0;
    s32 unk4;
    void (*unk8)();
    void (*unk12)();
    u8 unk16;
    u8 unk17;
    u8 unk18;
    u8 unk19;
    u32 unk20;
};

struct SpuVoice {
    s16 unk0;
    s16 unk2;
    s16 unk04;
    u16 unk6;
    s16 unk8;
    char unka;
    char unkb;
    s16 note; /* 0xC */
    s16 unke;
    s16 unk10;
    s16 prog;  /* 0x12 */
    s16 tone;  /* 0x14*/
    s16 vabId; /* 0x16 */
    s16 unk18;
    u8 pad4[1];
    u8 unk1b;
    s16 auto_vol; /* 0x1c */
    s16 unk1e;
    s16 unk20;
    s16 unk22;
    s16 start_vol; /* 0x24 */
    s16 end_vol;   /* 0x26 */
    s16 auto_pan;  /* 0x28 */
    s16 unk2a;
    s16 unk2c;
    s16 unk2e;
    s16 start_pan; /* 0x30 */
    s16 end_pan;   /* 0x32 */
};

struct struct_svm {
    char field_0_sep_sep_no_tonecount;
    char field_1_vabId;
    char field_2_note;
    char field_0x3;
    char field_4_voll;
    char field_0x5;
    char field_6_program;
    char field_7_fake_program;
    char field_8_unknown;
    char field_0x9;
    char field_A_mvol;
    char field_B_mpan;
    char field_C_vag_idx;
    char field_D_vol;
    char field_E_pan;
    char field_F_prior;
    char field_10_centre;
    unsigned char field_11_shift;
    char field_12_mode;
    char field_0x13;
    u8 field_14_seq_sep_no;
    u8 pad;
    short field_16_vag_idx;
    short field_18_voice_idx;
    short field_0x1a;
    short field_0x1c;
    short field_0x1e;
};

extern s32 D_8003C74C;
#ifndef SndSsMarkCallbackProc_DEF
extern SndSsMarkCallbackProc _SsMarkCallback[32][16];
#endif
extern s32 _snd_ev_flag;
extern s32 _snd_openflag;
extern s16 _snd_seq_s_max;
extern s16 _snd_seq_t_max;
extern struct SndSeqTickEnv _snd_seq_tick_env;
extern struct SeqStruct* _ss_score[32];
extern s32 _svm_brr_start_addr[];
extern struct struct_svm _svm_cur;
extern s16 _svm_damper;
extern s16 kMaxPrograms;
extern unsigned short _svm_okon1;
extern unsigned short _svm_okon2;
extern unsigned short _svm_okof1;
extern unsigned short _svm_okof2;
extern ProgAtr* _svm_pg;
extern SpuReverbAttr _svm_rattr;
extern VagAtr* _svm_tn;
extern s32 _svm_vab_start[];
extern VagAtr* _svm_vab_tn[16];
extern s32 _svm_vab_total[];
extern u8 _svm_vab_used[];
extern ProgAtr* _svm_vab_pg[16];
extern VabHdr* _svm_vab_vh[16];
extern VabHdr* _svm_vh;
extern u16 _svm_vab_count;
extern u8 spuVmMaxVoice;
extern s16 _svm_stereo_mono;
extern u32 VBLANK_MINUS;

void* InterruptCallback(u8, void (*)());
void SeAutoPan(s16, s16, s16, s16);
void SeAutoVol(s16, s16, s16, s16);
void Snd_SetPlayMode(s16, s16, u8, s16);
long SpuIsTransferCompleted(long);
void SpuQuit(void);
void SpuSetCommonAttr(SpuCommonAttr* attr);
s32 SpuVmGetSeqVol(s16, s16*, s16*);
void SpuVmFlush();
void SpuVmSeKeyOn(s16 arg0, s16 arg1, u16 arg2, s32 arg3, u16 arg4, u16 arg5);
s32 SpuVmSetSeqVol(s16 seq_sep_no, u16 voll, u16 volr, s16 arg3);
u32 SpuVmVSetUp(s16, s16);
void _SsInit(void);
void _SsSeqPlay(s16, s16);
void _SsSndCrescendo(s16, s16);
void _SsSndDecrescendo(s16, s16);
void _SsSndPause(s16, s16);
void _SsSndPlay(s16, s16);
void _SsSndReplay(s16, s16);
void _SsSndTempo(s16, s16);
void _SsUtResolveADSR(u16 arg0, u16 arg1, struct Unk* arg2);
void _SsVmSeqKeyOff(s16 seq_sep_num);
s32 _SsVmSetSeqVol(s16 seq_sep_no, u16 voll, u16 volr, s16 arg3);
void _spu_setInTransfer(s32);
void vmNoiseOn2(u8 arg0, u16 arg1, u16 arg2, u16 arg3, u16 arg4);

#ifdef __cplusplus
}
#endif

#endif
