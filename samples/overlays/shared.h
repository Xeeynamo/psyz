#ifndef SHARED_H
#define SHARED_H
#include <psyz/module.h>

// Imported from both main and overlays. Ensures signatrues do not diverge.

extern int g_SampleCounter;
extern void MainLog(const char* s);

struct SampleState {
    int starts;
    int stops;
    void (*pfnEntrypoint)(void);
};

#endif
