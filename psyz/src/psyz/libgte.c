#include <assert.h>
#include <psyz.h>
#include <libgte.h>
#include <log.h>

// https://www.problemkaputt.de/psx-spx.htm#gteoverview
static SVECTOR V0;         // cop1 0-1
static SVECTOR V1;         // cop1 2-3
static SVECTOR V2;         // cop1 4-5
static CVECTOR RGBC;       // cop1 6
static unsigned short OTZ; // cop1 7 average Z value
static short IR0;          // cop1 8 accumulator, interpolate
static short IR1;          // cop1 9 accumulator, vector x
static short IR2;          // cop1 10 accumulator, vector y
static short IR3;          // cop1 11 accumulator, vector z
static short SX0, SY0;     // cop1 12
static short SX1, SY1;     // cop1 13
static short SX2, SY2;     // cop1 14
static short SXP, SYP;     // cop1 15
static unsigned short SZ0; // cop1 16 screen Z-coordinate FIFO
static unsigned short SZ1; // cop1 17 screen Z-coordinate FIFO
static unsigned short SZ2; // cop1 18 screen Z-coordinate FIFO
static unsigned short SZ3; // cop1 19 screen Z-coordinate FIFO
static int MAC0;           // cop1 24 math accumulator (value)
static int MAC1;           // cop1 25 math accumulator (vector)
static int MAC2;           // cop1 26 math accumulator (vector)
static int MAC3;           // cop1 27 math accumulator (vector)
static MATRIX M = {0};     // cop2 0-7, rotation 3x3 + translation
static MATRIX L1 = {0};    // cop2 8-15 light source 3x3 + bg color
static MATRIX L2 = {0};    // cop2 16-23 light source 3x3 + bg color
static int OFX;            // cop2 24 screen offset X
static int OFY;            // cop2 25 screen offset Y
static unsigned short H;   // cop2 26 projection plane distance
static short DQA;          // cop2 27 depth queing parameter A (coeff)
static short DQB;          // cop2 28 depth queing parameter B (offset)
static short ZSF3;         // cop2 29 average Z scale factor
static short ZSF4;         // cop2 30 average Z scale factor
static unsigned int FLAG;  // cop2 31

// FLAG register bits
#define FLAG_IR1_SAT (1 << 24)     // IR1 saturated
#define FLAG_IR2_SAT (1 << 23)     // IR2 saturated
#define FLAG_IR3_SAT (1 << 22)     // IR3 saturated
#define FLAG_SZ3_OTZ_SAT (1 << 18) // SZ3 or OTZ saturated
#define FLAG_DIV_OVF (1 << 17)     // Divide overflow
#define FLAG_SX2_SAT (1 << 14)     // SX2 saturated
#define FLAG_SY2_SAT (1 << 13)     // SY2 saturated
#define FLAG_IR0_SAT (1 << 12)     // IR0 saturated
#define FLAG_ERROR_MASK 0x7F87E000 // Bits 30-23 and 18-13
#define FLAG_ERROR (1u << 31)      // Error flag (OR of error bits)

// Update bit 31 based on error bits
static void FLAG_update_error() {
    if (FLAG & FLAG_ERROR_MASK)
        FLAG |= FLAG_ERROR;
}

void InitGeom() {
    ZSF3 = 0x155;
    ZSF4 = 0x100;
    H = 1000;
    DQA = -0x1062;
    DQB = 0x1400000;
    OFX = 0;
    OFY = 0;
}

void SetGeomOffset(long ofx, long ofy) {
    OFX = (short)(ofx & 0xFFFF);
    OFY = (short)(ofy & 0xFFFF);
}

void SetGeomScreen(long h) { H = h; }

void SetRotMatrix(MATRIX* m) {
    M.m[0][0] = m->m[0][0];
    M.m[0][1] = m->m[0][1];
    M.m[0][2] = m->m[0][2];
    M.m[1][0] = m->m[1][0];
    M.m[1][1] = m->m[1][1];
    M.m[1][2] = m->m[1][2];
    M.m[2][0] = m->m[2][0];
    M.m[2][1] = m->m[2][1];
    M.m[2][2] = m->m[2][2];
}

void SetTransMatrix(MATRIX* m) {
    M.t[0] = m->t[0];
    M.t[1] = m->t[1];
    M.t[2] = m->t[2];
}

void SetLightMatrix(MATRIX* m) {
    L1.m[0][0] = m->m[0][0];
    L1.m[0][1] = m->m[0][1];
    L1.m[0][2] = m->m[0][2];
    L1.m[1][0] = m->m[1][0];
    L1.m[1][1] = m->m[1][1];
    L1.m[1][2] = m->m[1][2];
    L1.m[2][0] = m->m[2][0];
    L1.m[2][1] = m->m[2][1];
    L1.m[2][2] = m->m[2][2];
}

void SetBackColor(long rbk, long gbk, long bbk) {
    L1.t[0] = rbk << 4;
    L1.t[1] = gbk << 4;
    L1.t[2] = bbk << 4;
}

void SetColorMatrix(MATRIX* m) {
    L2.m[0][0] = m->m[0][0];
    L2.m[0][1] = m->m[0][1];
    L2.m[0][2] = m->m[0][2];
    L2.m[1][0] = m->m[1][0];
    L2.m[1][1] = m->m[1][1];
    L2.m[1][2] = m->m[1][2];
    L2.m[2][0] = m->m[2][0];
    L2.m[2][1] = m->m[2][1];
    L2.m[2][2] = m->m[2][2];
}

void SetFarColor(long rfc, long gfc, long bfc) {
    L2.t[0] = rfc << 4;
    L2.t[1] = gfc << 4;
    L2.t[2] = bfc << 4;
}

long SquareRoot0_impl(long a);
long SquareRoot0(long a) { return SquareRoot0_impl(a); }

long SquareRoot12_impl(long a);
long SquareRoot12(long a) { return SquareRoot12_impl(a); }

MATRIX* RotMatrix(SVECTOR* r, MATRIX* m) {
#ifdef PLATFORM_64BIT
    // 64-bit version, accurate with PS1 implementation
    long long cx = rcos(r->vx);
    long long sx = rsin(r->vx);
    long long cy = rcos(r->vy);
    long long sy = rsin(r->vy);
    long long cz = rcos(r->vz);
    long long sz = rsin(r->vz);

    m->m[0][0] = (short)((cy * cz) >> 12);
    m->m[0][1] = (short)((-cy * sz) >> 12);
    m->m[0][2] = (short)(sy);

    m->m[1][0] = (short)((sx * sy * cz + (cx * sz << 12)) >> 24);
    m->m[1][1] = (short)((-sx * sy * sz + (cx * cz << 12)) >> 24);
    m->m[1][2] = (short)((-sx * cy) >> 12);

    m->m[2][0] = (short)((-cx * sy * cz + (sx * sz << 12)) >> 24);
    m->m[2][1] = (short)((cx * sy * sz + (sx * cz << 12)) >> 24);
    m->m[2][2] = (short)((cx * cy) >> 12);
#else
    // 32-bit version, less accurate but much faster on non-64bit CPUs
    int cx = rcos(r->vx);
    int sx = rsin(r->vx);
    int cy = rcos(r->vy);
    int sy = rsin(r->vy);
    int cz = rcos(r->vz);
    int sz = rsin(r->vz);

    m->m[0][0] = (short)((cy * cz) >> 12);
    m->m[0][1] = (short)((-cy * sz) >> 12);
    m->m[0][2] = (short)(sy);

    m->m[1][0] = (short)((((sx * sy) >> 12) * cz + (cx * sz)) >> 12);
    m->m[1][1] = (short)((((-sx * sy) >> 12) * sz + (cx * cz)) >> 12);
    m->m[1][2] = (short)((-sx * cy) >> 12);

    m->m[2][0] = (short)((((-cx * sy) >> 12) * cz + (sx * sz)) >> 12);
    m->m[2][1] = (short)((((cx * sy) >> 12) * sz + (sx * cz)) >> 12);
    m->m[2][2] = (short)((cx * cy) >> 12);
#endif

    m->t[0] = 0;
    m->t[1] = 0;
    m->t[2] = 0;

    return m;
}

MATRIX* TransMatrix(MATRIX* m, VECTOR* v) {
    m->t[0] = v->vx;
    m->t[1] = v->vy;
    m->t[2] = v->vz;
    return m;
}

// RTPS, RTPT, NCLIP, AVSZ3, AVSZ4 are implementations after
// https://problemkaputt.de/psxspx-gte-coordinate-calculation-commands.htm

// Perspective Transformation helper (assumes sf=1)
// depth_cue: if non-zero, compute IR0 from DQA/DQB
static void RTPS_vertex(SVECTOR* v, int depth_cue) {
    // IR1 = MAC1 = (TRX*1000h + RT11*VX + RT12*VY + RT13*VZ) SAR 12
    MAC1 = ((M.t[0] << 12) + M.m[0][0] * v->vx + M.m[0][1] * v->vy +
            M.m[0][2] * v->vz) >>
           12;
    MAC2 = ((M.t[1] << 12) + M.m[1][0] * v->vx + M.m[1][1] * v->vy +
            M.m[1][2] * v->vz) >>
           12;
    MAC3 = ((M.t[2] << 12) + M.m[2][0] * v->vx + M.m[2][1] * v->vy +
            M.m[2][2] * v->vz) >>
           12;

    // IR1,IR2,IR3 saturated to -8000h..+7FFFh
    if (MAC1 < -0x8000 || MAC1 > 0x7FFF)
        FLAG |= FLAG_IR1_SAT;
    if (MAC2 < -0x8000 || MAC2 > 0x7FFF)
        FLAG |= FLAG_IR2_SAT;
    if (MAC3 < -0x8000 || MAC3 > 0x7FFF)
        FLAG |= FLAG_IR3_SAT;
    IR1 = (short)(CLAMP(MAC1, -0x8000, 0x7FFF));
    IR2 = (short)(CLAMP(MAC2, -0x8000, 0x7FFF));
    IR3 = (short)(CLAMP(MAC3, -0x8000, 0x7FFF));

    // Push SZ FIFO before writing SZ3
    SZ0 = SZ1;
    SZ1 = SZ2;
    SZ2 = SZ3;

    // SZ3 = MAC3 (with sf=1, no additional shift), saturate to 0..FFFFh
    if (MAC3 < 0 || MAC3 > 0xFFFF)
        FLAG |= FLAG_SZ3_OTZ_SAT;
    SZ3 = (unsigned short)(CLAMP(MAC3, 0, 0xFFFF));

    // Division: (H*20000h/SZ3+1)/2, saturate to 1FFFFh
    int div_result;
    if (SZ3 == 0) {
        div_result = 0x1FFFF;
        FLAG |= FLAG_DIV_OVF;
    } else {
        div_result = (((H * 0x20000) / SZ3) + 1) / 2;
        if (div_result > 0x1FFFF) {
            div_result = 0x1FFFF;
            FLAG |= FLAG_DIV_OVF;
        }
    }

    // Push SXY FIFO before writing SX2,SY2
    SX0 = SX1;
    SY0 = SY1;
    SX1 = SX2;
    SY1 = SY2;

    // MAC0=(div_result)*IR1+OFX, SX2=MAC0/10000h, saturate to -400h..+3FFh
    MAC0 = div_result * IR1 + (OFX << 16);
    int sx = MAC0 >> 16;
    if (sx < -0x400 || sx > 0x3FF)
        FLAG |= FLAG_SX2_SAT;
    SX2 = (short)(CLAMP(sx, -0x400, 0x3FF));

    // MAC0=(div_result)*IR2+OFY, SY2=MAC0/10000h, saturate to -400h..+3FFh
    MAC0 = div_result * IR2 + (OFY << 16);
    int sy = MAC0 >> 16;
    if (sy < -0x400 || sy > 0x3FF)
        FLAG |= FLAG_SY2_SAT;
    SY2 = (short)(CLAMP(sy, -0x400, 0x3FF));

    // Depth cueing only for the last vertex
    if (depth_cue) {
        // MAC0=(div_result)*DQA+DQB, IR0=MAC0/1000h, saturate to 0..+1000h
        MAC0 = div_result * DQA + DQB;
        int ir0 = MAC0 >> 12;
        if (ir0 < 0 || ir0 > 0x1000)
            FLAG |= FLAG_IR0_SAT;
        IR0 = (short)(CLAMP(ir0, 0, 0x1000));
    }
}

// Perspective Transformation (single)
static void RTPS() {
    FLAG = 0;
    RTPS_vertex(&V0, 1);
    FLAG_update_error();
}

// Perspective Transformation (triple)
static void RTPT() {
    FLAG = 0;
    RTPS_vertex(&V0, 0);
    RTPS_vertex(&V1, 0);
    RTPS_vertex(&V2, 1);
    FLAG_update_error();
}

// Normal clipping
static void NCLIP() {
    FLAG = 0;
    MAC0 = SX0 * (SY1 - SY2) + SX1 * (SY2 - SY0) + SX2 * (SY0 - SY1);
}

// Average of three Z values
static void AVSZ3() {
    FLAG = 0;
    MAC0 = ZSF3 * (SZ1 + SZ2 + SZ3);
    int otz = MAC0 >> 12;
    if (otz < 0 || otz > 0xFFFF)
        FLAG |= FLAG_SZ3_OTZ_SAT;
    OTZ = (unsigned short)(CLAMP(otz, 0, 0xFFFF));
    FLAG_update_error();
}

// Average of four Z values
static void AVSZ4() {
    FLAG = 0;
    MAC0 = ZSF4 * (SZ0 + SZ1 + SZ2 + SZ3);
    int otz = MAC0 >> 12;
    if (otz < 0 || otz > 0xFFFF)
        FLAG |= FLAG_SZ3_OTZ_SAT;
    OTZ = (unsigned short)(CLAMP(otz, 0, 0xFFFF));
    FLAG_update_error();
}

long AverageZ3(long sz0, long sz1, long sz2) {
    SZ1 = sz0;
    SZ2 = sz1;
    SZ3 = sz2;
    AVSZ3();
    return MAC0 >> 12;
}

long AverageZ4(long sz0, long sz1, long sz2, long sz3) {
    SZ0 = sz0;
    SZ1 = sz1;
    SZ2 = sz2;
    SZ3 = sz3;
    AVSZ4();
    return MAC0 >> 12;
}

long NormalClip(long sxy0, long sxy1, long sxy2) {
    // TODO can this be simplified with an union?
    SX0 = (short)sxy0;
    SY0 = (short)(sxy0 >> 16);
    SX1 = (short)sxy1;
    SY1 = (short)(sxy1 >> 16);
    SX2 = (short)sxy2;
    SY2 = (short)(sxy2 >> 16);
    NCLIP();
    return MAC0;
}

long RotTransPers(SVECTOR* v0, long* sxy, long* p, long* flag) {
    V0 = *v0;
    RTPS();
    *(unsigned int*)sxy = SX2 | (SY2 << 16);
    *p = IR0;
    *flag = (long)FLAG;
    return SZ3 >> 2;
}

long RotTransPers3(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, long* sxy0,
                   long* sxy1, long* sxy2, long* p, long* flag) {
    V0 = *v0;
    V1 = *v1;
    V2 = *v2;
    RTPT();
    *(unsigned int*)sxy0 = SX0 | (SY0 << 16);
    *(unsigned int*)sxy1 = SX1 | (SY1 << 16);
    *(unsigned int*)sxy2 = SX2 | (SY2 << 16);
    *p = IR0;
    *flag = (long)FLAG;
    return SZ3 >> 2;
}

long RotTransPers4(
    SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, long* sxy0, long* sxy1,
    long* sxy2, long* sxy3, long* p, long* flag) {
    long flag1, flag2;
    RotTransPers3(v0, v1, v2, sxy0, sxy1, sxy2, p, &flag1);
    RotTransPers(v3, sxy3, p, &flag2);
    *flag = flag1 | flag2;
    return SZ3 >> 2;
}

long RotAverage3(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, long* sxy0, long* sxy1,
                 long* sxy2, long* p, long* flag) {
    V0 = *v0;
    V1 = *v1;
    V2 = *v2;
    RTPT();
    *(unsigned int*)sxy0 = SX0 | (SY0 << 16);
    *(unsigned int*)sxy1 = SX1 | (SY1 << 16);
    *(unsigned int*)sxy2 = SX2 | (SY2 << 16);
    *flag = (long)FLAG;
    *p = IR0;
    AVSZ3();
    return OTZ;
}

long RotAverage4(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, long* sxy0,
                 long* sxy1, long* sxy2, long* sxy3, long* p, long* flag) {
    V0 = *v0;
    V1 = *v1;
    V2 = *v2;
    RTPT();
    *(unsigned int*)sxy0 = SX0 | (SY0 << 16);
    *(unsigned int*)sxy1 = SX1 | (SY1 << 16);
    *(unsigned int*)sxy2 = SX2 | (SY2 << 16);
    long flag1 = (long)FLAG;
    V0 = *v3;
    RTPS();
    *(unsigned int*)sxy3 = SX2 | (SY2 << 16);
    *p = IR0;
    *flag = flag1 | (long)FLAG;
    AVSZ4();
    return OTZ;
}

long RotAverageNclip3(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, long* sxy0,
                      long* sxy1, long* sxy2, long* p, long* otz, long* flag) {
    V0 = *v0;
    V1 = *v1;
    V2 = *v2;
    RTPT();
    *flag = (long)FLAG;
    NCLIP();
    if (MAC0 > 0) {
        *(unsigned int*)sxy0 = SX0 | (SY0 << 16);
        *(unsigned int*)sxy1 = SX1 | (SY1 << 16);
        *(unsigned int*)sxy2 = SX2 | (SY2 << 16);
        *p = IR0;
        AVSZ3();
        *otz = OTZ;
    }
    return MAC0;
}

long RotAverageNclip4(
    SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, long* sxy0, long* sxy1,
    long* sxy2, long* sxy3, long* p, long* otz, long* flag) {
    V0 = *v0;
    V1 = *v1;
    V2 = *v2;
    RTPT();
    long flag1 = (long)FLAG;
    *flag = flag1;
    NCLIP();
    if (MAC0 > 0) {
        *(unsigned int*)sxy0 = SX0 | (SY0 << 16);
        *(unsigned int*)sxy1 = SX1 | (SY1 << 16);
        *(unsigned int*)sxy2 = SX2 | (SY2 << 16);
        V0 = *v3;
        RTPS();
        *(unsigned int*)sxy3 = SX2 | (SY2 << 16);
        *p = IR0;
        *flag = flag1 | (long)FLAG;
        AVSZ4();
        *otz = OTZ;
    }
    return MAC0;
}
