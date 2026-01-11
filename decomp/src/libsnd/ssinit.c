#include <common.h>
#include <libsnd.h>

#define SndSsMarkCallbackProc_DEF
typedef void (*SndSsMarkCallbackProc)(short seq_no, short sep_no, short data);

extern s16 _snd_seq_s_max;
extern s16 _snd_seq_t_max;
extern s32 _snd_ev_flag;
extern _SsFCALL SsFCALL;
extern SndSsMarkCallbackProc _SsMarkCallback[32][16];
extern struct SeqStruct* _ss_score[32];
extern u32 VBLANK_MINUS;
extern s32 _snd_openflag;

#include "libsnd_private.h"

s16 _snd_seq_s_max;
s16 _snd_seq_t_max;
s32 _snd_ev_flag;
_SsFCALL SsFCALL;
SndSsMarkCallbackProc _SsMarkCallback[32][16];
struct SeqStruct* _ss_score[32];
u32 VBLANK_MINUS;
s32 _snd_openflag;

INCLUDE_ASM("asm/nonmatchings/libsnd/ssinit", _SsInit);
