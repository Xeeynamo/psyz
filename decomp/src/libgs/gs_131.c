#include <common.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>

INCLUDE_ASM("asm/nonmatchings/libgs/gs_131", GsSetRefView2);

WEAK_INCLUDE_ASM("asm/nonmatchings/libgs/gs_131", scale_view_param);

WEAK_INCLUDE_ASM("asm/nonmatchings/libgs/gs_131", select_max_param);

WEAK_INCLUDE_ASM("asm/nonmatchings/libgs/gs_131", len_param);
