#include <common.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>

INCLUDE_ASM("asm/nonmatchings/libgs/gs_107", GsSetFlatLight);

INCLUDE_ASM("asm/nonmatchings/libgs/gs_107", gte_set_lc);

INCLUDE_ASM("asm/nonmatchings/libgs/gs_107", gte_read_lc);
