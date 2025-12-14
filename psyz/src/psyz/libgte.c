#include <psyz.h>
#include <libgte.h>
#include <log.h>

void InitGeom() { NOT_IMPLEMENTED; }

void SetFarColor(long rfc, long gfc, long bfc) { NOT_IMPLEMENTED; }

void SetGeomOffset(long ofx, long ofy) { NOT_IMPLEMENTED; }

void SetGeomScreen(long h) { NOT_IMPLEMENTED; }

long SquareRoot0(long a) {
    NOT_IMPLEMENTED;
    return 0;
}

long SquareRoot12(long a) {
    NOT_IMPLEMENTED;
    return 0;
}

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

void SetRotMatrix(MATRIX* m) { NOT_IMPLEMENTED; }

void SetLightMatrix(MATRIX* m) { NOT_IMPLEMENTED; }

void SetColorMatrix(MATRIX* m) { NOT_IMPLEMENTED; }

void SetTransMatrix(MATRIX* m) { NOT_IMPLEMENTED; }

void SetBackColor(long rbk, long gbk, long bbk) { NOT_IMPLEMENTED; }

void func_80017008(VECTOR* v, long* l) { NOT_IMPLEMENTED; }

long RotAverageNclip3(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, long* sxy0,
                      long* sxy1, long* sxy2, long* p, long* otz, long* flag) {
    NOT_IMPLEMENTED;
    return 0;
}

long RotAverage3(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, long* sxy0, long* sxy1,
                 long* sxy2, long* p, long* flag) {
    NOT_IMPLEMENTED;
    return 0;
}

long RotAverageNclip4(
    SVECTOR* v0, SVECTOR* v1, SVECTOR* v2,
    SVECTOR* v3, // Pointer to vectors (input)
    long* sxy0, long* sxy1, long* sxy2,
    long* sxy3, // Pointer to coordinates (output)
    long* p,    // Pointer to interpolation value (output)
    long* otz,  // Pointer to OTZ value (output)
    long* flag  // Pointer to flag (output)
) {
    NOT_IMPLEMENTED;
    return 1;
}

long RotTransPers(SVECTOR* v0, long* sxy, long* p, long* flag) {
    NOT_IMPLEMENTED;
    return 1;
}

long AverageZ3(long sz0, long sz1, long sz2) {
    // TODO we could leverage 64-bit long to avoid casting to double
    return (long)(((double)sz0 + (double)sz1 + (double)sz2) / 3);
}

long AverageZ4(long sz0, long sz1, long sz2, long sz3) {
    // TODO we could leverage 64-bit long to avoid casting to double
    return (long)(((double)sz0 + (double)sz1 + (double)sz2 + (double)sz3) / 3);
}
