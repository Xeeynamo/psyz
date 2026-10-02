#include <common.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>

INCLUDE_ASM("asm/nonmatchings/libgs/gs_001", GsInitGraph);

INCLUDE_ASM("asm/nonmatchings/libgs/gs_001", gpu_init);

INCLUDE_ASM("asm/nonmatchings/libgs/gs_001", GsInitGraph2);

INCLUDE_ASM("asm/nonmatchings/libgs/gs_001", valiable_init);

INCLUDE_ASM("asm/nonmatchings/libgs/gs_001", GsSortClear);
