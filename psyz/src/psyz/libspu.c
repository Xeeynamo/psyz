#include <psyz.h>
#include <libspu.h>
#include <psyz/log.h>
#include "../../decomp/src/libspu/libspu_private.h"

static SPU_RXX spu_RXX;
union SpuUnion* _spu_RXX = (union SpuUnion*)&spu_RXX;
volatile u16 _spu_RQ[10] = {0};

void SpuNGetVoiceAttr(int vNum, SpuVoiceAttr* arg) { NOT_IMPLEMENTED; }
