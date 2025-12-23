#include <common.h>
#include <libspu.h>

static s32 D_800D0B80[] = {0x15117350, 0x0040B49C};
s32 _spu_EVdma = 0;
s32 _spu_keystat = 0;
s32 _spu_trans_mode = 0;
s32 _spu_rev_flag = 0;
s32 _spu_rev_reserve_wa = 0;
s32 _spu_rev_offsetaddr = 0;
s32 _spu_rev_attr = 0;
s32 D_800D0BA4 = 0;
s16 D_800D0BA8 = 0;
s16 D_800D0BAA = 0;
s32 D_800D0BAC = 0;
s32 D_800D0BB0 = 0;
s32 _spu_RQvoice = 0;
s32 _spu_RQmask = 0;
s16 _spu_voice_centerNote[] = {
    0xC000, 0xC000, 0xC000, 0xC000, 0xC000, 0xC000, 0xC000, 0xC000,
    0xC000, 0xC000, 0xC000, 0xC000, 0xC000, 0xC000, 0xC000, 0xC000,
    0xC000, 0xC000, 0xC000, 0xC000, 0xC000, 0xC000, 0xC000, 0xC000,
};
s32 _spu_zerobuf[256] = {};
s32 _spu_env = 0;
s32 _spu_isCalled = 0;
static s32 dummy = 0;

INCLUDE_ASM("asm/nonmatchings/libspu/s_ini", _SpuInit);

INCLUDE_ASM("asm/nonmatchings/libspu/s_ini", SpuStart);
