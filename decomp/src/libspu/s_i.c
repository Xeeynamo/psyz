#include <common.h>
#include <libspu.h>

void _SpuInit(s32 arg0);
void SpuInit(void) { _SpuInit(0); }
