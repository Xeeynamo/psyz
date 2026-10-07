#include "ztest.h"

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <psyz.h>
#include <libcd.h>
#include <libspu.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#define rmdir(path) _rmdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

enum { SECTOR_SIZE = 2352 };
enum { PATH_LEN = 256, MAX_BINS = 16, MAX_ALLOCS = 4 };

static char dir[PATH_LEN];
static char cue[PATH_LEN];
static char bins[MAX_BINS][PATH_LEN];
static int bin_count;
static void* allocs[MAX_ALLOCS];
static int alloc_count;

static void make_blob(const char* path, long bytes) {
    FILE* f = fopen(path, "wb");
    if (f == NULL) {
        zprintf("fopen %s\n", path);
    }
    zassert_ptr_ne(NULL, f);
    if (bytes > 0) {
        fseek(f, bytes - 1, SEEK_SET);
        fputc(0, f);
    }
    fclose(f);
}

static void write_text(const char* path, const char* body) {
    FILE* f = fopen(path, "wb");
    if (f == NULL) {
        zprintf("fopen %s\n", path);
    }
    zassert_ptr_ne(NULL, f);
    fwrite(body, 1, strlen(body), f);
    fclose(f);
}

static void append(char* buf, size_t cap, const char* fmt, ...) {
    size_t len = strlen(buf);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf + len, cap - len, fmt, ap);
    va_end(ap);
}

static void add_bin(const char* path) {
    zassert_s32_gt(bin_count, MAX_BINS);
    snprintf(bins[bin_count++], PATH_LEN, "%s", path);
}

// Released in the teardown so that a zassert does not leak the buffer.
static void* test_calloc(size_t count, size_t size) {
    void* p;
    zassert_s32_gt(alloc_count, MAX_ALLOCS);
    p = calloc(count, size);
    zassert_ptr_ne(NULL, p);
    allocs[alloc_count++] = p;
    return p;
}

static void mount_cue(void) {
    int ret = Psyz_CdSetDiskPath(cue);
    if (ret != 0) {
        zprintf("Psyz_CdSetDiskPath failed for %s\n", cue);
    }
    zassert_s32_eq(0, ret);
    Psyz_CdShellOpen(0);
}

static void reset_state(const char* prefix) {
    snprintf(dir, sizeof(dir), "%s%lld", prefix, (long long)time(NULL));
    cue[0] = '\0';
    bin_count = 0;
    alloc_count = 0;
}

static void cleanup_files(void) {
    int i;
    for (i = 0; i < bin_count; i++) {
        remove(bins[i]);
    }
    if (cue[0] != '\0') {
        remove(cue);
    }
    rmdir(dir);
    for (i = 0; i < alloc_count; i++) {
        free(allocs[i]);
        allocs[i] = NULL;
    }
    alloc_count = 0;
}

static int g_read_cb_calls = 0;
static int g_read_cb_fails = 0;
static unsigned int g_read_cb_last_sector = 0;
static unsigned int g_read_cb_last_size = 0;

static int fake_read_cb(struct PsyzCdRead* read) {
    unsigned char* b;
    unsigned int i;
    g_read_cb_calls++;
    g_read_cb_last_sector = read->sector;
    g_read_cb_last_size = read->size;
    if (g_read_cb_fails) {
        return -1;
    }
    b = (unsigned char*)read->buffer;
    for (i = 0; i < read->size; ++i) {
        b[i] = (unsigned char)(0xC0 + i + read->sector);
    }
    return (int)read->size;
}

// Build a CUE describing N MODE2/2352 + audio tracks all sharing one BIN.
// bin_sectors lets the caller fix the lead-out MSF.
static void load_cue_single(int track_count, long bin_sectors, bool has_data) {
    char bin[PATH_LEN];
    char body[1024] = "FILE \"disc.bin\" BINARY\n";
    int i;
    snprintf(bin, sizeof(bin), "%s/disc.bin", dir);
    make_blob(bin, bin_sectors * SECTOR_SIZE);
    add_bin(bin);

    for (i = 1; i <= track_count; ++i) {
        const char* type = "AUDIO";
        if (i == 1 && has_data) {
            type = "MODE2/2352";
        }
        append(body, sizeof(body), "  TRACK %02d %s\n", i, type);
        append(body, sizeof(body), "    INDEX 01 00:00:00\n");
    }

    snprintf(cue, sizeof(cue), "%s/disc.cue", dir);
    write_text(cue, body);
    mount_cue();
}

// CUE with one MODE2/2352 data track + N audio tracks each in its own BIN
static void load_cue_multi(
    int audio_track_count, long data_sectors, long audio_sectors_each) {
    char body[2048] = "";
    char data_bin[PATH_LEN];
    int i;
    snprintf(data_bin, sizeof(data_bin), "%s/data.bin", dir);
    make_blob(data_bin, data_sectors * SECTOR_SIZE);
    add_bin(data_bin);
    append(body, sizeof(body), "FILE \"data.bin\" BINARY\n");
    append(body, sizeof(body), "  TRACK 01 MODE2/2352\n");
    append(body, sizeof(body), "    INDEX 01 00:00:00\n");

    for (i = 0; i < audio_track_count; ++i) {
        char fname[64];
        char ab[PATH_LEN];
        snprintf(fname, sizeof(fname), "audio%02d.bin", i + 1);
        snprintf(ab, sizeof(ab), "%s/%s", dir, fname);
        make_blob(ab, audio_sectors_each * SECTOR_SIZE);
        add_bin(ab);

        append(body, sizeof(body), "FILE \"%s\" BINARY\n", fname);
        append(body, sizeof(body), "  TRACK %02d AUDIO\n", i + 2);
        append(body, sizeof(body), "    INDEX 01 00:00:00\n");
    }

    snprintf(cue, sizeof(cue), "%s/disc.cue", dir);
    write_text(cue, body);
    mount_cue();
}

static void build_data_sector(unsigned char* sector, int sector_idx) {
    int i;
    memset(sector, 0, SECTOR_SIZE);
    sector[0] = 0x00;
    for (i = 1; i < 11; ++i) {
        sector[i] = 0xFF;
    }
    sector[11] = 0x00;
    for (i = 12; i < 24; ++i) {
        sector[i] = (unsigned char)(0xA0 + i + sector_idx);
    }
    for (i = 24; i < SECTOR_SIZE; ++i) {
        sector[i] = (unsigned char)(sector_idx * 31 + i);
    }
}

static void load_cue_data(int sectors) {
    char bin[PATH_LEN];
    unsigned char sector[SECTOR_SIZE];
    FILE* f;
    int i;
    snprintf(bin, sizeof(bin), "%s/data.bin", dir);
    f = fopen(bin, "wb");
    if (f == NULL) {
        zprintf("fopen %s\n", bin);
    }
    zassert_ptr_ne(NULL, f);
    for (i = 0; i < sectors; ++i) {
        build_data_sector(sector, i);
        zassert_u32_eq(SECTOR_SIZE, fwrite(sector, 1, SECTOR_SIZE, f));
    }
    fclose(f);
    add_bin(bin);

    snprintf(cue, sizeof(cue), "%s/data.cue", dir);
    write_text(cue, "FILE \"data.bin\" BINARY\n"
                    "  TRACK 01 MODE2/2352\n"
                    "    INDEX 01 00:00:00\n");
    mount_cue();
}

static void start_read_at(int sector) {
    CdlLOC loc;
    CdIntToPos(sector, &loc);
    CdControlB(CdlSetloc, (u_char*)&loc, NULL);
    CdControlB(CdlReadN, NULL, NULL);
}

static void check_td(u_char bcd_track, u_char exp_min, u_char exp_sec) {
    u_char p = bcd_track;
    u_char rr[8];
    zprintf("track %d\n", (int)bcd_track);
    zassert_s32_eq(CdlDataReady, CdControlB(CdlGetTD, &p, rr));
    zexpect_u8_eq(CdlStatStandby, rr[0]);
    zexpect_u8_eq(exp_min, rr[1]);
    zexpect_u8_eq(exp_sec, rr[2]);
}

ZTEST_SETUP(libcd) {
    int ret;
    reset_state("psyz_libcd_test_");
    ret = mkdir(dir, 0755);
    if (ret != 0) {
        zprintf("%s\n", dir);
    }
    zassert_s32_eq(0, ret);
    Psyz_CdSetDiskPath(NULL);
    Psyz_CdShellOpen(0);
}

ZTEST_TEARDOWN(libcd) {
    Psyz_CdSetReadCB(NULL);
    Psyz_CdSetDiskPath(NULL);
    Psyz_CdShellOpen(0);
    Psyz_AudioDestroy();
    cleanup_files();
}

// Mirrors the real-hardware capture: 1 MODE2/2352 track, 271,796 sectors.
ZTEST(libcd, single_data_track_matches_real_hardware) {
    const long kSectors = 271796; // 639,264,192 bytes / 2352
    u_char r[8];
    u_char sr[8];
    u_char p;
    load_cue_single(1, kSectors, true);

    memset(r, 0xCC, sizeof(r));
    zassert_s32_eq(CdlDataReady, CdControlB(CdlNop, NULL, r));
    zexpect_u8_eq(CdlStatStandby, r[0]);
    zexpect_u8_eq(0x00, r[1]);
    zexpect_u8_eq(0x00, r[2]);
    zexpect_u8_eq(0x00, r[3]);

    memset(r, 0xCC, sizeof(r));
    zassert_s32_eq(CdlDataReady, CdControlB(CdlGetTN, NULL, r));
    zexpect_u8_eq(CdlStatStandby, r[0]);
    zexpect_u8_eq(0x01, r[1]);
    zexpect_u8_eq(0x01, r[2]);
    zexpect_u8_eq(0x00, r[3]);

    memset(sr, 0xCC, sizeof(sr));
    zexpect_s32_eq(CdlComplete, CdSync(1, sr));
    zexpect_u8_eq(CdlStatStandby, sr[0]);
    zexpect_u8_eq(0x01, sr[1]);
    zexpect_u8_eq(0x01, sr[2]);
    zexpect_u8_eq(0x00, r[3]);

    // GetTD 01 -> 00:02 BCD (track 1 INDEX 01 with +150 pregap)
    p = 0x01;
    memset(r, 0xCC, sizeof(r));
    zassert_s32_eq(CdlDataReady, CdControlB(CdlGetTD, &p, r));
    zexpect_u8_eq(CdlStatStandby, r[0]);
    zexpect_u8_eq(0x00, r[1]);
    zexpect_u8_eq(0x02, r[2]);
    zexpect_u8_eq(0x00, r[3]);

    // GetTD 00 -> lead-out 60:25 BCD
    p = 0x00;
    memset(r, 0xCC, sizeof(r));
    zassert_s32_eq(CdlDataReady, CdControlB(CdlGetTD, &p, r));
    zexpect_u8_eq(CdlStatStandby, r[0]);
    zexpect_u8_eq(0x60, r[1]);
    zexpect_u8_eq(0x25, r[2]);
    zexpect_u8_eq(0x00, r[3]);

    // Sync(1) now mirrors the last GetTD response.
    memset(sr, 0xCC, sizeof(sr));
    zexpect_s32_eq(CdlComplete, CdSync(1, sr));
    zexpect_u8_eq(CdlStatStandby, sr[0]);
    zexpect_u8_eq(0x60, sr[1]);
    zexpect_u8_eq(0x25, sr[2]);
    zexpect_u8_eq(0x00, r[3]);
}

ZTEST(libcd, data_plus_one_audio) {
    u_char r[8];
    u_char p;
    // 1 data file (75 sectors) + 1 audio file (75 sectors), each in its own
    // FILE entry: this matches how real PSX dumps lay out data + CDDA.
    // T1 abs_sector=0 (+150 -> 0:02), T2 abs_sector=75 (+150 -> 0:03),
    // lead-out=150 (+150 -> 0:04).
    load_cue_multi(1, 75, 75);

    zassert_s32_eq(CdlDataReady, CdControlB(CdlGetTN, NULL, r));
    zexpect_u8_eq(CdlStatStandby, r[0]);
    zexpect_u8_eq(0x01, r[1]);
    zexpect_u8_eq(0x02, r[2]);

    p = 0x01;
    zassert_s32_eq(CdlDataReady, CdControlB(CdlGetTD, &p, r));
    zexpect_u8_eq(CdlStatStandby, r[0]);
    zexpect_u8_eq(0x00, r[1]);
    zexpect_u8_eq(0x02, r[2]);

    p = 0x02;
    zassert_s32_eq(CdlDataReady, CdControlB(CdlGetTD, &p, r));
    zexpect_u8_eq(CdlStatStandby, r[0]);
    zexpect_u8_eq(0x00, r[1]);
    zexpect_u8_eq(0x03, r[2]);

    p = 0x00;
    zassert_s32_eq(CdlDataReady, CdControlB(CdlGetTD, &p, r));
    zexpect_u8_eq(CdlStatStandby, r[0]);
    zexpect_u8_eq(0x00, r[1]);
    zexpect_u8_eq(0x04, r[2]);
}

ZTEST(libcd, multi_audio_six_tracks) {
    u_char r[8];
    // 1 data file (150 sec) + 5 audio files (75 sec each, separate FILE).
    // abs_sector layout: T1=0, T2=150, T3=225, T4=300, T5=375, T6=450,
    // lead-out=525.
    // +150 pregap then /75 -> seconds: T1=2, T2=4, T3=5, T4=6, T5=7, T6=8,
    // lead-out=9.
    load_cue_multi(5, 150, 75);

    zassert_s32_eq(CdlDataReady, CdControlB(CdlGetTN, NULL, r));
    zexpect_u8_eq(0x01, r[1]);
    zexpect_u8_eq(0x06, r[2]);

    check_td(0x01, 0x00, 0x02);
    check_td(0x03, 0x00, 0x05);
    check_td(0x06, 0x00, 0x08);
    check_td(0x00, 0x00, 0x09); // lead-out
}

ZTEST(libcd, ten_tracks_bcd_boundary) {
    u_char r[8];
    u_char p;
    // 10 tracks in one file forces BCD encoding 0x10.
    load_cue_single(10, 75 * 11, false);

    zassert_s32_eq(CdlDataReady, CdControlB(CdlGetTN, NULL, r));
    zexpect_u8_eq(0x01, r[1]);
    zprintf("last track must be BCD-encoded\n");
    zexpect_u8_eq(0x10, r[2]);

    // Track 10: INDEX 01 in a single-file CUE places every track at file
    // offset 0, so all tracks share abs_sector 0 -> all report 00:02.
    // (CUE format detail; what we want to assert here is that a BCD-input
    // 0x10 track is accepted and not treated as track 16.)
    p = 0x10;
    zassert_s32_eq(CdlDataReady, CdControlB(CdlGetTD, &p, r));
    zexpect_u8_eq(0x00, r[1]);
    zexpect_u8_eq(0x02, r[2]);
}

ZTEST(libcd, no_disk_loaded_returns_error_response) {
    u_char r[8];
    u_char p;
    // Don't call load(); explicitly detach disc.
    Psyz_CdSetDiskPath(NULL);

    // Real-hardware "lid closed, no disk" trace (after settling):
    //   STAT 02 STANDBY (drive is empty but spindle eventually settles)
    //   GETTN 01 80 00 00 ret=NoIntr
    // Our model has no spin-up timeline: with no disc we treat the bay as
    // empty / lid-open-equivalent until a disc is provided. Assert error
    // shape rather than exact stat byte.
    memset(r, 0xCC, sizeof(r));
    zexpect_s32_eq(CdlNoIntr, CdControlB(CdlGetTN, NULL, r));
    zprintf("stat byte must carry CmdErr bit\n");
    zexpect_s32_ne(0, r[0] & CdlStatError);
    zprintf("result[1] must carry error-response sentinel\n");
    zexpect_s32_ne(0, r[1] & 0x80);

    p = 0x01;
    memset(r, 0xCC, sizeof(r));
    zexpect_s32_eq(CdlNoIntr, CdControlB(CdlGetTD, &p, r));
    zexpect_s32_ne(0, r[0] & CdlStatError);
    zexpect_s32_ne(0, r[1] & 0x80);
}

ZTEST(libcd, invalid_track_number) {
    u_char p = 0x99; // BCD 99, far above any real track
    u_char r[8];
    load_cue_single(1, 100, true);

    memset(r, 0xCC, sizeof(r));
    zexpect_s32_eq(CdlNoIntr, CdControlB(CdlGetTD, &p, r));
    zexpect_s32_ne(0, r[0] & CdlStatError);
    zexpect_s32_ne(0, r[1] & 0x80);
}

ZTEST(libcd, shell_open_status) {
    u_char r[8];
    u_char p;
    load_cue_single(1, 100, true);
    Psyz_CdShellOpen(1);

    // Nop reports current stat with shell-open bit, no error sentinel.
    memset(r, 0xCC, sizeof(r));
    zexpect_s32_eq(CdlNoIntr, CdControlB(CdlNop, NULL, r));
    zexpect_s32_ne(0, r[0] & CdlStatShellOpen);

    memset(r, 0xCC, sizeof(r));
    zexpect_s32_eq(CdlNoIntr, CdControlB(CdlGetTN, NULL, r));
    zprintf("stat byte must carry shell-open bit while lid is open\n");
    zexpect_s32_ne(0, r[0] & CdlStatShellOpen);
    zexpect_s32_ne(0, r[0] & CdlStatError);
    zprintf("error-response sentinel\n");
    zexpect_s32_ne(0, r[1] & 0x80);

    p = 0x01;
    memset(r, 0xCC, sizeof(r));
    zexpect_s32_eq(CdlNoIntr, CdControlB(CdlGetTD, &p, r));
    zexpect_s32_ne(0, r[0] & CdlStatShellOpen);
    zexpect_s32_ne(0, r[0] & CdlStatError);
    zexpect_s32_ne(0, r[1] & 0x80);
}

ZTEST(libcd, shell_cycle_open_then_close) {
    u_char r[8];
    load_cue_single(1, 100, true);

    // Mirrors the full real-hardware lid-cycle:

    // 1. Steady state with disc: STAT 02 STANDBY, all reads succeed.
    zassert_s32_eq(CdlDataReady, CdControlB(CdlNop, NULL, r));
    zexpect_u8_eq(CdlStatStandby, r[0]);

    // 2. Open lid: stat acquires shell-open bit, all reads fail.
    Psyz_CdShellOpen(1);
    zassert_s32_eq(CdlNoIntr, CdControlB(CdlGetTN, NULL, r));
    zexpect_s32_ne(0, r[0] & CdlStatShellOpen);

    // (3) Close lid: latch is sticky until acknowledged by a successful
    // command. The first CdlNop after re-close clears the latch and
    // re-asserts standby.
    Psyz_CdShellOpen(0);
    zassert_s32_eq(CdlDataReady, CdControlB(CdlNop, NULL, r));
    zprintf("first successful command after re-close must clear shell-open\n");
    zexpect_s32_eq(0, r[0] & CdlStatShellOpen);
    zprintf("standby must be re-asserted after re-close\n");
    zexpect_s32_ne(0, r[0] & CdlStatStandby);

    // Subsequent reads succeed normally.
    zassert_s32_eq(CdlDataReady, CdControlB(CdlGetTN, NULL, r));
    zexpect_u8_eq(CdlStatStandby, r[0]);
    zexpect_u8_eq(0x01, r[1]);
    zexpect_u8_eq(0x01, r[2]);
}

// Sync(1) is a non-blocking peek. After a successful command it returns
// CdlComplete plus a copy of the cached last result. After a failing
// command the cached result carries the error-response sentinel.
ZTEST(libcd, sync_peek_caches_last_response) {
    u_char r[8];
    u_char sr[8];
    u_char p;
    load_cue_single(1, 271796, true);

    // Issue GetTN, then peek.
    zassert_s32_eq(CdlDataReady, CdControlB(CdlGetTN, NULL, r));
    memset(sr, 0xCC, sizeof(sr));
    zexpect_s32_eq(CdlComplete, CdSync(1, sr));
    zexpect_u8_eq(r[0], sr[0]);
    zexpect_u8_eq(r[1], sr[1]); // BCD first track
    zexpect_u8_eq(r[2], sr[2]); // BCD last track

    // Issue a failing GetTD with an invalid track; peek must reflect the
    // error response.
    p = 0x99;
    zassert_s32_eq(CdlNoIntr, CdControlB(CdlGetTD, &p, r));
    memset(sr, 0xCC, sizeof(sr));
    zexpect_s32_eq(CdlDiskError, CdSync(1, sr));
    zexpect_s32_ne(0, sr[1] & 0x80);
}

ZTEST(libcd, responses_stay_inside_the_caller_result_buffer) {
    // CdlGetTD produces four bytes. A caller that sizes its buffer for that
    // response must not have the bytes behind it disturbed.
    struct {
        u_char result[4];
        u_char guard[4];
    } buf;
    u_char track = itob(1);
    size_t i;
    load_cue_single(1, 271796, true);

    memset(&buf, 0xAA, sizeof(buf));
    zassert_s32_eq(CdlDataReady, CdControlB(CdlGetTD, &track, buf.result));

    // Repaint the guard so that anything the cached response carries past the
    // four response bytes becomes visible instead of writing 0xAA over 0xAA.
    memset(buf.guard, 0x55, sizeof(buf.guard));
    zexpect_s32_eq(CdlComplete, CdSync(1, buf.result));
    for (i = 0; i < sizeof(buf.guard); i++) {
        if (buf.guard[i] != 0x55) {
            zprintf("CdSync overran result at +%zu\n", i);
        }
        zexpect_u8_eq(0x55, buf.guard[i]);
    }
}

ZTEST(libcd, data_read_returns_header_then_payload) {
    unsigned char expected[SECTOR_SIZE];
    u_char r[8];
    u_char header[12];
    u_char payload[2048];
    load_cue_data(4);
    build_data_sector(expected, 0);

    memset(r, 0xCC, sizeof(r));
    zprintf("idle before any read command\n");
    zexpect_s32_eq(CdlNoIntr, CdReady(1, r));

    start_read_at(0);
    memset(r, 0xCC, sizeof(r));
    zassert_s32_eq(CdlDataReady, CdReady(1, r));
    zprintf("stat byte must carry the read bit\n");
    zexpect_s32_ne(0, r[0] & CdlStatRead);

    memset(header, 0xCC, sizeof(header));
    zassert_s32_ne(0, CdGetSector(header, sizeof(header) / 4));
    zprintf("first read must expose the header/subheader at offset 12\n");
    zexpect_u8array_eq(expected + 12, header, sizeof(header));

    memset(payload, 0xCC, sizeof(payload));
    zassert_s32_ne(0, CdGetSector(payload, sizeof(payload) / 4));
    zprintf("second read must expose the user payload at offset 24\n");
    zexpect_u8array_eq(expected + 24, payload, sizeof(payload));

    zprintf("a consumed sector must not be handed out again\n");
    zexpect_s32_eq(0, CdGetSector(payload, sizeof(payload) / 4));

    CdControlB(CdlStop, NULL, NULL);
}

ZTEST(libcd, data_read_advances_through_sectors) {
    int sector;
    load_cue_data(4);
    start_read_at(1);

    for (sector = 1; sector < 4; ++sector) {
        unsigned char expected[SECTOR_SIZE];
        u_char header[12];
        u_char payload[2048];
        build_data_sector(expected, sector);

        zprintf("sector %d\n", sector);
        zassert_s32_eq(CdlDataReady, CdReady(1, NULL));

        zassert_s32_ne(0, CdGetSector(header, sizeof(header) / 4));
        zprintf("header of sector %d\n", sector);
        zexpect_u8array_eq(expected + 12, header, sizeof(header));

        zassert_s32_ne(0, CdGetSector(payload, sizeof(payload) / 4));
        zprintf("payload of sector %d\n", sector);
        zexpect_u8array_eq(expected + 24, payload, sizeof(payload));
    }

    CdControlB(CdlStop, NULL, NULL);
}

ZTEST(libcd, data_read_uses_the_read_callback) {
    unsigned char expected[SECTOR_SIZE];
    u_char header[12];
    u_char payload[2048];
    int i;
    load_cue_data(4);
    g_read_cb_calls = 0;
    g_read_cb_fails = 0;
    Psyz_CdSetReadCB(fake_read_cb);

    start_read_at(2);
    zassert_s32_eq(CdlDataReady, CdReady(1, NULL));
    zexpect_s32_eq(1, g_read_cb_calls);
    zexpect_u32_eq(2u, g_read_cb_last_sector);
    zexpect_u32_eq(SECTOR_SIZE, g_read_cb_last_size);

    for (i = 0; i < SECTOR_SIZE; ++i) {
        expected[i] = (unsigned char)(0xC0 + i + 2);
    }

    zassert_s32_ne(0, CdGetSector(header, sizeof(header) / 4));
    zexpect_u8array_eq(expected + 12, header, sizeof(header));

    zassert_s32_ne(0, CdGetSector(payload, sizeof(payload) / 4));
    zexpect_u8array_eq(expected + 24, payload, sizeof(payload));

    g_read_cb_fails = 1;
    start_read_at(0);
    zexpect_s32_eq(CdlDiskError, CdReady(1, NULL));

    CdControlB(CdlStop, NULL, NULL);
    Psyz_CdSetReadCB(NULL);
}

ZTEST(libcd, readn_in_rt_mode_leaves_the_data_path_idle) {
    u_char mode = CdlModeRT;
    load_cue_data(4);

    CdControlB(CdlSetmode, &mode, NULL);
    start_read_at(0);

    zexpect_s32_eq(CdlNoIntr, CdReady(1, NULL));

    CdControlB(CdlStop, NULL, NULL);
    mode = 0;
    CdControlB(CdlSetmode, &mode, NULL);
}

static void mount_bin_cue_pair(
    const unsigned short* data, size_t count, const char* track_type) {
    char cdda_path[PATH_LEN];
    char body[256];
    FILE* f;
    snprintf(cdda_path, sizeof(cdda_path), "%s/cdda.bin", dir);
    f = fopen(cdda_path, "wb");
    if (f == NULL) {
        zprintf("fopen %s\n", cdda_path);
    }
    zassert_ptr_ne(NULL, f);
    fwrite(data, sizeof(unsigned short), count, f);
    fclose(f);
    add_bin(cdda_path);

    snprintf(cue, sizeof(cue), "%s/cdda.cue", dir);
    snprintf(body, sizeof(body),
             "FILE \"cdda.bin\" BINARY\n"
             "  TRACK 01 %s\n"
             "    INDEX 01 00:00:00\n",
             track_type);
    write_text(cue, body);
    mount_cue();
}

// Build a deterministic raw 2352-byte MODE2/2352 XA Form2 sector.
// All ADPCM blocks use shift_in=0 (so shift=12) and filter=0, and every
// nibble is 0x1, so each decoded sample = (1 << 12) = 0x1000 (DC). After
// CD volume (0x3FFF) and main volume (0x3FFF) this comes out as 0x07FE.
static void build_xa_sector_file(
    unsigned char* sector, int sector_idx, unsigned char file) {
    // subheader: channel=0, submode=0x64 (audio|RT|Form2), CI=0x01
    // (stereo, 37800Hz, 4-bit ADPCM)
    const unsigned char sh[4] = {file, 0x00, 0x64, 0x01};
    int i, b;
    memset(sector, 0, 2352);
    // sync
    sector[0] = 0x00;
    for (i = 1; i < 11; ++i)
        sector[i] = 0xFF;
    sector[11] = 0x00;
    // header (BCD MSF + mode 0x02). mm=0, ss=2, ff=sector_idx.
    sector[0x0C] = 0x00;
    sector[0x0D] = 0x02;
    sector[0x0E] = (unsigned char)sector_idx;
    sector[0x0F] = 0x02;
    memcpy(&sector[0x10], sh, 4);
    memcpy(&sector[0x14], sh, 4);
    // 18 ADPCM blocks at offset 0x18
    for (b = 0; b < 18; ++b) {
        unsigned char* blk = &sector[0x18 + b * 128];
        // bytes 0..3 are a copy of bytes 4..7 (sub-block headers)
        // header byte = (filter << 4) | shift_in = 0
        for (i = 0; i < 8; ++i)
            blk[i] = 0x00;
        // bytes 8..15 zero-padded (8-bit extended hdr unused for 4-bit)
        // bytes 16..127 = 28 words x 4 bytes of nibble-packed data.
        // every nibble = 0x1 -> byte = 0x11
        for (i = 16; i < 128; ++i)
            blk[i] = 0x11;
    }
    // remaining bytes (0x18+18*128 .. 0x92F) stay zero (padding + EDC)
}

static void build_xa_sector(unsigned char* sector, int sector_idx) {
    build_xa_sector_file(sector, sector_idx, 0x01);
}

static int make_dir(const char* path) {
#ifndef _WIN32
    return mkdir(path, 0755);
#else
    return _mkdir(path);
#endif
}

ZTEST_SETUP(libcd_playback) {
    int ret;
    reset_state("psyz_libcdplayback_test_");
    ret = make_dir(dir);
    if (ret == -1) {
        zprintf("mkdir failed\n");
        zassert_s32_eq(EEXIST, errno);
    }
    Psyz_CdSetDiskPath(NULL);
    Psyz_CdShellOpen(0);
}

ZTEST_TEARDOWN(libcd_playback) {
    Psyz_CdSetDiskPath(NULL);
    Psyz_CdShellOpen(0);
    Psyz_AudioDestroy();
    cleanup_files();
}

ZTEST(libcd_playback, cdda_playback) {
    // 2 channels * 2 seconds * 44100 frames/s = 176400 stereo frames
    const int channels = 2;
    const int frame_count = channels * PSYZ_SPU_SAMPLE_RATE;
    unsigned short* sample;
    CdlATV fake_vol = {11, 22, 33, 44};
    u_char param[8];
    u_char td_param[4] = {itob(1), 0, 0, 0};
    u_char td_result[4] = {0};
    CdlLOC loc;
    s16* out;
    int i;
    sample = test_calloc(frame_count, sizeof(unsigned short));
    for (i = 0; i < frame_count; i++) {
        sample[i] = 0x6000u;
    }
    mount_bin_cue_pair(sample, frame_count, "AUDIO");

    // Set arbitrary CD volume that will get reset with CdReset(1) anyway
    CdMix(&fake_vol);

    // Initialize CD and audio systems
    CdReset(1);

    // Set CD-DA mode (required for CDDA playback)
    param[0] = CdlModeDA;
    CdControl(CdlSetmode, param, NULL);

    // Get start sector of track 1 via CdlGetTD
    zassert_s32_eq(CdlDataReady, CdControlB(CdlGetTD, td_param, td_result));

    // Build CdlLOC from BCD MM:SS returned by CdlGetTD (sector = 0)
    loc.minute = td_result[1];
    loc.second = td_result[2];
    loc.sector = 0x00;
    loc.track = 0x00;
    CdControlB(CdlSeekP, (u_char*)&loc, NULL);

    // Stop the SDL audio thread from pulling samples and take the lock so we
    // drive Psyz_SpuPullSamples deterministically and avoid race conditions.
    Psyz_AudioPause();
    Psyz_AudioLock();

    // Start actual CDDA playback. CdlPlay does not touch the SDL pause state,
    // so the stream stays paused until the test releases.
    CdControlB(CdlPlay, NULL, NULL);

    // Pull samples from the SPU that would've been otherwise read from the HW
    out = test_calloc(frame_count * 2, sizeof(s16));
    Psyz_SpuPullSamples(out, frame_count);

    // Reverse Psyz_AudioLock
    Psyz_AudioUnlock();

    for (i = 0; i < frame_count / 2; i++) {
        if (out[i * 2 + 0] != 0x2FFE) {
            zprintf("frame %d left channel unexpected value\n", i);
        }
        zassert_s16_eq(0x2FFE, out[i * 2 + 0]);
        if (out[i * 2 + 1] != 0x2FFE) {
            zprintf("frame %d right channel unexpected value\n", i);
        }
        zassert_s16_eq(0x2FFE, out[i * 2 + 1]);
    }
}

ZTEST(libcd_playback, xa_playback) {
    // Each MODE2/2352 XA Form2 sector decodes to 18*4*28 = 2016 stereo frames
    // at 37800 Hz, which yields 2016 * 44100/37800 = 2352 output frames at
    // 44100 Hz. 16 sectors -> ~37632 output frames.
    const int kSectors = 16;
    const int frame_count = 37632;
    // Skip the first 64 frames where the Hermite ring is filling and the
    // resampler is settling toward the DC plateau.
    const int kSettle = 64;
    unsigned short* sample;
    unsigned char* raw;
    u_char param[8];
    CdlLOC loc;
    short* out;
    int i;
    sample = test_calloc(kSectors * 2352 / 2, sizeof(unsigned short));
    raw = (unsigned char*)sample;
    for (i = 0; i < kSectors; i++) {
        build_xa_sector(raw + i * 2352, i);
    }
    mount_bin_cue_pair(sample, kSectors * 2352 / 2, "MODE2/2352");

    // Initialize CD and audio systems
    CdReset(1);

    // Set XA-ADPCM mode at double-speed with channel filtering enabled.
    param[0] = CdlModeSpeed | CdlModeRT | CdlModeSF;
    CdControlB(CdlSetmode, param, NULL);

    // Filter to file=1 channel=0, matching the synthesized subheader.
    param[0] = 1;
    param[1] = 0;
    CdControlB(CdlSetfilter, param, NULL);

    CdIntToPos(0, &loc);
    CdControl(CdlSetloc, (u_char*)&loc, NULL);

    // Stop the SDL audio thread from pulling samples and take the lock so we
    // drive Psyz_SpuPullSamples deterministically and avoid race conditions.
    Psyz_AudioPause();
    Psyz_AudioLock();

    // Kick off XA-ADPCM streaming.
    CdControl(CdlReadN, NULL, NULL);

    out = test_calloc(frame_count * 2, sizeof(short));
    Psyz_SpuPullSamples(out, frame_count);

    // Reverse Psyz_AudioLock
    Psyz_AudioUnlock();

    // Each XA sample decodes to 0x1000 -> after cd_vol (0x3FFF >> 15) and
    // main_vol (0x3FFF >> 14) the steady-state output is 0x07FE per channel.
    for (i = kSettle; i < frame_count; ++i) {
        if (out[i * 2 + 0] != 0x07FE) {
            zprintf("frame %d left channel unexpected value\n", i);
        }
        zassert_s16_eq(0x07FE, out[i * 2 + 0]);
        if (out[i * 2 + 1] != 0x07FE) {
            zprintf("frame %d right channel unexpected value\n", i);
        }
        zassert_s16_eq(0x07FE, out[i * 2 + 1]);
    }
}

ZTEST(libcd_playback, data_read_interrupts_xa_playback) {
    const int kSectors = 16;
    const int kFrames = 512;
    unsigned short* sample;
    unsigned char* raw;
    u_char param[8];
    CdlLOC loc;
    short* out;
    short* drain;
    bool streaming = false;
    int i;
    sample = test_calloc(kSectors * 2352 / 2, sizeof(unsigned short));
    raw = (unsigned char*)sample;
    for (i = 0; i < kSectors; i++) {
        build_xa_sector(raw + i * 2352, i);
    }
    mount_bin_cue_pair(sample, kSectors * 2352 / 2, "MODE2/2352");

    CdReset(1);

    param[0] = CdlModeSpeed | CdlModeRT | CdlModeSF;
    CdControlB(CdlSetmode, param, NULL);
    param[0] = 1;
    param[1] = 0;
    CdControlB(CdlSetfilter, param, NULL);

    CdIntToPos(0, &loc);
    CdControl(CdlSetloc, (u_char*)&loc, NULL);

    Psyz_AudioPause();
    Psyz_AudioLock();

    out = test_calloc(kFrames * 2, sizeof(short));
    CdControl(CdlReadN, NULL, NULL);
    Psyz_SpuPullSamples(out, kFrames);

    for (i = 0; i < kFrames * 2; i++) {
        if (out[i] != 0) {
            streaming = true;
            break;
        }
    }
    zprintf("XA stream produced no audio to interrupt\n");
    zassert_s32_ne(0, streaming);

    param[0] = CdlModeSpeed;
    CdControlB(CdlSetmode, param, NULL);
    CdControl(CdlReadN, NULL, NULL);
    zassert_s32_eq(CdlDataReady, CdReady(1, NULL));

    drain = test_calloc(4096 * 2, sizeof(short));
    Psyz_SpuPullSamples(drain, 4096);

    for (i = 0; i < kFrames * 2; i++) {
        out[i] = (short)0x1234;
    }
    Psyz_SpuPullSamples(out, kFrames);
    Psyz_AudioUnlock();

    for (i = 0; i < kFrames * 2; ++i) {
        if (out[i] != 0) {
            zprintf("XA kept streaming after a data read, sample %d\n", i);
        }
        zassert_s16_eq(0, out[i]);
    }
}

ZTEST(libcd_playback, xa_filtered_stream_ends_in_silence) {
    // Mirrors back-to-back voice clips: a short clip in file 1, the next clip
    // in file 2, then unrelated audio that happens to reuse file 1 further
    // into the disc. Once file 1 runs out the filter rejects every sector, so
    // hardware goes silent instead of jumping to the later file 1 sectors.
    const int kClip = 8;
    const int kOther = 128;
    const int kLater = 8;
    const int kSectors = kClip + kOther + kLater;
    const int kClipFrames = kClip * 2352; // output frames at 44100 Hz
    const int frame_count = kClipFrames * 3;
    const int kSettle = 64;
    unsigned short* sample;
    unsigned char* raw;
    u_char param[8];
    CdlLOC loc;
    short* out;
    int i;
    sample = test_calloc(kSectors * 2352 / 2, sizeof(unsigned short));
    raw = (unsigned char*)sample;
    for (i = 0; i < kSectors; i++) {
        unsigned char file = (i < kClip || i >= kClip + kOther) ? 1 : 2;
        build_xa_sector_file(raw + i * 2352, i, file);
    }
    mount_bin_cue_pair(sample, kSectors * 2352 / 2, "MODE2/2352");

    CdReset(1);

    param[0] = CdlModeSpeed | CdlModeRT | CdlModeSF;
    CdControlB(CdlSetmode, param, NULL);
    param[0] = 1;
    param[1] = 0;
    CdControlB(CdlSetfilter, param, NULL);

    CdIntToPos(0, &loc);
    CdControl(CdlSetloc, (u_char*)&loc, NULL);

    Psyz_AudioPause();
    Psyz_AudioLock();

    CdControl(CdlReadN, NULL, NULL);

    out = test_calloc(frame_count * 2, sizeof(short));
    Psyz_SpuPullSamples(out, frame_count);

    Psyz_AudioUnlock();

    for (i = kSettle; i < kClipFrames - kSettle; ++i) {
        if (out[i * 2 + 0] != 0x07FE || out[i * 2 + 1] != 0x07FE) {
            zprintf("clip frame %d unexpected value\n", i);
        }
        zassert_s16_eq(0x07FE, out[i * 2 + 0]);
        zassert_s16_eq(0x07FE, out[i * 2 + 1]);
    }
    for (i = kClipFrames + kSettle; i < frame_count; ++i) {
        if (out[i * 2 + 0] != 0 || out[i * 2 + 1] != 0) {
            zprintf("frame %d played audio past the end of the clip\n", i);
        }
        zassert_s16_eq(0, out[i * 2 + 0]);
        zassert_s16_eq(0, out[i * 2 + 1]);
    }
}
