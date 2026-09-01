#include "libsnd_private.h"

#ifdef __psyz
void (*SsSetTickCallback(void (*cb)(void)))(void) {
#else
int SsSetTickCallback(void (*cb)(void)) {
#endif
    void (*prev)(void);

    prev = _snd_seq_tick_env.tick_cb;
    if (cb != _snd_seq_tick_env.tick_cb) {
        _snd_seq_tick_env.tick_cb = cb;
    }
    return prev;
}
