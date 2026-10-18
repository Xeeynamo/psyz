#include <psyz.h>
#include "mdec.h"
#include "../internal.h"

static unsigned _dma_dpcr;
unsigned* dma_dpcr = &_dma_dpcr;

static struct {
    u_long address;
    u32 block, control;
} mdec_dma[2];
static int mdec_servicing;

u32 Psyz_DmaReadControl(void) { return _dma_dpcr; }
void Psyz_DmaWriteControl(u32 value) { _dma_dpcr = value; }

u_long Psyz_DmaRead(PsyzDmaChannel ch, unsigned offset) {
    if (ch < DMA_CHANNEL_MDEC_IN || ch > DMA_CHANNEL_MDEC_OUT || offset > 2)
        return 0;
    if (offset == 0)
        return mdec_dma[ch].address;
    return offset == 1 ? mdec_dma[ch].block : mdec_dma[ch].control;
}

static int ServiceMdec(PsyzDmaChannel ch) {
    u32 control = mdec_dma[ch].control;
    u32 expected = ch == DMA_CHANNEL_MDEC_IN ? 0x01000201 : 0x01000200;
    if ((control & 0x01000603) != expected || !(_dma_dpcr & (8u << (ch * 4))))
        return 0;
    size_t words =
        (mdec_dma[ch].block >> 16) * (size_t)(mdec_dma[ch].block & 0xFFFF);
    if (!words)
        return 0;
    void* data = (void*)mdec_dma[ch].address;
    int result = ch == DMA_CHANNEL_MDEC_IN ? Psyz_MdecDmaIn(data, words)
                                           : Psyz_MdecDmaOut(data, words);
    if (result)
        return 0;
    mdec_dma[ch].address += words * 4;
    mdec_dma[ch].block &= 0xFFFF;
    mdec_dma[ch].control &= ~0x01000000;
    Psyz_KernelDmaComplete(ch);
    return 1;
}

static void ServiceMdecPending(void) {
    if (mdec_servicing)
        return;
    mdec_servicing = 1;
    for (;;) {
        int progress = ServiceMdec(DMA_CHANNEL_MDEC_IN);
        progress |= ServiceMdec(DMA_CHANNEL_MDEC_OUT);
        if (!progress)
            break;
    }
    mdec_servicing = 0;
}

static void DmaMdec(PsyzDmaChannel ch, unsigned offset, u_long value) {
    switch (offset) {
    case 0:
        mdec_dma[ch].address = value;
        break;
    case 1:
        mdec_dma[ch].block = (u32)value;
        break;
    case 2:
        mdec_dma[ch].control = (u32)value;
        ServiceMdecPending();
        break;
    }
}

static void DmaSpu(unsigned offset, u_long value) {
    static u_long _addr = 0;
    static unsigned _len = 0;
    static u_long memAddr;
    static unsigned spuAddr;

    switch (offset) {
    case 0:
        _addr = value;
        break;
    case 1:
        _len = (unsigned)((value >> 0x10) * (value & 0xFFFF) * sizeof(int));
        break;
    case 2: // chcr
        memAddr = _addr;
        spuAddr = (unsigned)Psyz_SpuRead(0x1A6) << 3;
        if (!(value & 0x300) != 0x200) {
            WARNF("SyncMode %d is invalid for SPU", (value >> 9) & 3);
            return;
        }
        if (value & 2) { // simulate backward direction
            memAddr -= _len;
        }
        if (value & 1) {
            Psyz_SpuMemWrite(spuAddr, (void*)memAddr, _len);
        } else {
            Psyz_SpuMemRead(spuAddr, (void*)memAddr, _len);
        }
        if (value & 0x01000000) {
            // DMA transfer is always synchronous on PC
        }
        break;
    }
}

void Psyz_DmaWrite(PsyzDmaChannel ch, unsigned offset, u_long value) {
    if (offset > 2) {
        WARNF("offset %d invalid for DMA channel %d", offset, ch);
        return;
    }
    switch (ch) {
    case DMA_CHANNEL_MDEC_IN:
    case DMA_CHANNEL_MDEC_OUT:
        DmaMdec(ch, offset, value);
        break;
    case DMA_CHANNEL_GPU:
        LOG_ONCE("DMA_CHANNEL_GPU not supported");
        break;
    case DMA_CHANNEL_CD:
        LOG_ONCE("DMA_CHANNEL_CD not supported");
        break;
    case DMA_CHANNEL_SPU:
        DmaSpu(offset, value);
        break;
    case DMA_CHANNEL_PIO:
        LOG_ONCE("DMA_CHANNEL_PIO not supported");
        break;
    case DMA_CHANNEL_OTC:
        LOG_ONCE("DMA_CHANNEL_OTC not supported");
        break;
    default:
        WARNF("DMA channel %d is invalid", ch);
        break;
    }
}
