#ifndef PSYZ_GTE_H
#define PSYZ_GTE_H
#include <libgte.h>

/**
 * @file gte.h
 * @brief Geometry Transformation Engine (COP2) endpoints.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read a GTE data register (COP2 data)
 *
 * The PS1 GTE (COP2) exposes 32 data registers holding input vectors,
 * accumulators, screen XY/Z FIFOs, and MAC values. Register indices follow
 * https://psx-spx.consoledev.net/geometrytransformationenginegte/
 *
 * @param reg Register index (0-31)
 * @return Register value packed as a 32-bit word
 */
unsigned int Psyz_GteDataRead(unsigned reg);

/**
 * @brief Write a GTE data register (COP2 data)
 *
 * Writes a 32-bit word into the GTE data register file. Used by an external
 * emulator to forward MTC2/LWC2 instructions into PsyZ's GTE state.
 * Register indices follow
 * https://psx-spx.consoledev.net/geometrytransformationenginegte/
 *
 * @param reg Register index (0-31)
 * @param value Value to write
 */
void Psyz_GteDataWrite(unsigned reg, unsigned int value);

/**
 * @brief Read a GTE control register (COP2 control)
 *
 * The GTE control registers hold transformation matrices, translation vectors,
 * projection parameters, and the FLAG register. Register indices follow
 * https://psx-spx.consoledev.net/geometrytransformationenginegte/
 *
 * @param reg Register index (0-31)
 * @return Register value packed as a 32-bit word
 */
unsigned int Psyz_GteCtrlRead(unsigned reg);

/**
 * @brief Write a GTE control register (COP2 control)
 *
 * Writes a 32-bit word into the GTE control register file. Used by an external
 * emulator to forward CTC2 instructions into PsyZ's GTE state.
 * Register indices follow
 * https://psx-spx.consoledev.net/geometrytransformationenginegte/
 *
 * @param reg Register index (0-31)
 * @param value Value to write
 */
void Psyz_GteCtrlWrite(unsigned reg, unsigned int value);

/**
 * @brief Execute a GTE command (COP2 instruction)
 *
 * Dispatches a 25-bit GTE command to the corresponding operation. Bits 0-5
 * select the operation (e.g. RTPS, RTPT, NCLIP, AVSZ3, AVSZ4). Used by an
 * external emulator to forward COP2 instructions into PsyZ's GTE state.
 * https://psx-spx.consoledev.net/geometrytransformationenginegte/
 *
 * On a real PlayStation it emits the COP2 instruction instead, so `cmd` must
 * be a compile-time constant there.
 *
 * @param cmd 25-bit GTE command word
 */
#ifdef __psyz
void Psyz_GteCommand(unsigned int cmd);
#else
#define Psyz_GteCommand(cmd)                                                   \
    __asm__ volatile("nop;"                                                    \
                     "nop;"                                                    \
                     ".word %0" ::"i"(0x4A000000 | (cmd)))
#endif

void Psyz_GteLdRgb(CVECTOR* v);
void Psyz_GteStRgb(CVECTOR* v);
void Psyz_GteLdRgb3(CVECTOR* v0, CVECTOR* v1, CVECTOR* v2);
void Psyz_GteLdRgb3c(CVECTOR* v);
void Psyz_GteStRgb3(CVECTOR* v0, CVECTOR* v1, CVECTOR* v2);
void Psyz_GteStRgb3G3(void* polyG3);
void Psyz_GteStRgb3Gt3(void* polyGt3);
void Psyz_GteStRgb3G4(void* polyG4);
void Psyz_GteStRgb3Gt4(void* polyGt4);
void Psyz_GteLdDp(long p);
void Psyz_GteLdSz3(long sz1, long sz2, long sz3);
void Psyz_GteLdSz4(long sz0, long sz1, long sz2, long sz3);
void Psyz_GteNccs(void);
void Psyz_GteLdClmv(void* p);
void Psyz_GteStClmv(void* p);
void Psyz_GteLdTr(long tx, long ty, long tz);
void Psyz_GteLdTx(long v);
void Psyz_GteLdTy(long v);
void Psyz_GteLdTz(long v);
void Psyz_GteAvsz3(void);
void Psyz_GteAvsz4(void);
void Psyz_GteDpcs(void);
void Psyz_GteLcir(void);
void Psyz_GteRtps(void);
void Psyz_GteRtpt(void);
void Psyz_GteNclip(void);
void Psyz_GteRt(void);
void Psyz_GteStlvnl(VECTOR* out);
void Psyz_GteRtir(void);
void Psyz_GteLdlv0(VECTOR* v);
void Psyz_GteStlvl(VECTOR* out);
void Psyz_GteLdopv1(VECTOR* v);
void Psyz_GteLdopv2(VECTOR* v);
void Psyz_GteOp12(void);
int Psyz_GteReadflg(void);
void Psyz_GteLdv0(SVECTOR* v);
void Psyz_GteLdv3(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2);
void Psyz_GteLdv01c(SVECTOR* v);
void Psyz_GteLdv3c(SVECTOR* v);
void Psyz_GteStsxy(unsigned int* out);
void Psyz_GteStsxy3(unsigned int* out0, unsigned int* out1, unsigned int* out2);
void Psyz_GteStsxy01c(unsigned int* out);
void Psyz_GteStsxy3Gt3(void* polyGt3);
void Psyz_GteStsxy3G3(void* polyG3);
void Psyz_GteStszotz(unsigned int* out);
void Psyz_GteStotz(unsigned int* out);
void Psyz_GteStopz(int* out);
void Psyz_GteRtv0(void);
void Psyz_GteRtv1(void);
void Psyz_GteRtv2(void);
void Psyz_GteStsz(unsigned int* out);
void Psyz_GteStsz3(unsigned int* out0, unsigned int* out1, unsigned int* out2);
void Psyz_GteStsz4(unsigned int* out0, unsigned int* out1, unsigned int* out2,
                   unsigned int* out3);
void Psyz_GteStsz3c(unsigned int* out);
void Psyz_GteStsz4c(unsigned int* out);
void Psyz_GteGpl0(void);
void Psyz_GteGpl12(void);
void Psyz_GteLdbv(unsigned char* v);
void Psyz_GteLdcv(unsigned char* v);
void Psyz_GteLdfc(VECTOR* v);
void Psyz_GteLdsxy0(unsigned int sxy);
void Psyz_GteLdsxy1(unsigned int sxy);
void Psyz_GteLdsxy2(unsigned int sxy);
void Psyz_GteLdsxy3(unsigned int sxy0, unsigned int sxy1, unsigned int sxy2);
void Psyz_GteLdsxy3c(unsigned int* sxy);
void Psyz_GteLdsvrtrow0(SVECTOR* v);
void Psyz_GteLdsvllrow0(SVECTOR* v);
void Psyz_GteLdsvlcrow0(SVECTOR* v);
void Psyz_GteRtv0tr(void);
void Psyz_GteRtv1tr(void);
void Psyz_GteRtv2tr(void);
void Psyz_GteRtv0bk(void);
void Psyz_GteRtv1bk(void);
void Psyz_GteRtv2bk(void);
void Psyz_GteStsv(SVECTOR* out);
void Psyz_GteStsxy0(unsigned int* out);
void Psyz_GteStsxy1(unsigned int* out);
void Psyz_GteStsxy2(unsigned int* out);
void Psyz_GteStsxy01(unsigned int* out0, unsigned int* out1);
void Psyz_GteStsxy3c(unsigned int* out);
void Psyz_GteStsxy3F3(void* polyF3);
void Psyz_GteStsxy3F4(void* polyF4);
void Psyz_GteStsxy3Ft3(void* polyFt3);
void Psyz_GteStsxy3Ft4(void* polyFt4);
void Psyz_GteStsxy3G4(void* polyG4);
void Psyz_GteStsxy3Gt4(void* polyGt4);
void Psyz_GteSttr(VECTOR* out);
void Psyz_GteRotNclip3(
    SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, unsigned int* sxy0,
    unsigned int* sxy1, unsigned int* sxy2, int* p, int* otz, int* flag,
    int* opz);
void Psyz_GteRotAverageNclip3(
    SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, unsigned int* sxy0,
    unsigned int* sxy1, unsigned int* sxy2, int* p, int* otz, int* flag,
    int* opz);
void Psyz_GteRotColorDpq(SVECTOR* v0, SVECTOR* n0, CVECTOR* rgb,
                         unsigned int* sxy, CVECTOR* out, int* flag, int* otz);
void Psyz_GteRotColorDpq3(
    SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* n0, SVECTOR* n1,
    SVECTOR* n2, CVECTOR* rgb, unsigned int* sxy0, unsigned int* sxy1,
    unsigned int* sxy2, CVECTOR* out0, CVECTOR* out1, CVECTOR* out2, int* flag,
    int* otz);
void Psyz_GteRotAverageNclipColorDpq3(
    SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* n0, SVECTOR* n1,
    SVECTOR* n2, CVECTOR* rgb, unsigned int* sxy0, unsigned int* sxy1,
    unsigned int* sxy2, CVECTOR* out0, CVECTOR* out1, CVECTOR* out2, int* otz,
    int* flag, int* opz);
void Psyz_GteRotAverageNclipColorCol3(
    SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* n0, SVECTOR* n1,
    SVECTOR* n2, CVECTOR* rgb, unsigned int* sxy0, unsigned int* sxy1,
    unsigned int* sxy2, CVECTOR* out0, CVECTOR* out1, CVECTOR* out2, int* otz,
    int* flag, int* opz);
void Psyz_GteNormalColor(SVECTOR* n0, CVECTOR* out);
void Psyz_GteNormalColor3(SVECTOR* n0, SVECTOR* n1, SVECTOR* n2, CVECTOR* out0,
                          CVECTOR* out1, CVECTOR* out2);
void Psyz_GteNormalColorDpq(SVECTOR* n0, CVECTOR* rgb, long p, CVECTOR* out);
void Psyz_GteNormalColorDpq3(
    SVECTOR* n0, SVECTOR* n1, SVECTOR* n2, CVECTOR* rgb, long p, CVECTOR* out0,
    CVECTOR* out1, CVECTOR* out2);
void Psyz_GteNormalColorCol(SVECTOR* n0, CVECTOR* rgb, CVECTOR* out);
void Psyz_GteNormalColorCol3(
    SVECTOR* n0, SVECTOR* n1, SVECTOR* n2, CVECTOR* rgb, CVECTOR* out0,
    CVECTOR* out1, CVECTOR* out2);
void Psyz_GteColorDpq(VECTOR* ir, CVECTOR* rgb, long p, CVECTOR* out);
void Psyz_GteColorCol(VECTOR* ir, CVECTOR* rgb, CVECTOR* out);
void Psyz_GteOuterProduct12(VECTOR* v0, VECTOR* v1, VECTOR* v2);
void Psyz_GteOuterProduct12SVL(SVECTOR* v0, SVECTOR* v1, VECTOR* v2);
void Psyz_GteOuterProduct0SVL(SVECTOR* v0, SVECTOR* v1, VECTOR* v2);
void Psyz_GteOuterProduct12SV(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2);
void Psyz_GteOuterProduct0SV(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2);
void Psyz_GteCompMatrix(MATRIX* m0, MATRIX* m1, MATRIX* m2);

#ifdef __cplusplus
}
#endif

#endif
