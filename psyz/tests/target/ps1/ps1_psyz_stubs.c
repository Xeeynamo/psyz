// PsyZ backend stubs for a native PS1 build
#include <psyz.h>
#include <libetc.h>
#include <libgpu.h>
#include <malloc.h>
#include <string.h>

#include "../../../src/psyz/mdec.h"
#include "ztest.h"

#include "common/syscalls/syscalls.h"

static uint32_t psx_mdec_read(unsigned offset) {
    return *(volatile uint32_t*)(0xBF801820u + offset);
}

static void psx_mdec_write(unsigned offset, uint32_t value) {
    *(volatile uint32_t*)(0xBF801820u + offset) = value;
}

static struct Ps1MdecTables {
    uint8_t quant[128], scale[128];
} mdec_tables, mdec_transfer_tables;
static uint32_t mdec_input[UINT16_MAX];
static size_t mdec_input_words, mdec_sent;
static uint32_t mdec_command;
static int mdec_started, mdec_error;
static uint8_t mdec_pixels[768];
static size_t mdec_pixel_pos, mdec_pixel_size;

void Psyz_MdecReset(void) {
    psx_mdec_write(4, 0x80000000u);
    mdec_input_words = mdec_sent = 0;
    mdec_command = 0;
    mdec_started = mdec_error = 0;
    mdec_pixel_pos = mdec_pixel_size = 0;
}

int Psyz_MdecCommand(uint32_t command, const void* data, size_t words) {
    unsigned opcode = command >> 29;
    size_t expected = opcode == 2   ? ((command & 1) ? 32 : 16)
                      : opcode == 3 ? 32
                                    : command & UINT16_MAX;
    if (opcode < 1 || opcode > 3 || words != expected || (!data && words)) {
        Psyz_MdecReset();
        mdec_error = 1;
        return -1;
    }
    if (opcode == 2) {
        memcpy(mdec_tables.quant, data, words * 4);
        return 0;
    }
    if (opcode == 3) {
        memcpy(mdec_tables.scale, data, words * 4);
        return 0;
    }
    Psyz_MdecReset();
    if (words)
        memcpy(mdec_input, data, words * 4);
    mdec_transfer_tables = mdec_tables;
    mdec_input_words = words;
    mdec_command = command;
    return 0;
}

static int psx_mdec_table(uint32_t command, const uint8_t data[128]) {
    unsigned sent = 0;
    for (unsigned poll = 0; poll < 0x100000; ++poll) {
        uint32_t status = psx_mdec_read(4);
        if (sent <= 32 && !(status & (sent ? 0x40000000u : 0x20000000u))) {
            uint32_t word = command;
            if (sent)
                memcpy(&word, data + (sent - 1) * 4, 4);
            psx_mdec_write(0, word);
            ++sent;
        }
        if (sent == 33 && !(psx_mdec_read(4) & 0x20000000u))
            return 0;
    }
    return -1;
}

static int psx_mdec_next_pixels(void) {
    if (!mdec_command || !mdec_input_words)
        return -1;
    if (!mdec_started) {
        if (psx_mdec_table(0x40000001, mdec_transfer_tables.quant) ||
            psx_mdec_table(0x60000000, mdec_transfer_tables.scale))
            return -1;
        psx_mdec_write(0, mdec_command);
        mdec_started = 1;
    }
    unsigned depth = (mdec_command >> 27) & 3;
    const unsigned sizes[] = {8, 16, 192, 128};
    unsigned words = sizes[depth], received = 0;
    uint32_t raw[192];
    for (unsigned idle = 0; received < words; ++idle) {
        uint32_t status = psx_mdec_read(4);
        if (mdec_sent < mdec_input_words && !(status & 0x40000000u)) {
            psx_mdec_write(0, mdec_input[mdec_sent++]);
            idle = 0;
        }
        if (!(status & 0x80000000u)) {
            raw[received++] = psx_mdec_read(0);
            idle = 0;
        }
        if (idle == 0x100000) {
            zprintf("MDEC timeout: command=%08X status=%08X input=%u/%u "
                    "output=%u/%u\n",
                    mdec_command, status, (unsigned)mdec_sent,
                    (unsigned)mdec_input_words, received, words);
            return -1;
        }
    }
    if (depth < 2) {
        memcpy(mdec_pixels, raw, words * 4);
    } else {
        unsigned bytes_per_pixel = depth == 2 ? 3 : 2;
        for (unsigned tile = 0; tile < 4; ++tile)
            for (unsigned y = 0; y < 8; ++y)
                memcpy(mdec_pixels + ((tile / 2 * 8 + y) * 16 + tile % 2 * 8) *
                                         bytes_per_pixel,
                       (uint8_t*)raw + (tile * 64 + y * 8) * bytes_per_pixel,
                       8 * bytes_per_pixel);
    }
    mdec_pixel_pos = 0;
    mdec_pixel_size = words * 4;
    return 0;
}

int Psyz_MdecRead(void* data, size_t words) {
    if ((!data && words) || words > SIZE_MAX / 4) {
        mdec_error = 1;
        return -1;
    }
    uint8_t* dst = data;
    size_t left = words * 4;
    while (left) {
        if (mdec_error ||
            (mdec_pixel_pos == mdec_pixel_size && psx_mdec_next_pixels())) {
            memset(dst, 0, left);
            psx_mdec_write(4, 0x80000000u);
            mdec_error = 1;
            return -1;
        }
        size_t count = mdec_pixel_size - mdec_pixel_pos;
        if (count > left)
            count = left;
        memcpy(dst, mdec_pixels + mdec_pixel_pos, count);
        mdec_pixel_pos += count;
        dst += count;
        left -= count;
    }
    return 0;
}

int Psyz_VideoSetDitheringMode(PsyzDitherMode mode) {
    (void)mode;
    return 0;
}

int Psyz_VideoSetInternalResolution(unsigned multiplier) {
    (void)multiplier;
    return multiplier == 1 ? 0 : -1;
}

unsigned Psyz_VideoGetInternalResolution(void) { return 1; }

static inline unsigned char color_5to8(unsigned int c5) {
    return (unsigned char)((c5 * 255 + 15) / 31);
}

unsigned char* Psyz_VideoAllocCapturedFrame(int* w, int* h) {
    const unsigned VRAM_W = 1024;
    const unsigned VRAM_H = 512;

    DISPENV disp;
    GetDispEnv(&disp);

    int cw = disp.disp.w ? disp.disp.w : 256;
    int ch = disp.disp.h ? disp.disp.h : 240;
    int cx = disp.disp.x;
    int cy = disp.disp.y;

    if (cx < 0) {
        cx = 0;
    }
    if (cy < 0) {
        cy = 0;
    }
    if (cw <= 0 || cw > VRAM_W) {
        cw = 256;
    }
    if (ch <= 0 || ch > VRAM_H) {
        ch = 240;
    }
    if (cx + cw > VRAM_W) {
        cx = VRAM_W - cw;
    }
    if (cy + ch > VRAM_H) {
        cy = VRAM_H - ch;
    }

    const int row_px = (cw + 1) & ~1;
    unsigned long* vram = (unsigned long*)malloc(
        (unsigned)(row_px * ch) * sizeof(unsigned short));
    if (vram == 0) {
        return 0;
    }

    RECT r;
    r.x = (short)cx;
    r.y = (short)cy;
    r.w = (short)row_px;
    r.h = (short)ch;

    DrawSync(0);
    StoreImage(&r, vram);
    DrawSync(0);

    unsigned char* out = (unsigned char*)malloc((unsigned)(cw * ch * 3));
    if (out == 0) {
        free(vram);
        return 0;
    }

    // expand rgb555 to rgb888
    const unsigned short* src = (const unsigned short*)vram;
    unsigned char* p = out;
    for (int y = 0; y < ch; y++) {
        const unsigned short* row = &src[y * row_px];
        for (int x = 0; x < cw; x++) {
            const unsigned short c = row[x];
            *p++ = color_5to8(c & 0x1F);
            *p++ = color_5to8((c >> 5) & 0x1F);
            *p++ = color_5to8((c >> 10) & 0x1F);
        }
    }
    free(vram);

    if (w) {
        *w = cw;
    }
    if (h) {
        *h = ch;
    }
    return out;
}

// GP0 is a real register here.
// TODO: super-slow at the moment due to gpustat spin to ensure synchronization
void Psyz_GpuWriteGP0(unsigned int word) {
    volatile unsigned int* const gpustat = (volatile unsigned int*)0x1F801814;
    int spins = 0x100000;
    while ((*gpustat & (1u << 28)) == 0 && --spins > 0) {
    }
    *(volatile unsigned int*)0x1F801810 = word;
}

void PS1_EnterCriticalSection(void) { enterCriticalSection(); }
void PS1_ExitCriticalSection(void) { leaveCriticalSection(); }

static int (*adjust_path_cb)(char* dst, const char* src, int maxlen);

static void copy_path(char* dst, const char* src, int maxlen) {
    int i = 0;
    while (src[i] != '\0' && i < maxlen - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void truncate_filename(char* path) {
    const int max_filename_len = 20;
    char* filename = path;
    for (char* p = path; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\') {
            filename = p + 1;
        }
    }
    int len = 0;
    while (filename[len] != '\0') {
        len++;
    }
    if (len >= max_filename_len) {
        filename[max_filename_len - 1] = '\0';
    }
}

static void default_adjust_path(char* dst, const char* src, int maxlen) {
    int len = 0;
    while (src[len] != '\0') {
        len++;
    }
    copy_path(dst, src, maxlen);
    // rewrite a memory card prefix (bu00:, bu10:, ...) as a directory
    if (len >= 5 && src[0] == 'b' && src[1] == 'u' && src[4] == ':' &&
        maxlen > 4) {
        dst[4] = '/';
        if (dst[5] == '\0' || dst[5] == '*') { // handles 'bu00:*'
            dst[5] = '\0';
        }
    }
}

void Psyz_AdjustPath(char* dst, const char* src, int maxlen) {
    if (dst == 0 || src == 0 || maxlen <= 0) {
        return;
    }
    if (adjust_path_cb == 0 || adjust_path_cb(dst, src, maxlen) < 0) {
        default_adjust_path(dst, src, maxlen);
    }
    truncate_filename(dst);
}

void Psyz_AdjustPathCB(
    int (*callback)(char* dst, const char* src, int maxlen)) {
    adjust_path_cb = callback;
}

void Psyz_SpuInit(void) {}
void Psyz_SpuReset(int hot) { (void)hot; }

void Psyz_SpuWrite(unsigned int reg_offset, unsigned short value) {
    *(volatile unsigned short*)(0x1F801C00 + reg_offset) = value;
}

unsigned short Psyz_SpuRead(unsigned int reg_offset) {
    return *(volatile unsigned short*)(0x1F801C00 + reg_offset);
}

static unsigned int spu_transfer_addr;

unsigned int Psyz_SpuGetTransferAddr(void) { return spu_transfer_addr; }
void Psyz_SpuSetTransferAddr(unsigned int addr) { spu_transfer_addr = addr; }

void Psyz_SpuFifoWrite(unsigned short word) { (void)word; }

void Psyz_SpuFifoWriteBulk(const unsigned char* src, unsigned int size) {
    (void)src;
    (void)size;
}

void Psyz_SpuMemRead(unsigned int offset, void* dst, unsigned int size) {
    (void)offset;
    (void)dst;
    (void)size;
}

void Psyz_SpuMemWrite(unsigned int offset, const void* src, unsigned int size) {
    (void)offset;
    (void)src;
    (void)size;
}

// There is no host-visible copy of SPU RAM to hand out.
unsigned char* Psyz_SpuGetRam(void) { return 0; }

void Psyz_SpuPullSamples(short* out, int num_frames) {
    (void)out;
    (void)num_frames;
}

void Psyz_AudioPause(void) {}
void Psyz_AudioUnpause(void) {}
void Psyz_AudioLock(void) {}
void Psyz_AudioUnlock(void) {}

int Psyz_GpuRegisterCommandHandler(
    unsigned int opcode, PsyzGpuCommandHandler handler, void* userdata) {}

int Psyz_GpuSetHorizontalGrid(
    unsigned int source_width, unsigned int target_width) {}
