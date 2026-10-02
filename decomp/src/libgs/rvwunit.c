#include <common.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>

INCLUDE_ASM("asm/nonmatchings/libgs/rvwunit", GsSetRefViewUnit);

WEAK_INCLUDE_ASM("asm/nonmatchings/libgs/rvwunit", scale_view_param);

WEAK_INCLUDE_ASM("asm/nonmatchings/libgs/rvwunit", select_max_param);

WEAK_INCLUDE_ASM("asm/nonmatchings/libgs/rvwunit", len_param);
