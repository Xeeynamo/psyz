#include <common.h>
#include <libspu.h>

u16 _spu_tsa[] = {0, 0};
s32 _spu_transMode = 0;
s32 _spu_addrMode = 0;
s32 _spu_mem_mode = 2;
s32 _spu_mem_mode_plus = 3;
s32 _spu_mem_mode_unit = 8;
s32 _spu_mem_mode_unitM = 7;
s32 _spu_inTransfer = 1;
s32 _spu_transferCallback = 0;
s32 _spu_IRQCallback = 0;
s8 D_800D1048[] = {7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};
s32 D_800D1058 = 0;
s32 D_800D105C = 0;
s32 D_800D1060 = 0;
static s32 D_800D1064 = 0;

static const char D_800B4D80[] = "SPU:T/O [%s]\n";

INCLUDE_ASM("asm/nonmatchings/libspu/spu", _spu_init);

INCLUDE_ASM("asm/nonmatchings/libspu/spu", _spu_FwriteByIO);

INCLUDE_ASM("asm/nonmatchings/libspu/spu", _spu_FiDMA);

INCLUDE_ASM("asm/nonmatchings/libspu/spu", _spu_Fr_);

INCLUDE_ASM("asm/nonmatchings/libspu/spu", _spu_t);

INCLUDE_ASM("asm/nonmatchings/libspu/spu", _spu_Fw);

INCLUDE_ASM("asm/nonmatchings/libspu/spu", _spu_Fr);

INCLUDE_ASM("asm/nonmatchings/libspu/spu", _spu_FsetRXX);

INCLUDE_ASM("asm/nonmatchings/libspu/spu", _spu_FsetRXXa);

INCLUDE_ASM("asm/nonmatchings/libspu/spu", _spu_FgetRXXa);

INCLUDE_ASM("asm/nonmatchings/libspu/spu", _spu_FsetPCR);

INCLUDE_ASM("asm/nonmatchings/libspu/spu", _spu_FsetDelayW);

INCLUDE_ASM("asm/nonmatchings/libspu/spu", _spu_FsetDelayR);

INCLUDE_ASM("asm/nonmatchings/libspu/spu", _spu_Fw1ts);
