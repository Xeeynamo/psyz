#include "libsnd_private.h"

short SsVabOpenHeadWithMode(
    u_char* addr, short vabid, short mode, u_long sbaddr);

short SsVabOpenHead(u_char* addr, short vabid) {
    return SsVabOpenHeadWithMode(addr, vabid, 0, 0);
}

short SsVabOpenHeadSticky(u_char* addr, short vabid, u_long sbaddr) {
    return SsVabOpenHeadWithMode(addr, vabid, 1, sbaddr);
}

short SsVabFakeHead(u_char* addr, short vabid, u_long sbaddr) {
    return SsVabOpenHeadWithMode(addr, vabid, 1, sbaddr);
}

INCLUDE_ASM("asm/nonmatchings/libsnd/vs_vh", SsVabOpenHeadWithMode);
