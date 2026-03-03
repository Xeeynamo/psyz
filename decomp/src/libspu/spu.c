#include "libspu_private.h"

#ifndef __psyz
union SpuUnion* _spu_RXX = (union SpuUnion*)0x1F801C00;
static u32* dma_spu_madr = (u32*)0x1F8010C0;
static u32* dma_spu_bcr = (u32*)0x1F8010C4;
static u32* dma_spu_chcr = (u32*)0x1F8010C8;
static u32* dma_dpcr = (u32*)0x1F8010F0;
static u32* spu_delay = (u32*)0x1F801014;
#else
extern u32* dma_spu_madr;
extern u32* dma_spu_bcr;
extern u32* dma_spu_chcr;
extern u32* dma_dpcr;
extern u32* spu_delay;
#endif

u16 _spu_tsa = 0;
static u16 dummy = 0;
s32 _spu_transMode = 0;
s32 _spu_addrMode = 0;
s32 _spu_mem_mode = 2;
s32 _spu_mem_mode_plus = 3;
s32 _spu_mem_mode_unit = 8;
s32 _spu_mem_mode_unitM = 7;
s32 _spu_inTransfer = 1;
void (* volatile _spu_transferCallback)() = NULL;
void (* volatile _spu_IRQCallback)() = NULL;
s8 _spu_dummy[] = {7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};
s32 D_800D1058 = 0;
s32 D_800D105C = 0;
s32 D_800D1060 = 0;

static const char D_800B4D80[] = "SPU:T/O [%s]\n";
static const char D_800B4D90[] = "wait (reset)";

extern volatile u16 _spu_RQ[10];

void _spu_Fw1ts(void);
void _spu_FwriteByIO(void* addr, int len);
int _spu_init(int bHot) {
    u32 dmaTimer;
    int i;

    *dma_dpcr |= DMA_DPCR_SPU_PRIORITY_HIGH | DMA_DPCR_MASK_DMA4_ENABLE;
    _spu_RXX->rxx.main_vol.left = 0;
    _spu_RXX->rxx.main_vol.right = 0;
    _spu_RXX->rxx.spucnt = 0;
    _spu_transMode = 0;
    _spu_addrMode = 0;
    _spu_tsa = 0;
    _spu_Fw1ts();
    _spu_RXX->rxx.main_vol.left = 0;
    _spu_RXX->rxx.main_vol.right = 0;

    dmaTimer = 0;
    while (_spu_RXX->rxx.spustat & 0x7FF) {
        if (++dmaTimer > DMA_TIMEOUT) {
            printf(D_800B4D80, D_800B4D90);
            break;
        }
    }

    _spu_mem_mode = 2;
    _spu_mem_mode_plus = 3;
    _spu_mem_mode_unit = 8;
    _spu_mem_mode_unitM = 7;
    _spu_RXX->rxx.data_trans = 4;
    _spu_RXX->rxx.rev_vol.left = 0;
    _spu_RXX->rxx.rev_vol.right = 0;
    _spu_RXX->rxx.key_off[0] = 0xFFFF;
    _spu_RXX->rxx.key_off[1] = 0xFFFF;
    _spu_RXX->rxx.rev_mode[0] = 0;
    _spu_RXX->rxx.rev_mode[1] = 0;
    for (i = 0; i < 10; i++) {
        _spu_RQ[i] = 0;
    }
    if (!bHot) {
        _spu_RXX->rxx.chan_fm[0] = 0;
        _spu_RXX->rxx.chan_fm[1] = 0;
        _spu_RXX->rxx.noise_mode[0] = 0;
        _spu_RXX->rxx.noise_mode[1] = 0;
        _spu_RXX->rxx.cd_vol.left = 0;
        _spu_RXX->rxx.cd_vol.right = 0;
        _spu_RXX->rxx.ex_vol.left = 0;
        _spu_RXX->rxx.ex_vol.right = 0;
        _spu_tsa = 0x200;
        _spu_FwriteByIO(&_spu_dummy, LEN(_spu_dummy));
        for (i = 0; i < NUM_VOICES; i++) {
            _spu_RXX->rxx.voice[i].volume.left = 0;
            _spu_RXX->rxx.voice[i].volume.right = 0;
            _spu_RXX->rxx.voice[i].pitch = 0x3fff;
            _spu_RXX->rxx.voice[i].addr = 0x200;
            _spu_RXX->rxx.voice[i].adsr[0] = 0;
            _spu_RXX->rxx.voice[i].adsr[1] = 0;
        }
        _spu_RXX->rxx.key_on[0] = 0xFFFF;
        _spu_RXX->rxx.key_on[1] = 0xFF;
        _spu_Fw1ts();
        _spu_Fw1ts();
        _spu_Fw1ts();
        _spu_Fw1ts();
        _spu_RXX->rxx.key_off[0] = 0xFFFF;
        _spu_RXX->rxx.key_off[1] = 0xFF;
        _spu_Fw1ts();
        _spu_Fw1ts();
        _spu_Fw1ts();
        _spu_Fw1ts();
    }
    _spu_RXX->rxx.spucnt = SPU_CTRL_MASK_MUTE_SPU | SPU_CTRL_MASK_SPU_ENABLE;
    _spu_inTransfer = 1;
    _spu_transferCallback = NULL;
    _spu_IRQCallback = NULL;
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/libspu/spu", _spu_FwriteByIO);

INCLUDE_ASM("asm/nonmatchings/libspu/spu", _spu_FiDMA);

INCLUDE_ASM("asm/nonmatchings/libspu/spu", _spu_Fr_);

INCLUDE_ASM("asm/nonmatchings/libspu/spu", _spu_t);

INCLUDE_ASM("asm/nonmatchings/libspu/spu", _spu_Fw);

INCLUDE_ASM("asm/nonmatchings/libspu/spu", _spu_Fr);

INCLUDE_ASM("asm/nonmatchings/libspu/spu", _spu_FsetRXX);

INCLUDE_ASM("asm/nonmatchings/libspu/spu", _spu_FsetRXXa);

INCLUDE_ASM("asm/nonmatchings/libspu/spu", _spu_FgetRXXa);

INCLUDE_ASM("asm/nonmatchings/libspu/spu", _spu_FsetPCR);

INCLUDE_ASM("asm/nonmatchings/libspu/spu", _spu_FsetDelayW);

INCLUDE_ASM("asm/nonmatchings/libspu/spu", _spu_FsetDelayR);

INCLUDE_ASM("asm/nonmatchings/libspu/spu", _spu_Fw1ts);
