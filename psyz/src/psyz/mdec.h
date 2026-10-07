#ifndef PSYZ_MDEC_INTERNAL_H
#define PSYZ_MDEC_INTERNAL_H
#include <stddef.h>
#include <stdint.h>
void Psyz_MdecReset(void);
int Psyz_MdecCommand(uint32_t command, const void* data, size_t words);
int Psyz_MdecRead(void* data, size_t words);
#endif
