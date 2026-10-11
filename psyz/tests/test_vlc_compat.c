#include "ztest.h"
#include <libpress.h>
#include <stdint.h>
#include <string.h>
#include "vlc_psyq47.h"

static uint32_t output[512], expected[512];

ZTEST_TEARDOWN(vlc_compat) { DecDCTvlcSize(0); }

ZTEST(vlc_compat, sdk_decode_matches_psyq47_at_every_continuation) {
    for (unsigned i = 0; i < sizeof(vlc_psyq47) / sizeof(*vlc_psyq47); ++i) {
        const VlcFixture* fixture = &vlc_psyq47[i];
        for (unsigned mode = 0; mode < 9; ++mode) {
            const VlcTrace* trace = &fixture->traces[mode];
            memset(output, 0xa5, sizeof(output));
            DecDCTvlcSize(0);
            zassert_s32_eq(0xffffff, DecDCTvlcSize(trace->limit));
            for (unsigned call = 0; call < trace->calls; ++call) {
                int result =
                    call ? DecDCTvlc(NULL, NULL)
                         : DecDCTvlc((u_long*)fixture->input, (u_long*)output);
                int status = call + 1 < trace->calls;
                memset(expected, 0xa5, sizeof(expected));
                memcpy(expected, fixture->expected, trace->written[call] * 2);
                if (result != status ||
                    memcmp(expected, output, (fixture->words + 1) * 4))
                    zprintf("fixture: %s, limit: %d, call: %u\n", fixture->name,
                            trace->limit, call);
                zassert_s32_eq(status, result);
                zassert_u8array_eq(expected, output, (fixture->words + 1) * 4);
            }
        }
    }
}

ZTEST(vlc_compat, size_returns_the_previous_sdk_limit) {
    DecDCTvlcSize(0);
    zassert_s32_eq(0xffffff, DecDCTvlcSize(2));
    zassert_s32_eq(4, DecDCTvlcSize(7));
    zassert_s32_eq(14, DecDCTvlcSize(1));
    zassert_s32_eq(0xffffff, DecDCTvlcSize(-1));
    zassert_s32_eq(0xffffff, DecDCTvlcSize(0));
}
