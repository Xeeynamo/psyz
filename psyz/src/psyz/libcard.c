#include <psyz.h>
#include <libapi.h>
#include <libapi.h>
#include <log.h>
#include <sys/stat.h>

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

long _card_info(long chan) {
    NOT_IMPLEMENTED;
    return 0;
}

long _card_load(long chan) {
    NOT_IMPLEMENTED;
    return 0;
}

void _new_card(void) { NOT_IMPLEMENTED; }

long _card_status(long drv) {
    NOT_IMPLEMENTED;
    return 0;
}

void InitCARD2(long val) { NOT_IMPLEMENTED; }

long StartCARD2(void) {
    NOT_IMPLEMENTED;
    return 0;
}

long StopCARD2(void) {
    NOT_IMPLEMENTED;
    return 0;
}

void _ExitCard(void) { NOT_IMPLEMENTED; }

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
    NOT_IMPLEMENTED;
    return 0;
}

long _card_read(long chan, long block, unsigned char* buf) {
    NOT_IMPLEMENTED;
    return 0;
}