#ifndef PSYZ_MDEC_H
#define PSYZ_MDEC_H

#include <psyz/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void Psyz_MdecWrite(unsigned offset, u32 value);

u32 Psyz_MdecReadStatus(void);

#ifdef __cplusplus
}
#endif

#endif
