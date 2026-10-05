#include <psyz.h>
#include <libapi.h>
#include <psyz/log.h>
#include <sys/stat.h>
#include "../internal.h"

// 1:valid, 0:invalid
static inline int validate_chan(long chan) {
    unsigned int cardNo = (unsigned int)(chan & 15);
    unsigned int portNo = (unsigned int)(chan >> 4);
    if (cardNo >= 4) { // validate multi-tap card support
        return 0;
    }
    if (portNo >= 2) { // PS1 has only two memcard ports
        return 0;
    }
    return 1;
}

void _bu_init(void) {
#ifdef _MSC_VER
    NOT_IMPLEMENTED;
#elif __MINGW32__
    mkdir("bu00");
    mkdir("bu10");
#else
    mkdir("bu00", 0755);
    mkdir("bu10", 0755);
#endif
}

long _card_auto(long val) {
    NOT_IMPLEMENTED;
    return val;
}

static long last_chan;

// Cards live as host directories and are always plugged in, but there is no
// multi-tap, so its extra slots time out like an empty port.
static long card_op(unsigned int sw_desc, long chan) {
    if (!validate_chan(chan)) {
        return 0;
    }
    last_chan = chan;
    unsigned int spec = (chan & 15) ? EvSpTIMOUT : EvSpIOE;
    if (sw_desc) {
        Psyz_KernelRaise(sw_desc, spec);
    }
    Psyz_KernelRaise(HwCARD, spec);
    return 1;
}

long _card_info(long chan) { return card_op(SwCARD, chan); }

long _card_load(long chan) { return card_op(SwCARD, chan); }

void _new_card(void) {}

long _card_status(long drv) {
    (void)drv;
    return 1;
}

long _card_wait(long drv) {
    (void)drv;
    return 1;
}

unsigned long _card_chan(void) { return (unsigned long)last_chan; }

void InitCARD2(long val) { (void)val; }

long StartCARD2(void) { return 1; }

long StopCARD2(void) { return 1; }

void _ExitCard(void) {}

static void _bzero(unsigned char* p, int n) { memset(p, 0, n); }

long _card_sector_write(long chan, long block, unsigned char* buf) {
    unsigned char sp10[0x80];
    u8 checksum;
    s32 retries;
    s32 i;
    u8* var_v1;
    u8* var_v1_2;

    retries = 0;
    var_v1 = buf;
    checksum = 0;
    for (i = 0; i < 0x7F; i++) {
        checksum ^= *var_v1++;
    }
    *var_v1 = checksum;
    while (1) {
        if (retries < 8) {
            _new_card();
            if (_card_write(chan, block, buf) == 1) {
                do {
                } while (!(_card_status(chan >> 4) & 1));
                var_v1_2 = sp10;
                _bzero(var_v1_2, sizeof(sp10));
                _new_card();
                if (_card_read(chan, block, var_v1_2) != 1) {
                    ERRORF("card read error\n");
                } else {
                    do {
                    } while (!(_card_status(chan >> 4) & 1));
                }
                var_v1 = sp10;
                checksum = 0;
                for (i = 0; i < 0x7F; i++) {
                    checksum ^= *var_v1++;
                }
                if (buf[0x7F] == checksum) {
                    return 1;
                }
                retries++;
            } else {
                return 0;
            }
        } else {
            break;
        }
    }
    return 0;
}

long _card_write(long chan, long block, unsigned char* buf) {
    LOG_ONCE("raw sectors are not emulated, only the completion event");
    (void)block;
    (void)buf;
    return card_op(0, chan);
}

long _card_read(long chan, long block, unsigned char* buf) {
    LOG_ONCE("raw sectors are not emulated, only the completion event");
    (void)block;
    (void)buf;
    return card_op(0, chan);
}
