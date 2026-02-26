#include "libsnd_private.h"

struct SndSeqTickEnv _snd_seq_tick_env = {
    60, 1, SsSeqCalledTbyT, NULL, 0, 0, 127};

void _SsStart(int arg0);
INCLUDE_ASM("asm/nonmatchings/libsnd/ssstart", _SsStart);

void SsStart(void) { _SsStart(1); }

void SsStart2(void) { _SsStart(0); }

void _SsTrapIntrVSync(void) {
    struct SndSeqTickEnv* env = &_snd_seq_tick_env;

    if (env->vsync_cb) {
        env->vsync_cb();
    }
    env->tick_cb();
}

void _SsSeqCalledTbyT_1per2(void) {
    struct SndSeqTickEnv* env = &_snd_seq_tick_env;

    if (env->unk14 == 0) {
        env->unk14 = 1;
    } else {
        env->unk14 = 0;
        env->tick_cb();
    }
}
