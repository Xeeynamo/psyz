// SPDX-License-Identifier: MIT
#ifndef LIBSPU_INTERNAL_H
#define LIBSPU_INTERNAL_H

#include <common.h>
#include <libspu.h>

#define NUM_VOICES 24

#define SPU_VOL_MODE_MASK (1 << 15)
#define SPU_VOL_MAX 0x7F

// SPU Control Register (SPUCNT) bit masks
#define SPU_CTRL_MASK_CD_AUDIO_ENABLE (1 << 0)
#define SPU_CTRL_MASK_EXT_AUDIO_ENABLE (1 << 1)
#define SPU_CTRL_MASK_CD_AUDIO_REVERB (1 << 2)
#define SPU_CTRL_MASK_EXT_AUDIO_REVERB (1 << 3)
#define SPU_CTRL_MASK_SRAM_TRANSFER_MODE ((1 << 4) | (1 << 5))
#define SPU_CTRL_MASK_IRQ9_ENABLE (1 << 6)
#define SPU_CTRL_MASK_REVERB_MASTER_ENABLE (1 << 7)
#define SPU_CTRL_MASK_NOISE_FREQ_STEP ((1 << 8) | (1 << 9))
#define SPU_CTRL_MASK_NOISE_FREQ_SHIFT                                         \
    ((1 << 10) | (1 << 11) | (1 << 12) | (1 << 13))
#define SPU_CTRL_MASK_MUTE_SPU (1 << 14)
#define SPU_CTRL_MASK_SPU_ENABLE (1 << 15)

// SPU Register volume modes
#define SPU_VOL_MODE_DIRECT 0x0000
#define SPU_VOL_MODE_LINEARIncN 0x8000
#define SPU_VOL_MODE_LINEARIncR 0x9000
#define SPU_VOL_MODE_LINEARDecN 0xA000
#define SPU_VOL_MODE_LINEARDecR 0xB000
#define SPU_VOL_MODE_EXPIncN 0xC000
#define SPU_VOL_MODE_EXPIncR 0xD000
#define SPU_VOL_MODE_EXPDec 0xE000

#define SPU_RXX_REV_WA_START_ADDR 0xD1 /**< Reverb work area start address */

struct rev_param_entry {
    u32 flags;
    u16 dAPF1;
    u16 dAPF2;
    u16 vIIR;
    u16 vCOMB1;
    u16 vCOMB2;
    u16 vCOMB3;
    u16 vCOMB4;
    u16 vWALL;
    u16 vAPF1;
    u16 vAPF2;
    u16 mLSAME;
    u16 mRSAME;
    u16 mLCOMB1;
    u16 mRCOMB1;
    u16 mLCOMB2;
    u16 mRCOMB2;
    u16 dLSAME;
    u16 dRSAME;
    u16 mLDIFF;
    u16 mRDIFF;
    u16 mLCOMB3;
    u16 mRCOMB3;
    u16 mLCOMB4;
    u16 mRCOMB4;
    u16 dLDIFF;
    u16 dRDIFF;
    u16 mLAPF1;
    u16 mRAPF1;
    u16 mLAPF2;
    u16 mRAPF2;
    u16 vLIN;
    u16 vRIN;
};

typedef struct tagSpuMalloc {
    u32 addr;
    u32 size;
} SPU_MALLOC;

typedef struct tagSpuVoiceRegister {
    SpuVolume volume; // 0-2
    u16 pitch;        // 4
    u16 addr;         // 6
    u16 adsr[2];      // 8-A
    u16 volumex;      // C
    u16 loop_addr;    // E
} SPU_VOICE_REG;      // 16 bytes

typedef struct tagSpuControl {
    SPU_VOICE_REG voice[NUM_VOICES];
    SpuVolume main_vol; // 180
    SpuVolume rev_vol;  // 184
    // bit flags
    u16 key_on[2];       // 188
    u16 key_off[2];      // 18C
    u16 chan_fm[2];      // 190
    u16 noise_mode[2];   // 194
    u16 rev_mode[2];     // 198
    u32 chan_on;         // 19C
    u16 unk;             // 1A0
    u16 rev_work_addr;   // 1A2
    u16 irq_addr;        // 1A4
    u16 trans_addr;      // 1A6
    u16 trans_fifo;      // 1A8
    u16 spucnt;          // 1AA SPUCNT
    u16 data_trans;      // 1AC
    u16 spustat;         // 1AE SPUSTAT
    SpuVolume cd_vol;    // 1B0
    SpuVolume ex_vol;    // 1B4
    SpuVolume main_volx; // 1B8
    SpuVolume unk_vol;   // 1BC

    u16 dAPF1; // Starting at 0x1F801DC0
    u16 dAPF2;
    u16 vIIR;
    u16 vCOMB1;
    u16 vCOMB2;
    u16 vCOMB3;
    u16 vCOMB4;
    u16 vWALL;
    u16 vAPF1;
    u16 vAPF2;
    u16 mLSAME;
    u16 mRSAME;
    u16 mLCOMB1;
    u16 mRCOMB1;
    u16 mLCOMB2;
    u16 mRCOMB2;
    u16 dLSAME;
    u16 dRSAME;
    u16 mLDIFF;
    u16 mRDIFF;
    u16 mLCOMB3;
    u16 mRCOMB3;
    u16 mLCOMB4;
    u16 mRCOMB4;
    u16 dLDIFF;
    u16 dRDIFF;
    u16 mLAPF1;
    u16 mRAPF1;
    u16 mLAPF2;
    u16 mRAPF2;
    u16 vLIN;
    u16 vRIN;
} SPU_RXX;

union SpuUnion {
    volatile SPU_RXX rxx;
    volatile u16 raw[0x100];
};

extern s32 D_80033098;
extern s32 D_80033550;
extern s32 D_8003355C;
extern s32 D_80033560;
extern s8* D_80033564;
extern int _spu_AllocBlockNum;
extern int _spu_AllocLastNum;
extern s32 _spu_EVdma;
extern s32 _spu_RQmask;
extern s32 _spu_RQvoice;
extern union SpuUnion* _spu_RXX;
extern s32 _spu_env;
extern s32 _spu_inTransfer;
extern s32 _spu_isCalled;
extern s32 _spu_keystat;
extern s32 _spu_mem_mode_plus;
extern s32 _spu_mem_mode_unitM;
extern SPU_MALLOC* _spu_memList;
extern s32 _spu_rev_flag;
extern s32 _spu_rev_offsetaddr;
extern s32 _spu_rev_reserve_wa;
extern s32 _spu_rev_startaddr[];
extern SpuReverbAttr _spu_rev_attr;
extern s32 _spu_transMode;
extern s32 _spu_trans_mode;
extern u16 _spu_tsa;
extern u16 _spu_voice_centerNote[];
extern void (* volatile _spu_IRQCallback)();
extern void (* volatile _spu_transferCallback)();

void* InterruptCallback(int, void (*)());
s32 SpuSetAnyVoice(s32 on_off, u32 bits, s32 addr1, s32 addr2);
void _SpuCallback(void (*cb)());
void _SpuInit(int bHot);
void _spu_init(int bHot);
s32 _SpuIsInAllocateArea_(u32);
void _spu_FiDMA(void);
void _spu_Fr(u_char* addr, u_long size);
void _spu_FsetRXX(u32 offset, u32 value, u32 mode);
u32 _spu_FsetRXXa(s32 arg0, u32 arg1);
void _spu_gcSPU(void);
s32 _spu_t(s32, ...);
s32 _spu_write(u8*, u32);
void _SpuDataCallback(SpuTransferCallbackProc func);

#endif
