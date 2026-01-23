#include <common.h>
#include <libcomb.h>

INCLUDE_ASM("asm/nonmatchings/libcomb/comb", AddCOMB);

const char D_800B28B8[] = "SIO console";
const char D_800B28C4[] = "sio";

INCLUDE_ASM("asm/nonmatchings/libcomb/comb", DelCOMB);

void ChangeClearSIO(long val) {}

INCLUDE_ASM("asm/nonmatchings/libcomb/comb", SioAnsyncRead);

INCLUDE_ASM("asm/nonmatchings/libcomb/comb", SioSyncroRead);

INCLUDE_ASM("asm/nonmatchings/libcomb/comb", SioAnsyncWrite);

INCLUDE_ASM("asm/nonmatchings/libcomb/comb", SioSyncroWrite);

INCLUDE_ASM("asm/nonmatchings/libcomb/comb", _comb_control);

INCLUDE_ASM("asm/nonmatchings/libcomb/comb", EvalpSio);

INCLUDE_ASM("asm/nonmatchings/libcomb/comb", HandleSio);

INCLUDE_ASM("asm/nonmatchings/libcomb/comb", r_sioinit);

INCLUDE_ASM("asm/nonmatchings/libcomb/comb", r_sioopen);

INCLUDE_ASM("asm/nonmatchings/libcomb/comb", r_sioclose);

INCLUDE_ASM("asm/nonmatchings/libcomb/comb", r_sioremove);

INCLUDE_ASM("asm/nonmatchings/libcomb/comb", r_siostrategy);

INCLUDE_ASM("asm/nonmatchings/libcomb/comb", __nulldev);
