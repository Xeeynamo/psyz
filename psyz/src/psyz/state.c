#include <psyz.h>
#include <psyz/state.h>
#include <psyz/audio.h>
#include <libspu.h>
#include <libsnd.h>
#include "../../decomp/src/libspu/libspu_private.h"
#include "../../decomp/src/libsnd/libsnd_private.h"

extern unsigned int _svm_envx_ptr;

void Psyz_LibSndStateSync(PsyzState* state) {
    PSYZ_STATE_VAR(state, _snd_ev_flag);
    PSYZ_STATE_VAR(state, _snd_openflag);
    PSYZ_STATE_VAR(state, _snd_seq_tick_env.tick_mode);
    PSYZ_STATE_VAR(state, _snd_seq_tick_env.manual_tick);
    state->pointer(state, &_snd_seq_tick_env.tick_cb);
    state->pointer(state, &_snd_seq_tick_env.vsync_cb);
    PSYZ_STATE_VAR(state, _snd_seq_tick_env.vsync_tick);
    PSYZ_STATE_VAR(state, _snd_seq_tick_env.unk11);
    PSYZ_STATE_VAR(state, _snd_seq_tick_env.alarm_tick);
    PSYZ_STATE_VAR(state, _snd_seq_tick_env.unk14);
    PSYZ_STATE_VAR(state, _svm_cur);
    PSYZ_STATE_VAR(state, _svm_damper);
    PSYZ_STATE_VAR(state, _svm_okon1);
    PSYZ_STATE_VAR(state, _svm_okon2);
    PSYZ_STATE_VAR(state, _svm_okof1);
    PSYZ_STATE_VAR(state, _svm_okof2);
    PSYZ_STATE_VAR(state, _svm_orev1);
    PSYZ_STATE_VAR(state, _svm_orev2);
    PSYZ_STATE_VAR(state, _svm_voice);
    PSYZ_STATE_VAR(state, _svm_envx_hist);
    PSYZ_STATE_VAR(state, _svm_envx_ptr);
    PSYZ_STATE_VAR(state, _svm_rattr);
    PSYZ_STATE_VAR(state, _svm_vab_used);
    for (int i = 0; i < NUM_VAB; i++) {
        state->pointer(state, &_svm_vab_vh[i]);
        state->pointer(state, &_svm_vab_pg[i]);
        state->pointer(state, &_svm_vab_tn[i]);
    }
    PSYZ_STATE_VAR(state, _svm_vab_start);
    PSYZ_STATE_VAR(state, _svm_vab_total);
    state->pointer(state, &_svm_vh);
    state->pointer(state, &_svm_pg);
    state->pointer(state, &_svm_tn);
    state->pointer(state, &_svm_vg);
    PSYZ_STATE_VAR(state, _svm_vab_count);
    PSYZ_STATE_VAR(state, _svm_stereo_mono);
    PSYZ_STATE_VAR(state, _SsVmMaxVoice);
    state->pointer(state, &_autovol);
    state->pointer(state, &_autopan);
    PSYZ_STATE_VAR(state, _svm_sreg_buf);
    PSYZ_STATE_VAR(state, _svm_sreg_dirty);
    for (int i = 0; i < _snd_seq_s_max; i++) {
        for (int j = 0; j < _snd_seq_t_max; j++) {
            struct SeqStruct* score = &_ss_score[i][j];
            state->pointer(state, &score->seq_ptr);
            state->pointer(state, &score->read_pos);
            state->pointer(state, &score->next_sep_pos);
            state->pointer(state, &score->loop_pos);
            state->bytes(state, &score->unk10,
                         sizeof(*score) - offsetof(struct SeqStruct, unk10));
        }
    }
}

void Psyz_LibSpuDecompStateSync(PsyzState* state) {
    PSYZ_STATE_VAR(state, _spu_EVdma);
    PSYZ_STATE_VAR(state, _spu_keystat);
    PSYZ_STATE_VAR(state, _spu_trans_mode);
    PSYZ_STATE_VAR(state, _spu_transMode);
    PSYZ_STATE_VAR(state, _spu_rev_flag);
    PSYZ_STATE_VAR(state, _spu_rev_reserve_wa);
    PSYZ_STATE_VAR(state, _spu_rev_offsetaddr);
    PSYZ_STATE_VAR(state, _spu_rev_attr);
    state->bytes(state, (void*)&_spu_RQvoice, sizeof(_spu_RQvoice));
    state->bytes(state, (void*)&_spu_RQmask, sizeof(_spu_RQmask));
    PSYZ_STATE_VAR(state, _spu_env);
    PSYZ_STATE_VAR(state, _spu_isCalled);
    PSYZ_STATE_VAR(state, _spu_AllocBlockNum);
    PSYZ_STATE_VAR(state, _spu_AllocLastNum);
    state->pointer(state, &_spu_memList);
    PSYZ_STATE_VAR(state, _spu_tsa);
    PSYZ_STATE_VAR(state, _spu_mem_mode_plus);
    PSYZ_STATE_VAR(state, _spu_mem_mode_unitM);
    PSYZ_STATE_VAR(state, _spu_inTransfer);
    state->pointer(state, (void*)&_spu_transferCallback);
    state->pointer(state, (void*)&_spu_IRQCallback);
}

void Psyz_StateSync(PsyzState* state) {
    Psyz_AudioLock();
    Psyz_SpuStateSync(state);
    Psyz_LibSpuStateSync(state);
    Psyz_LibSpuDecompStateSync(state);
    Psyz_LibSndStateSync(state);
    Psyz_CdStateSync(state);
    Psyz_EtcStateSync(state);
    Psyz_GpuStateSync(state);
    Psyz_GpuSysStateSync(state);
    Psyz_GteStateSync(state);
    Psyz_AudioUnlock();
}
