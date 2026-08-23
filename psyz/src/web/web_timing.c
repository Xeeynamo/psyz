#include <emscripten.h>

// psy-q blocks in VSync which doesn't work well with emscripten.
// use asyncify to break out to the browser and resume after
EM_ASYNC_JS(void, Psyz_WebWaitForNextFrame, (), {
    await new Promise((resolve) => requestAnimationFrame(resolve));
    globalThis.__psyzFrameCount = (globalThis.__psyzFrameCount || 0) + 1;
});
