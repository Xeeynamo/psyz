#include "libspu_private.h"

static inline void copyReverbEntry(char* dst, char* src, size_t size) {
    while (size--) {
        *dst++ = *src++;
    }
}

long SpuSetReverbModeParam(SpuReverbAttr* attr) {
    struct rev_param_entry entry;
    u32 mode;
    u16 cnt;
    int wasEnabled = 0;
    int setMode = 0;
    int setDelay = 0;
    int clearWorkArea = 0;
    int setFeedback = 0;
    u32 mask = attr->mask;
    int setAll = attr->mask == 0;

    entry.flags = 0;
    if (setAll || mask & SPU_REV_MODE) {
        mode = attr->mode;
        if (attr->mode & SPU_REV_MODE_CLEAR_WA) {
            mode &= ~SPU_REV_MODE_CLEAR_WA;
            clearWorkArea = 1;
        }
        if (mode >= SPU_REV_MODE_MAX ||
            _SpuIsInAllocateArea_(_spu_rev_startaddr[mode])) {
            return -1;
        }
        setMode = 1;
        _spu_rev_attr.mode = mode;
        _spu_rev_offsetaddr = _spu_rev_startaddr[_spu_rev_attr.mode];
        copyReverbEntry((char*)&entry,
                        (char*)&_spu_rev_param[_spu_rev_attr.mode *
                                               sizeof(struct rev_param_entry)],
                        sizeof(struct rev_param_entry));
        switch (_spu_rev_attr.mode) {
        case SPU_REV_MODE_ECHO:
            _spu_rev_attr.feedback = 0x7F;
            _spu_rev_attr.delay = 0x7F;
            break;
        case SPU_REV_MODE_DELAY:
            _spu_rev_attr.feedback = 0;
            _spu_rev_attr.delay = 0x7F;
            break;
        default:
            _spu_rev_attr.feedback = 0;
            _spu_rev_attr.delay = 0;
            break;
        }
    }
    if (setAll || mask & SPU_REV_DELAYTIME) {
        switch (_spu_rev_attr.mode) {
        case SPU_REV_MODE_ECHO:
        case SPU_REV_MODE_DELAY:
            setDelay = 1;
            if (!setMode) {
                copyReverbEntry(
                    (char*)&entry,
                    (char*)&_spu_rev_param[_spu_rev_attr.mode *
                                           sizeof(struct rev_param_entry)],
                    sizeof(struct rev_param_entry));
                entry.flags = SPU_REV_ATTR_MLSAME | SPU_REV_ATTR_MRSAME |
                              SPU_REV_ATTR_MLCOMB1 | SPU_REV_ATTR_DLSAME |
                              SPU_REV_ATTR_MLAPF1 | SPU_REV_ATTR_MRAPF1;
            }
            _spu_rev_attr.delay = attr->delay;
            entry.mLSAME = ((_spu_rev_attr.delay << 13) / 0x7F) - entry.dAPF1;
            entry.mRSAME = ((_spu_rev_attr.delay << 12) / 0x7F) - entry.dAPF2;
            entry.mLCOMB1 =
                ((_spu_rev_attr.delay << 12) / 0x7F) + entry.mRCOMB1;
            entry.dLSAME = ((_spu_rev_attr.delay << 12) / 0x7F) + entry.dRSAME;
            entry.mLAPF1 = ((_spu_rev_attr.delay << 12) / 0x7F) + entry.mLAPF2;
            entry.mRAPF1 = ((_spu_rev_attr.delay << 12) / 0x7F) + entry.mRAPF2;
            break;
        }
    }
    if (setAll || mask & SPU_REV_FEEDBACK) {
        switch (_spu_rev_attr.mode) {
        case SPU_REV_MODE_ECHO:
        case SPU_REV_MODE_DELAY:
            setFeedback = 1;
            if (!setMode) {
                if (!setDelay) {
                    copyReverbEntry(
                        (char*)&entry,
                        (char*)&_spu_rev_param[_spu_rev_attr.mode *
                                               sizeof(struct rev_param_entry)],
                        sizeof(struct rev_param_entry));
                    entry.flags = SPU_REV_ATTR_VWALL;
                } else {
                    entry.flags |= SPU_REV_ATTR_VWALL;
                }
            }
            _spu_rev_attr.feedback = attr->feedback;
            entry.vWALL = (_spu_rev_attr.feedback * 0x8100) / 0x7F;
            break;
        }
    }
    if (setMode) {
        wasEnabled = (SPUR(spucnt) >> 7) & 1;
        if (wasEnabled) {
            cnt = SPUR(spucnt);
            cnt &= ~0x80;
            SPUW(spucnt, cnt);
        }
    }
    if (!setMode) {
        if (setAll || mask & SPU_REV_DEPTHL) {
            SPUW(rev_vol.left, attr->depth.left);
            _spu_rev_attr.depth.left = attr->depth.left;
        }
        if (setAll || mask & SPU_REV_DEPTHR) {
            SPUW(rev_vol.right, attr->depth.right);
            _spu_rev_attr.depth.right = attr->depth.right;
        }
    } else {
        SPUW(rev_vol.left, 0);
        SPUW(rev_vol.right, 0);
        _spu_rev_attr.depth.left = 0;
        _spu_rev_attr.depth.right = 0;
    }
    if (setMode || setDelay || setFeedback) {
        _spu_setReverbAttr(&entry);
    }
    if (clearWorkArea) {
        SpuClearReverbWorkArea(_spu_rev_attr.mode);
    }
    if (setMode) {
        _spu_FsetRXX(SPU_RXX_REV_WA_START_ADDR, _spu_rev_offsetaddr, 0);
        if (wasEnabled) {
            cnt = SPUR(spucnt);
            cnt |= 0x80;
            SPUW(spucnt, cnt);
        }
    }
    return 0;
}
