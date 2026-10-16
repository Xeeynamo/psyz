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

void Psyz_MdecWrite(unsigned offset, u32 value) {
    if (offset == 4 && (value & 0x80000000u))
        Psyz_MdecReset();
    if (offset == 0) {
        mdec_command = value;
        mdec_input_words = mdec_sent = 0;
        mdec_pixel_pos = mdec_pixel_size = 0;
        mdec_started = 1;
        mdec_error = 0;
    }
    psx_mdec_write(offset, value);
}

int Psyz_MdecDmaIn(const void* data, size_t words) {
    unsigned opcode = mdec_command >> 29;
    if ((!data && words) || words > UINT16_MAX)
        return -1;
    if (opcode == 1) {
        memcpy(mdec_input, data, words * 4);
        mdec_input_words = words;
        return 0;
    }
    if ((opcode != 2 && opcode != 3) || words > 32)
        return -1;
    size_t sent = 0;
    for (unsigned idle = 0; idle < 0x100000; ++idle) {
        if (sent < words && !(psx_mdec_read(4) & 0x40000000u)) {
            uint32_t word;
            memcpy(&word, (const uint8_t*)data + sent * 4, 4);
            psx_mdec_write(0, word);
            ++sent;
            idle = 0;
        }
        if (sent == words && !(psx_mdec_read(4) & 0x20000000u)) {
            memcpy(opcode == 2 ? mdec_tables.quant : mdec_tables.scale, data,
                   words * 4);
            return 0;
        }
    }
    return -1;
}

int Psyz_MdecDmaOut(void* data, size_t words) {
    return Psyz_MdecRead(data, words);
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

// pre-calculated version of (c5 * 255 + 15) / 31
static const unsigned char color_5to8[32] = {
    0,   8,   16,  25,  33,  41,  49,  58,  66,  74,  82,
    90,  99,  107, 115, 123, 132, 140, 148, 156, 165, 173,
    181, 189, 197, 206, 214, 222, 230, 239, 247, 255};

zimage zimage_frontbuffer(void) {
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
    zimage img = zimage_alloc(row_px, ch, ZIMAGE_R5G5B5);
    if (img.data == 0) {
        return img;
    }

    RECT r;
    r.x = (short)cx;
    r.y = (short)cy;
    r.w = (short)row_px;
    r.h = (short)ch;

    DrawSync(0);
    StoreImage(&r, (unsigned long*)img.data);
    DrawSync(0);
    img.width = cw;
    return img;
}

unsigned char* Psyz_VideoAllocCapturedFrame(int* w, int* h) {
    zimage img = zimage_frontbuffer();
    if (img.data == 0) {
        return 0;
    }

    const int cw = img.width;
    const int ch = img.height;
    unsigned char* out = (unsigned char*)malloc((unsigned)(cw * ch * 3));
    if (out == 0) {
        zimage_free(&img);
        return 0;
    }

    // expand rgb555 to rgb888
    unsigned char* p = out;
    for (int y = 0; y < ch; y++) {
        const unsigned short* row =
            (const unsigned short*)(img.data + y * img.stride);
        for (int x = 0; x < cw; x++) {
            const unsigned short c = row[x];
            *p++ = color_5to8[c & 0x1F];
            *p++ = color_5to8[(c >> 5) & 0x1F];
            *p++ = color_5to8[(c >> 10) & 0x1F];
        }
    }
    zimage_free(&img);

    if (w) {
        *w = cw;
    }
    if (h) {
        *h = ch;
    }
    return out;
}

// GP0 is a real register here. Requires a DrawSync to ensure they're committed
void Psyz_GpuWriteGP0(unsigned int word) {
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

// PCDrv calls are break instructions that the UniROM debug core forwards to
// the host. A FILE* is the host file descriptor plus one.
#define PCDRV(code, arg0, arg1, arg2, arg3)                                    \
    ({                                                                         \
        register int a0 __asm__("a0") = (int)(arg0);                           \
        register int a1 __asm__("a1") = (int)(arg1);                           \
        register int a2 __asm__("a2") = (int)(arg2);                           \
        register int a3 __asm__("a3") = (int)(arg3);                           \
        register int v0 __asm__("v0");                                         \
        register int v1 __asm__("v1");                                         \
        __asm__ volatile("break 0, " #code "\n"                                \
                         : "=r"(v0), "=r"(v1)                                  \
                         : "r"(a0), "r"(a1), "r"(a2), "r"(a3)                  \
                         : "memory");                                          \
        v0 == 0 ? v1 : -1;                                                     \
    })
#define FILE_FD(f) ((int)(f) - 1)

FILE* fopen(const char* path, const char* mode) {
    int fd = mode[0] == 'w' ? PCDRV(0x102, path, path, 0, 0)
                            : PCDRV(0x103, path, path, 0, 0);
    return fd < 0 ? NULL : (FILE*)(fd + 1);
}

size_t fread(void* p, size_t size, size_t n, FILE* f) {
    int got = PCDRV(0x105, 0, FILE_FD(f), size * n, p);
    return got < 0 ? 0 : (size_t)got / size;
}

size_t fwrite(const void* p, size_t size, size_t n, FILE* f) {
    int put = PCDRV(0x106, 0, FILE_FD(f), size * n, p);
    return put < 0 ? 0 : (size_t)put / size;
}

int fclose(FILE* f) {
    PCDRV(0x104, FILE_FD(f), FILE_FD(f), 0, 0);
    return 0;
}

int fseek(FILE* f, long offset, int whence) {
    return PCDRV(0x107, FILE_FD(f), FILE_FD(f), offset, whence) < 0 ? -1 : 0;
}

long ftell(FILE* f) {
    return PCDRV(0x107, FILE_FD(f), FILE_FD(f), 0, SEEK_CUR);
}

#define SPU_REG(offset) (*(volatile unsigned short*)(0x1F801C00 + (offset)))
#define SPU_DMA_MADR (*(volatile unsigned int*)0x1F8010C0)
#define SPU_DMA_BCR (*(volatile unsigned int*)0x1F8010C4)
#define SPU_DMA_CHCR (*(volatile unsigned int*)0x1F8010C8)
#define DMA_DPCR (*(volatile unsigned int*)0x1F8010F0)
#define DMA_DICR (*(volatile unsigned int*)0x1F8010F4)
#define SPU_BUS_DELAY (*(volatile unsigned int*)0x1F801014)

enum {
    SPU_VOICE_ADSR2 = 0x0A,
    SPU_KEY_OFF = 0x18C,
    SPU_XFER_ADDR = 0x1A6,
    SPU_XFER_FIFO = 0x1A8,
    SPU_CTRL = 0x1AA,
    SPU_XFER_CTRL = 0x1AC,
    SPU_STAT = 0x1AE,

    SPU_XFER_STOP = 0,
    SPU_XFER_MANUAL_WRITE = 1,
    SPU_XFER_DMA_READ = 3,
    SPU_STAT_BUSY = 0x400,
    SPU_FIFO_BYTES = 64,
    SPU_POLL_LIMIT = 0x100000,
};

// The frames the SPU sends to its DAC, recorded by the host from the reset on.
static FILE* spu_tap;
static int spu_capture_on;
// Polls of SPUSTAT that take as long as 256 SPU frames, measured once.
static unsigned int spu_polls_per_256_frames;

static void spu_transfer_mode(unsigned mode) {
    SPU_REG(SPU_CTRL) = (SPU_REG(SPU_CTRL) & ~0x30) | (mode << 4);
    for (int poll = 0; poll < SPU_POLL_LIMIT; poll++) {
        if ((SPU_REG(SPU_STAT) & 0x30) == mode << 4) {
            return;
        }
    }
    zprintf("SPU transfer mode %u never applied\n", mode);
}

static void spu_wait_idle(void) {
    for (int poll = 0; poll < SPU_POLL_LIMIT; poll++) {
        if (!(SPU_REG(SPU_STAT) & SPU_STAT_BUSY)) {
            return;
        }
    }
    zprintf("SPU transfer never finished\n");
}

void Psyz_SpuInit(void) {}

// Silences every voice. SPU RAM is left alone: clearing it takes too long.
static void spu_tap_close(void) {
    if (spu_tap) {
        fclose(spu_tap);
        spu_tap = NULL;
    }
}

// Bit 11 of SPUSTAT flips every 256 frames while the SPU is enabled.
static void spu_calibrate_polls(void) {
    unsigned short start = SPU_REG(SPU_STAT) & 0x800;
    unsigned int polls = 0;
    for (int poll = 0; (SPU_REG(SPU_STAT) & 0x800) == start; poll++) {
        if (poll == SPU_POLL_LIMIT) {
            zprintf("SPUSTAT bit 11 never flipped\n");
            return;
        }
    }
    start ^= 0x800;
    while ((SPU_REG(SPU_STAT) & 0x800) == start && polls < SPU_POLL_LIMIT) {
        polls++;
    }
    spu_polls_per_256_frames = polls;
}

static void spu_wait_frames(int num_frames) {
    unsigned int polls = spu_polls_per_256_frames * (unsigned int)num_frames;
    for (polls >>= 8; polls; polls--) {
        (void)SPU_REG(SPU_STAT);
    }
}

int PS1_SpuCaptureStart(void) {
    spu_capture_on = 1;
    spu_tap = fopen("spu_capture.raw", "rb");
    return spu_tap != NULL;
}

void PS1_SpuCaptureStop(void) {
    spu_capture_on = 0;
    spu_tap_close();
}

void Psyz_SpuReset(int hot) {
    (void)hot;
    spu_tap_close();
    for (int voice = 0; voice < 24; voice++) {
        for (int reg = 0; reg < 0x10; reg += 2) {
            SPU_REG((voice << 4) + reg) = 0;
        }
    }
    SPU_REG(SPU_KEY_OFF) = 0xFFFF;
    SPU_REG(SPU_KEY_OFF + 2) = 0x00FF;
    for (int reg = 0x180; reg < 0x188; reg += 2) {
        SPU_REG(reg) = 0;
    }
    for (int reg = 0x190; reg < 0x19C; reg += 2) {
        SPU_REG(reg) = 0;
    }
    for (int reg = 0x1B0; reg < 0x1B8; reg += 2) {
        SPU_REG(reg) = 0;
    }
    // RAM transfers and the capture buffers stop while the SPU is disabled.
    SPU_REG(SPU_CTRL) = 0x8000;
    SPU_REG(SPU_XFER_CTRL) = 4;
    if (!spu_polls_per_256_frames) {
        spu_calibrate_polls();
    }
    if (spu_capture_on) {
        spu_tap = fopen("spu_capture.raw", "rb");
    }
}

void Psyz_SpuWrite(unsigned int reg_offset, unsigned short value) {
    SPU_REG(reg_offset) = value;
}

unsigned short Psyz_SpuRead(unsigned int reg_offset) {
    return SPU_REG(reg_offset);
}

static unsigned int spu_transfer_addr;

unsigned int Psyz_SpuGetTransferAddr(void) { return spu_transfer_addr; }
void Psyz_SpuSetTransferAddr(unsigned int addr) { spu_transfer_addr = addr; }

void Psyz_SpuFifoWrite(unsigned short word) { (void)word; }

void Psyz_SpuFifoWriteBulk(const unsigned char* src, unsigned int size) {
    (void)src;
    (void)size;
}

// The transfer address register counts in 8-byte units: offset is rounded down.
void Psyz_SpuMemRead(unsigned int offset, void* dst, unsigned int size) {
    static unsigned int words[256];
    unsigned char* out = dst;
    // libspu waits on the SPU DMA interrupt: this transfer must not raise it.
    unsigned int dicr = DMA_DICR & 0x00FFFFFF;
    DMA_DICR = dicr & ~0x00100000;
    while (size) {
        unsigned int skip = offset & 7;
        unsigned int count = sizeof(words) - skip;
        if (count > size) {
            count = size;
        }
        spu_transfer_mode(SPU_XFER_STOP);
        SPU_REG(SPU_XFER_ADDR) = (unsigned short)(offset >> 3);
        SPU_BUS_DELAY = (SPU_BUS_DELAY & 0xF0FFFFFF) | 0x22000000;
        spu_transfer_mode(SPU_XFER_DMA_READ);
        DMA_DPCR |= 0x000B0000;
        SPU_DMA_MADR = (unsigned int)words & 0x00FFFFFF;
        SPU_DMA_BCR = ((skip + count + 63) >> 6 << 16) | 16;
        SPU_DMA_CHCR = 0x01000200;
        for (int poll = 0; SPU_DMA_CHCR & 0x01000000; poll++) {
            if (poll == SPU_POLL_LIMIT) {
                zprintf("SPU DMA read never finished\n");
                break;
            }
        }
        SPU_BUS_DELAY = (SPU_BUS_DELAY & 0xF0FFFFFF) | 0x20000000;
        memcpy(out, (unsigned char*)words + skip, count);
        out += count;
        offset += count;
        size -= count;
    }
    spu_transfer_mode(SPU_XFER_STOP);
    DMA_DICR = dicr;
}

void Psyz_SpuMemWrite(unsigned int offset, const void* src, unsigned int size) {
    const unsigned char* in = src;
    while (size) {
        unsigned int count = size < SPU_FIFO_BYTES ? size : SPU_FIFO_BYTES;
        spu_transfer_mode(SPU_XFER_STOP);
        SPU_REG(SPU_XFER_ADDR) = (unsigned short)(offset >> 3);
        for (unsigned int i = 0; i < count; i += 2) {
            unsigned int high = i + 1 < count ? in[i + 1] : 0;
            SPU_REG(SPU_XFER_FIFO) = (unsigned short)(in[i] | (high << 8));
        }
        spu_transfer_mode(SPU_XFER_MANUAL_WRITE);
        spu_wait_idle();
        in += count;
        offset += count;
        size -= count;
    }
    spu_transfer_mode(SPU_XFER_STOP);
}

// There is no host-visible copy of SPU RAM to hand out.
unsigned char* Psyz_SpuGetRam(void) { return 0; }

// With a capture running, hands out the frames that followed the last ones
// read. Otherwise lets the frames go by and returns silence.
void Psyz_SpuPullSamples(short* out, int num_frames) {
    if (spu_tap) {
        fread(out, 4, (size_t)num_frames, spu_tap);
        return;
    }
    spu_wait_frames(num_frames);
    memset(out, 0, (size_t)num_frames * 4);
}

void Psyz_AudioPause(void) {}
void Psyz_AudioUnpause(void) {}
void Psyz_AudioLock(void) {}
void Psyz_AudioUnlock(void) {}

int Psyz_GpuRegisterCommandHandler(
    unsigned int opcode, PsyzGpuCommandHandler handler, void* userdata) {}

int Psyz_GpuSetHorizontalGrid(
    unsigned int source_width, unsigned int target_width) {}
