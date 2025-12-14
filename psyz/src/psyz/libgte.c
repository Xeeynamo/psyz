#include <psyz.h>
#include <libgte.h>
#include <log.h>

void InitGeom() { NOT_IMPLEMENTED; }

void SetFarColor(long rfc, long gfc, long bfc) { NOT_IMPLEMENTED; }

void SetGeomOffset(long ofx, long ofy) { NOT_IMPLEMENTED; }

void SetGeomScreen(long h) { NOT_IMPLEMENTED; }

long ratan2(long y, long x) {
    NOT_IMPLEMENTED;
    return 0;
}

long SquareRoot0(long a) {
    NOT_IMPLEMENTED;
    return 0;
}

long SquareRoot12(long a) {
    NOT_IMPLEMENTED;
    return 0;
}

MATRIX* RotMatrix(SVECTOR* r, MATRIX* m) {
    NOT_IMPLEMENTED;
    return 0;
}

MATRIX* TransMatrix(MATRIX* m, VECTOR* v) {
    NOT_IMPLEMENTED;
    return 0;
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
