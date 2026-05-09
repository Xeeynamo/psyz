#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include <psyz.h>
#include <libcd.h>
#include <libspu.h>
}

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#define rmdir(path) _rmdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

constexpr int SECTOR_SIZE = 2352;

void make_blob(const std::string& path, long bytes) {
    FILE* f = std::fopen(path.c_str(), "wb");
    ASSERT_NE(f, nullptr) << "fopen " << path;
    if (bytes > 0) {
        std::fseek(f, bytes - 1, SEEK_SET);
        std::fputc(0, f);
    }
    std::fclose(f);
}

void write_text(const std::string& path, const std::string& body) {
    FILE* f = std::fopen(path.c_str(), "wb");
    ASSERT_NE(f, nullptr) << "fopen " << path;
    std::fwrite(body.data(), 1, body.size(), f);
    std::fclose(f);
}

class LibCdTest : public ::testing::Test {
  protected:
    std::string dir;
    std::string cue;
    std::vector<std::string> bins;

    void SetUp() override {
        dir = std::string("psyz_libcd_test_") +
              std::to_string(std::time(nullptr));
        ASSERT_EQ(mkdir(dir.c_str(), 0755), 0) << dir;
        Psyz_SetDiskPath(nullptr);
        Psyz_CdShellOpen(0);
    }

    void TearDown() override {
        Psyz_SetDiskPath(nullptr);
        Psyz_CdShellOpen(0);
        for (const auto& b : bins) {
            std::remove(b.c_str());
        }
        if (!cue.empty()) {
            std::remove(cue.c_str());
        }
        rmdir(dir.c_str());
    }

    // Build a CUE describing N MODE2/2352 + audio tracks all sharing one BIN.
    // bin_sectors lets the caller fix the lead-out MSF.
    void load_cue_single(int track_count, long bin_sectors, bool has_data) {
        std::string bin = dir + "/disc.bin";
        make_blob(bin, bin_sectors * SECTOR_SIZE);
        bins.push_back(bin);

        std::string body = "FILE \"disc.bin\" BINARY\n";
        for (int i = 1; i <= track_count; ++i) {
            char buf[64];
            const char* type = "AUDIO";
            if (i == 1 && has_data) {
                type = "MODE2/2352";
            }
            std::snprintf(buf, sizeof(buf), "  TRACK %02d %s\n", i, type);
            body += buf;
            std::snprintf(buf, sizeof(buf), "    INDEX 01 00:00:00\n");
            body += buf;
        }

        cue = dir + "/disc.cue";
        write_text(cue, body);
        ASSERT_EQ(Psyz_SetDiskPath(cue.c_str()), 0)
            << "Psyz_SetDiskPath failed for " << cue;
        Psyz_CdShellOpen(0);
    }

    // CUE with one MODE2/2352 data track + N audio tracks each in its own BIN
    void load_cue_multi(
        int audio_track_count, long data_sectors, long audio_sectors_each) {
        std::string body;
        std::string data_bin = dir + "/data.bin";
        make_blob(data_bin, data_sectors * SECTOR_SIZE);
        bins.push_back(data_bin);
        body += "FILE \"data.bin\" BINARY\n";
        body += "  TRACK 01 MODE2/2352\n";
        body += "    INDEX 01 00:00:00\n";

        for (int i = 0; i < audio_track_count; ++i) {
            char fname[64];
            std::snprintf(fname, sizeof(fname), "audio%02d.bin", i + 1);
            std::string ab = dir + "/" + fname;
            make_blob(ab, audio_sectors_each * SECTOR_SIZE);
            bins.push_back(ab);

            char buf[128];
            std::snprintf(buf, sizeof(buf), "FILE \"%s\" BINARY\n", fname);
            body += buf;
            std::snprintf(buf, sizeof(buf), "  TRACK %02d AUDIO\n", i + 2);
            body += buf;
            body += "    INDEX 01 00:00:00\n";
        }

        cue = dir + "/disc.cue";
        write_text(cue, body);
        ASSERT_EQ(Psyz_SetDiskPath(cue.c_str()), 0)
            << "Psyz_SetDiskPath failed for " << cue;
        Psyz_CdShellOpen(0);
    }
};

// Mirrors the real-hardware capture: 1 MODE2/2352 track, 271,796 sectors.
TEST_F(LibCdTest, single_data_track_matches_real_hardware) {
    constexpr long kSectors = 271796; // 639,264,192 bytes / 2352
    load_cue_single(1, kSectors, true);

    u_char r[8];

    std::memset(r, 0xCC, sizeof(r));
    ASSERT_EQ(CdControlB(CdlNop, nullptr, r), CdlDataReady);
    EXPECT_EQ(r[0], CdlStatStandby);
    EXPECT_EQ(r[1], 0x00);
    EXPECT_EQ(r[2], 0x00);
    EXPECT_EQ(r[3], 0x00);

    std::memset(r, 0xCC, sizeof(r));
    ASSERT_EQ(CdControlB(CdlGetTN, nullptr, r), CdlDataReady);
    EXPECT_EQ(r[0], CdlStatStandby);
    EXPECT_EQ(r[1], 0x01);
    EXPECT_EQ(r[2], 0x01);
    EXPECT_EQ(r[3], 0x00);

    u_char sr[8];
    std::memset(sr, 0xCC, sizeof(sr));
    EXPECT_EQ(CdSync(1, sr), CdlComplete);
    EXPECT_EQ(sr[0], CdlStatStandby);
    EXPECT_EQ(sr[1], 0x01);
    EXPECT_EQ(sr[2], 0x01);
    EXPECT_EQ(r[3], 0x00);

    // GetTD 01 -> 00:02 BCD (track 1 INDEX 01 with +150 pregap)
    u_char p = 0x01;
    std::memset(r, 0xCC, sizeof(r));
    ASSERT_EQ(CdControlB(CdlGetTD, &p, r), CdlDataReady);
    EXPECT_EQ(r[0], CdlStatStandby);
    EXPECT_EQ(r[1], 0x00);
    EXPECT_EQ(r[2], 0x02);
    EXPECT_EQ(r[3], 0x00);

    // GetTD 00 -> lead-out 60:25 BCD
    p = 0x00;
    std::memset(r, 0xCC, sizeof(r));
    ASSERT_EQ(CdControlB(CdlGetTD, &p, r), CdlDataReady);
    EXPECT_EQ(r[0], CdlStatStandby);
    EXPECT_EQ(r[1], 0x60);
    EXPECT_EQ(r[2], 0x25);
    EXPECT_EQ(r[3], 0x00);

    // Sync(1) now mirrors the last GetTD response.
    std::memset(sr, 0xCC, sizeof(sr));
    EXPECT_EQ(CdSync(1, sr), CdlComplete);
    EXPECT_EQ(sr[0], CdlStatStandby);
    EXPECT_EQ(sr[1], 0x60);
    EXPECT_EQ(sr[2], 0x25);
    EXPECT_EQ(r[3], 0x00);
}

TEST_F(LibCdTest, data_plus_one_audio) {
    // 1 data file (75 sectors) + 1 audio file (75 sectors), each in its own
    // FILE entry — this matches how real PSX dumps lay out data + CDDA.
    // T1 abs_sector=0 (+150 -> 0:02), T2 abs_sector=75 (+150 -> 0:03),
    // lead-out=150 (+150 -> 0:04).
    load_cue_multi(1, 75, 75);

    u_char r[8];
    ASSERT_EQ(CdControlB(CdlGetTN, nullptr, r), CdlDataReady);
    EXPECT_EQ(r[0], CdlStatStandby);
    EXPECT_EQ(r[1], 0x01);
    EXPECT_EQ(r[2], 0x02);

    u_char p;
    p = 0x01;
    ASSERT_EQ(CdControlB(CdlGetTD, &p, r), CdlDataReady);
    EXPECT_EQ(r[0], CdlStatStandby);
    EXPECT_EQ(r[1], 0x00);
    EXPECT_EQ(r[2], 0x02);

    p = 0x02;
    ASSERT_EQ(CdControlB(CdlGetTD, &p, r), CdlDataReady);
    EXPECT_EQ(r[0], CdlStatStandby);
    EXPECT_EQ(r[1], 0x00);
    EXPECT_EQ(r[2], 0x03);

    p = 0x00;
    ASSERT_EQ(CdControlB(CdlGetTD, &p, r), CdlDataReady);
    EXPECT_EQ(r[0], CdlStatStandby);
    EXPECT_EQ(r[1], 0x00);
    EXPECT_EQ(r[2], 0x04);
}

TEST_F(LibCdTest, multi_audio_six_tracks) {
    // 1 data file (150 sec) + 5 audio files (75 sec each, separate FILE).
    // abs_sector layout: T1=0, T2=150, T3=225, T4=300, T5=375, T6=450,
    // lead-out=525.
    // +150 pregap then /75 -> seconds: T1=2, T2=4, T3=5, T4=6, T5=7, T6=8,
    // lead-out=9.
    load_cue_multi(5, 150, 75);

    u_char r[8];
    ASSERT_EQ(CdControlB(CdlGetTN, nullptr, r), CdlDataReady);
    EXPECT_EQ(r[1], 0x01);
    EXPECT_EQ(r[2], 0x06);

    auto check_td = [&](u_char bcd_track, u_char exp_min, u_char exp_sec) {
        u_char p = bcd_track;
        u_char rr[8];
        ASSERT_EQ(CdControlB(CdlGetTD, &p, rr), CdlDataReady)
            << "track " << (int)bcd_track;
        EXPECT_EQ(rr[0], CdlStatStandby) << "track " << (int)bcd_track;
        EXPECT_EQ(rr[1], exp_min) << "track " << (int)bcd_track;
        EXPECT_EQ(rr[2], exp_sec) << "track " << (int)bcd_track;
    };
    check_td(0x01, 0x00, 0x02);
    check_td(0x03, 0x00, 0x05);
    check_td(0x06, 0x00, 0x08);
    check_td(0x00, 0x00, 0x09); // lead-out
}

TEST_F(LibCdTest, ten_tracks_bcd_boundary) {
    // 10 tracks in one file forces BCD encoding 0x10.
    load_cue_single(10, 75 * 11, false);

    u_char r[8];
    ASSERT_EQ(CdControlB(CdlGetTN, nullptr, r), CdlDataReady);
    EXPECT_EQ(r[1], 0x01);
    EXPECT_EQ(r[2], 0x10) << "last track must be BCD-encoded";

    // Track 10: INDEX 01 in a single-file CUE places every track at file
    // offset 0, so all tracks share abs_sector 0 -> all report 00:02.
    // (CUE format detail; what we want to assert here is that a BCD-input
    // 0x10 track is accepted and not treated as track 16.)
    u_char p = 0x10;
    ASSERT_EQ(CdControlB(CdlGetTD, &p, r), CdlDataReady);
    EXPECT_EQ(r[1], 0x00);
    EXPECT_EQ(r[2], 0x02);
}

TEST_F(LibCdTest, no_disk_loaded_returns_error_response) {
    // Don't call load(); explicitly detach disc.
    Psyz_SetDiskPath(nullptr);

    u_char r[8];

    // Real-hardware "lid closed, no disk" trace (after settling):
    //   STAT 02 STANDBY (drive is empty but spindle eventually settles)
    //   GETTN 01 80 00 00 ret=NoIntr
    // Our model has no spin-up timeline: with no disc we treat the bay as
    // empty / lid-open-equivalent until a disc is provided. Assert error
    // shape rather than exact stat byte.
    std::memset(r, 0xCC, sizeof(r));
    EXPECT_EQ(CdControlB(CdlGetTN, nullptr, r), CdlNoIntr);
    EXPECT_NE(r[0] & CdlStatError, 0) << "stat byte must carry CmdErr bit";
    EXPECT_NE(r[1] & 0x80, 0) << "result[1] must carry error-response sentinel";

    u_char p = 0x01;
    std::memset(r, 0xCC, sizeof(r));
    EXPECT_EQ(CdControlB(CdlGetTD, &p, r), CdlNoIntr);
    EXPECT_NE(r[0] & CdlStatError, 0);
    EXPECT_NE(r[1] & 0x80, 0);
}

TEST_F(LibCdTest, invalid_track_number) {
    load_cue_single(1, 100, true);

    u_char p = 0x99; // BCD 99, far above any real track
    u_char r[8];
    std::memset(r, 0xCC, sizeof(r));
    EXPECT_EQ(CdControlB(CdlGetTD, &p, r), CdlNoIntr);
    EXPECT_NE(r[0] & CdlStatError, 0);
    EXPECT_NE(r[1] & 0x80, 0);
}

TEST_F(LibCdTest, shell_open_status) {
    load_cue_single(1, 100, true);
    Psyz_CdShellOpen(1);

    u_char r[8];

    // Nop reports current stat with shell-open bit, no error sentinel.
    std::memset(r, 0xCC, sizeof(r));
    EXPECT_EQ(CdControlB(CdlNop, nullptr, r), CdlNoIntr);
    EXPECT_NE(r[0] & CdlStatShellOpen, 0);

    std::memset(r, 0xCC, sizeof(r));
    EXPECT_EQ(CdControlB(CdlGetTN, nullptr, r), CdlNoIntr);
    EXPECT_NE(r[0] & CdlStatShellOpen, 0)
        << "stat byte must carry shell-open bit while lid is open";
    EXPECT_NE(r[0] & CdlStatError, 0);
    EXPECT_NE(r[1] & 0x80, 0) << "error-response sentinel";

    u_char p = 0x01;
    std::memset(r, 0xCC, sizeof(r));
    EXPECT_EQ(CdControlB(CdlGetTD, &p, r), CdlNoIntr);
    EXPECT_NE(r[0] & CdlStatShellOpen, 0);
    EXPECT_NE(r[0] & CdlStatError, 0);
    EXPECT_NE(r[1] & 0x80, 0);
}

TEST_F(LibCdTest, shell_cycle_open_then_close) {
    load_cue_single(1, 100, true);

    u_char r[8];
    // Mirrors the full real-hardware lid-cycle:

    // 1. Steady state with disc: STAT 02 STANDBY, all reads succeed.
    ASSERT_EQ(CdControlB(CdlNop, nullptr, r), CdlDataReady);
    EXPECT_EQ(r[0], CdlStatStandby);

    // 2. Open lid: stat acquires shell-open bit, all reads fail.
    Psyz_CdShellOpen(1);
    ASSERT_EQ(CdControlB(CdlGetTN, nullptr, r), CdlNoIntr);
    EXPECT_NE(r[0] & CdlStatShellOpen, 0);

    // (3) Close lid: latch is sticky until acknowledged by a successful
    // command. The first CdlNop after re-close clears the latch and
    // re-asserts standby.
    Psyz_CdShellOpen(0);
    ASSERT_EQ(CdControlB(CdlNop, nullptr, r), CdlDataReady);
    EXPECT_EQ(r[0] & CdlStatShellOpen, 0)
        << "first successful command after re-close must clear shell-open";
    EXPECT_NE(r[0] & CdlStatStandby, 0)
        << "standby must be re-asserted after re-close";

    // Subsequent reads succeed normally.
    ASSERT_EQ(CdControlB(CdlGetTN, nullptr, r), CdlDataReady);
    EXPECT_EQ(r[0], CdlStatStandby);
    EXPECT_EQ(r[1], 0x01);
    EXPECT_EQ(r[2], 0x01);
}

// Sync(1) is a non-blocking peek. After a successful command it returns
// CdlComplete plus a copy of the cached last result. After a failing
// command the cached result carries the error-response sentinel.
TEST_F(LibCdTest, sync_peek_caches_last_response) {
    load_cue_single(1, 271796, true);

    u_char r[8];
    u_char sr[8];

    // Issue GetTN, then peek.
    ASSERT_EQ(CdControlB(CdlGetTN, nullptr, r), CdlDataReady);
    std::memset(sr, 0xCC, sizeof(sr));
    EXPECT_EQ(CdSync(1, sr), CdlComplete);
    EXPECT_EQ(sr[0], r[0]);
    EXPECT_EQ(sr[1], r[1]); // BCD first track
    EXPECT_EQ(sr[2], r[2]); // BCD last track

    // Issue a failing GetTD with an invalid track; peek must reflect the
    // error response.
    u_char p = 0x99;
    ASSERT_EQ(CdControlB(CdlGetTD, &p, r), CdlNoIntr);
    std::memset(sr, 0xCC, sizeof(sr));
    EXPECT_EQ(CdSync(1, sr), CdlDiskError);
    EXPECT_NE(sr[1] & 0x80, 0);
}

class LibCdPlaybackTest : public ::testing::Test {
  protected:
    std::string dir;
    std::string cue;
    std::vector<std::string> bins;

    void SetUp() override {
        dir = std::string("psyz_libcdplayback_test_") +
              std::to_string(std::time(nullptr));
        ASSERT_EQ(mkdir(dir.c_str(), 0755), 0) << dir;
        Psyz_SetDiskPath(nullptr);
        Psyz_CdShellOpen(0);
    }

    void TearDown() override {
        Psyz_SetDiskPath(nullptr);
        Psyz_CdShellOpen(0);
        for (const auto& b : bins) {
            std::remove(b.c_str());
        }
        if (!cue.empty()) {
            std::remove(cue.c_str());
        }
        rmdir(dir.c_str());
    }

    void mount_bin_cue_pair(std::vector<unsigned short>& data) {
        std::string cdda_path = dir + "/cdda.bin";
        FILE* f = std::fopen(cdda_path.c_str(), "wb");
        ASSERT_NE(f, nullptr) << "fopen " << cdda_path;
        std::fwrite(data.data(), sizeof(unsigned short), data.size(), f);
        std::fclose(f);
        bins.push_back(cdda_path);

        cue = dir + "/cdda.cue";
        write_text(cue, "FILE \"cdda.bin\" BINARY\n"
                        "  TRACK 01 AUDIO\n"
                        "    INDEX 01 00:00:00\n");
        ASSERT_EQ(Psyz_SetDiskPath(cue.c_str()), 0)
            << "Psyz_SetDiskPath failed for " << cue;
        Psyz_CdShellOpen(0);
    }
};

TEST_F(LibCdPlaybackTest, cdda_playback) {
    // 2 channels * 2 seconds * 44100 frames/s = 176400 stereo frames
    constexpr int channels = 2;
    constexpr int frame_count = channels * PSYZ_SPU_SAMPLE_RATE;
    std::vector<unsigned short> sample(frame_count);
    for (size_t i = 0; i < frame_count; i++) {
        sample[i] = 0x6000u;
    }
    mount_bin_cue_pair(sample);

    // Set arbitrary CD volume that will get reset with CdReset(1) anyway
    CdlATV fake_vol = {11, 22, 33, 44};
    CdMix(&fake_vol);

    // Initialize CD and audio systems
    CdReset(1);

    // Set CD-DA mode (required for CDDA playback)
    u_char param[8];
    param[0] = CdlModeDA;
    CdControl(CdlSetmode, param, nullptr);

    // Get start sector of track 1 via CdlGetTD
    u_char td_param[4] = {itob(1), 0, 0, 0};
    u_char td_result[4] = {};
    ASSERT_EQ(CdControlB(CdlGetTD, td_param, td_result), CdlDataReady);

    // Build CdlLOC from BCD MM:SS returned by CdlGetTD (sector = 0)
    CdlLOC loc;
    loc.minute = td_result[1];
    loc.second = td_result[2];
    loc.sector = 0x00;
    loc.track = 0x00;
    CdControlB(CdlSeekP, (u_char*)&loc, nullptr);

    // Stop the SDL audio thread from pulling samples and take the lock so we
    // drive Psyz_SpuPullSamples deterministically and avoid race conditions.
    Psyz_AudioPause();
    Psyz_AudioLock();

    // Start actual CDDA playback. CdlPlay does not touch the SDL pause state,
    // so the stream stays paused until the test releases.
    CdControlB(CdlPlay, nullptr, nullptr);

    // Pull samples from the SPU that would've been otherwise read from the HW
    std::vector<s16> out(frame_count * 2, 0);
    Psyz_SpuPullSamples(out.data(), frame_count);

    // Reverse Psyz_AudioLock
    Psyz_AudioUnlock();

    for (int i = 0; i < frame_count / 2; i++) {
        ASSERT_EQ(out[i * 2 + 0], 0x2FFE)
            << "frame " << i << " left channel unexpected value";
        ASSERT_EQ(out[i * 2 + 1], 0x2FFE)
            << "frame " << i << " right channel unexpected value";
    }
}

} // namespace
