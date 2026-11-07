#ifndef PSYZ_STATE_H
#define PSYZ_STATE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PsyzState PsyzState;

struct PsyzState {
    int loading;
    void (*bytes)(PsyzState* state, void* data, size_t size);
    void (*pointer)(PsyzState* state, void* slot);
    void* user;
};

#define PSYZ_STATE_VAR(state, var) (state)->bytes((state), &(var), sizeof(var))

void Psyz_StateSync(PsyzState* state);

void Psyz_SpuStateSync(PsyzState* state);
void Psyz_LibSpuStateSync(PsyzState* state);
void Psyz_LibSndStateSync(PsyzState* state);
void Psyz_LibSpuDecompStateSync(PsyzState* state);
void Psyz_CdStateSync(PsyzState* state);
void Psyz_EtcStateSync(PsyzState* state);
void Psyz_GpuStateSync(PsyzState* state);
void Psyz_GpuSysStateSync(PsyzState* state);
void Psyz_GteStateSync(PsyzState* state);

#ifdef __cplusplus
}
#endif

#endif
