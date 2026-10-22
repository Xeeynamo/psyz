#include <psyz.h>
#include <psyz/state.h>
#include <libspu.h>
#include <psyz/log.h>
#include "../../decomp/src/libspu/libspu_private.h"

static SPU_RXX spu_RXX;
union SpuUnion* _spu_RXX = (union SpuUnion*)&spu_RXX;
volatile u16 _spu_RQ[10] = {0};

void Psyz_LibSpuStateSync(PsyzState* state) {
    PSYZ_STATE_VAR(state, spu_RXX);
    PSYZ_STATE_VAR(state, _dma_dpcr);
    state->bytes(state, (void*)_spu_RQ, sizeof(_spu_RQ));
}

void SpuNGetVoiceAttr(int vNum, SpuVoiceAttr* arg) { NOT_IMPLEMENTED; }
