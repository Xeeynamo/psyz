#include <common.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>

INCLUDE_ASM("asm/nonmatchings/libgs/matrix", GsInitCoordinate2);

INCLUDE_ASM("asm/nonmatchings/libgs/matrix", GsInitCoord2param);

INCLUDE_ASM("asm/nonmatchings/libgs/matrix", GsSetLsMatrix);

INCLUDE_ASM("asm/nonmatchings/libgs/matrix", GsSetLightMatrix);

INCLUDE_ASM("asm/nonmatchings/libgs/matrix", GsSetLightMatrix2);

INCLUDE_ASM("asm/nonmatchings/libgs/matrix", GsMulCoord0);

INCLUDE_ASM("asm/nonmatchings/libgs/matrix", GsMulCoord2);

INCLUDE_ASM("asm/nonmatchings/libgs/matrix", GsMulCoord3);

INCLUDE_ASM("asm/nonmatchings/libgs/matrix", print_matrix);

INCLUDE_ASM("asm/nonmatchings/libgs/matrix", print_vector);
