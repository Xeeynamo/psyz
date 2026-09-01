#include "libspu_private.h"

void _spu_setReverbAttr(struct rev_param_entry* attr) {
    u32 mask = attr->flags;
    int setAll = attr->flags == 0;

    if (setAll || mask & SPU_REV_ATTR_DAPF1)
        SPUW(dAPF1, attr->dAPF1);
    if (setAll || mask & SPU_REV_ATTR_DAPF2)
        SPUW(dAPF2, attr->dAPF2);
    if (setAll || mask & SPU_REV_ATTR_VIIR)
        SPUW(vIIR, attr->vIIR);
    if (setAll || mask & SPU_REV_ATTR_VCOMB1)
        SPUW(vCOMB1, attr->vCOMB1);
    if (setAll || mask & SPU_REV_ATTR_VCOMB2)
        SPUW(vCOMB2, attr->vCOMB2);
    if (setAll || mask & SPU_REV_ATTR_VCOMB3)
        SPUW(vCOMB3, attr->vCOMB3);
    if (setAll || mask & SPU_REV_ATTR_VCOMB4)
        SPUW(vCOMB4, attr->vCOMB4);
    if (setAll || mask & SPU_REV_ATTR_VWALL)
        SPUW(vWALL, attr->vWALL);
    if (setAll || mask & SPU_REV_ATTR_VAPF1)
        SPUW(vAPF1, attr->vAPF1);
    if (setAll || mask & SPU_REV_ATTR_VAPF2)
        SPUW(vAPF2, attr->vAPF2);
    if (setAll || mask & SPU_REV_ATTR_MLSAME)
        SPUW(mLSAME, attr->mLSAME);
    if (setAll || mask & SPU_REV_ATTR_MRSAME)
        SPUW(mRSAME, attr->mRSAME);
    if (setAll || mask & SPU_REV_ATTR_MLCOMB1)
        SPUW(mLCOMB1, attr->mLCOMB1);
    if (setAll || mask & SPU_REV_ATTR_MRCOMB1)
        SPUW(mRCOMB1, attr->mRCOMB1);
    if (setAll || mask & SPU_REV_ATTR_MLCOMB2)
        SPUW(mLCOMB2, attr->mLCOMB2);
    if (setAll || mask & SPU_REV_ATTR_MRCOMB2)
        SPUW(mRCOMB2, attr->mRCOMB2);
    if (setAll || mask & SPU_REV_ATTR_DLSAME)
        SPUW(dLSAME, attr->dLSAME);
    if (setAll || mask & SPU_REV_ATTR_DRSAME)
        SPUW(dRSAME, attr->dRSAME);
    if (setAll || mask & SPU_REV_ATTR_MLDIFF)
        SPUW(mLDIFF, attr->mLDIFF);
    if (setAll || mask & SPU_REV_ATTR_MRDIFF)
        SPUW(mRDIFF, attr->mRDIFF);
    if (setAll || mask & SPU_REV_ATTR_MLCOMB3)
        SPUW(mLCOMB3, attr->mLCOMB3);
    if (setAll || mask & SPU_REV_ATTR_MRCOMB3)
        SPUW(mRCOMB3, attr->mRCOMB3);
    if (setAll || mask & SPU_REV_ATTR_MLCOMB4)
        SPUW(mLCOMB4, attr->mLCOMB4);
    if (setAll || mask & SPU_REV_ATTR_MRCOMB4)
        SPUW(mRCOMB4, attr->mRCOMB4);
    if (setAll || mask & SPU_REV_ATTR_DLDIFF)
        SPUW(dLDIFF, attr->dLDIFF);
    if (setAll || mask & SPU_REV_ATTR_DRDIFF)
        SPUW(dRDIFF, attr->dRDIFF);
    if (setAll || mask & SPU_REV_ATTR_MLAPF1)
        SPUW(mLAPF1, attr->mLAPF1);
    if (setAll || mask & SPU_REV_ATTR_MRAPF1)
        SPUW(mRAPF1, attr->mRAPF1);
    if (setAll || mask & SPU_REV_ATTR_MLAPF2)
        SPUW(mLAPF2, attr->mLAPF2);
    if (setAll || mask & SPU_REV_ATTR_MRAPF2)
        SPUW(mRAPF2, attr->mRAPF2);
    if (setAll || mask & SPU_REV_ATTR_VLIN)
        SPUW(vLIN, attr->vLIN);
    if (setAll || mask & SPU_REV_ATTR_VRIN)
        SPUW(vRIN, attr->vRIN);
}
