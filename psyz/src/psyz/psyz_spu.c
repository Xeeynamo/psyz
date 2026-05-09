#include <psyz.h>
#include <psyz/log.h>
#include <string.h>
#include "../../decomp/src/libspu/libspu_private.h"

static short clamp16(int v) {
    if (v < -32768)
        return (short)-32768;
    if (v > 32767)
        return (short)32767;
    return (short)v;
}

static short clamp15(int v) {
    if (v < -16384)
        return (short)-16384;
    if (v > 16383)
        return (short)16383;
    return (short)v;
}

#define N_CHANNELS 2                      // stereo interleaved
#define CD_RING_FRAMES 4096               // must be power of 2
#define CD_RING_MASK (CD_RING_FRAMES - 1) // ring wrapper
#define CD_RING_LOW_WATER 1024            // refill when short of N frames

// Full SPU state
static struct {
    u8 ram[PSYZ_SPU_RAM_SIZE];

    // Transfer address (byte address into SPU RAM)
    u32 transfer_addr;

    // Capture buffer position
    u16 capture_pos;

    // CD audio ring buffer + pull callback
    s16 cd_ring[CD_RING_FRAMES * N_CHANNELS];
    u32 cd_ring_read;
    u32 cd_ring_count; // number of valid frames in ring
    void* cd_audio_userdata;

    u8 initialized;
} spu;

u8* Psyz_SpuGetRam(void) { return spu.ram; }

void Psyz_SpuInit(void) {
    if (spu.initialized)
        return;
    memset(&spu, 0, sizeof(spu));
    spu.initialized = 1;
    INFOF("SPU emulation initialized");
}

void Psyz_SpuReset(int hot) {
    u8 saved_ram[PSYZ_SPU_RAM_SIZE];

    if (hot) {
        memcpy(saved_ram, spu.ram, PSYZ_SPU_RAM_SIZE);
    }
    memset(&spu, 0, sizeof(spu));
    spu.initialized = 1;

    if (hot) {
        memcpy(spu.ram, saved_ram, PSYZ_SPU_RAM_SIZE);
    }
}

void Psyz_SpuSetTransferAddr(unsigned int addr) {
    spu.transfer_addr = addr & (PSYZ_SPU_RAM_SIZE - 1);
}

void Psyz_SpuWriteToRam(const unsigned char* src, unsigned int size) {
    for (unsigned int i = 0; i < size; i++) {
        spu.ram[spu.transfer_addr] = src[i];
        spu.transfer_addr = (spu.transfer_addr + 1) & (PSYZ_SPU_RAM_SIZE - 1);
    }
}

static void write_capture(unsigned int idx, short val) {
    unsigned int addr = (idx * 0x400) | spu.capture_pos;
    if (addr < PSYZ_SPU_RAM_SIZE) {
        memcpy(&spu.ram[addr], &val, sizeof(val));
    }
}

// generate one frame at 44100hz with voices mix, cd playback and volume control
static void spu_tick(short* out) {
    SPU_RXX* rxx = (SPU_RXX*)_spu_RXX;
    unsigned short spucnt = rxx->spucnt;

    // accumulate frame to each separate channels
    int left_sum = 0, right_sum = 0;

    // Pre-fill CD ring buffer if running low
    if (spu.cd_ring_count < CD_RING_LOW_WATER) {
        size_t space = CD_RING_FRAMES - spu.cd_ring_count;
        size_t write_pos =
            (spu.cd_ring_read + spu.cd_ring_count) & CD_RING_MASK;
        // Fill into contiguous chunk up to end of array
        size_t chunk = CD_RING_FRAMES - write_pos;
        if (chunk > space)
            chunk = space;
        size_t got = Psyz_CdPullSamples(&spu.cd_ring[write_pos * 2], chunk);
        spu.cd_ring_count += got;
    }

    // Read one stereo frame from CD ring buffer
    int cd_left = 0, cd_right = 0;
    if (spu.cd_ring_count > 0) {
        cd_left = spu.cd_ring[spu.cd_ring_read * 2];
        cd_right = spu.cd_ring[spu.cd_ring_read * 2 + 1];
        spu.cd_ring_read = (spu.cd_ring_read + 1) & CD_RING_MASK;
        spu.cd_ring_count--;
    }

    // TODO: voice mixing, populate left_sum/right_sum before this branch.

    // Mute all voices. CD audio is mixed after, and not affected by mute.
    if (!(spucnt & SPU_CTRL_MASK_MUTE_SPU)) {
        left_sum = right_sum = 0;
    }

    // Mix CD audio per SPUCNT and cd_vol registers.
    if (spucnt & SPU_CTRL_MASK_CD_AUDIO_ENABLE) {
        left_sum += (cd_left * rxx->cd_vol.left) >> 15;
        right_sum += (cd_right * rxx->cd_vol.right) >> 15;
    }

    out[0] = clamp16((left_sum * clamp15(rxx->main_vol.left)) >> 14);
    out[1] = clamp16((right_sum * clamp15(rxx->main_vol.right)) >> 14);

    // Capture buffers back to the SPU RAM
    write_capture(0, (short)cd_left);
    write_capture(1, (short)cd_right);
    spu.capture_pos = (spu.capture_pos + 2) % 0x400;
}

void Psyz_SpuPullSamples(short* out, int num_frames) {
    if (!spu.initialized) {
        memset(out, 0, num_frames * 2 * sizeof(short));
        return;
    }
    for (int i = 0; i < num_frames; i++) {
        spu_tick(&out[i * 2]);
    }
}
