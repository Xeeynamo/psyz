#include <common.h>
#include <libgte.h>

void MatrixNormal_0(MATRIX* m, MATRIX* n) {
    VECTOR v0, v1, v2;

    v0.vx = m->m[0][0];
    v0.vy = m->m[0][1];
    v0.vz = m->m[0][2];

    v1.vx = m->m[1][0];
    v1.vy = m->m[1][1];
    v1.vz = m->m[1][2];

    OuterProduct12(&v0, &v1, &v2);
    OuterProduct12(&v1, &v2, &v0);

    VectorNormal(&v0, &v0);
    VectorNormal(&v1, &v1);
    VectorNormal(&v2, &v2);

    n->m[0][0] = v0.vx;
    n->m[0][1] = v0.vy;
    n->m[0][2] = v0.vz;

    n->m[1][0] = v1.vx;
    n->m[1][1] = v1.vy;
    n->m[1][2] = v1.vz;

    n->m[2][0] = v2.vx;
    n->m[2][1] = v2.vy;
    n->m[2][2] = v2.vz;
}

void MatrixNormal_1(MATRIX* m, MATRIX* n) {
    VECTOR v0, v1, v2;

    v1.vx = m->m[1][0];
    v1.vy = m->m[1][1];
    v1.vz = m->m[1][2];

    v2.vx = m->m[2][0];
    v2.vy = m->m[2][1];
    v2.vz = m->m[2][2];

    OuterProduct12(&v1, &v2, &v0);
    OuterProduct12(&v0, &v1, &v2);

    VectorNormal(&v0, &v0);
    VectorNormal(&v1, &v1);
    VectorNormal(&v2, &v2);

    n->m[0][0] = v0.vx;
    n->m[0][1] = v0.vy;
    n->m[0][2] = v0.vz;

    n->m[1][0] = v1.vx;
    n->m[1][1] = v1.vy;
    n->m[1][2] = v1.vz;

    n->m[2][0] = v2.vx;
    n->m[2][1] = v2.vy;
    n->m[2][2] = v2.vz;
}

void MatrixNormal_2(MATRIX* m, MATRIX* n) {
    VECTOR v0, v1, v2;

    v2.vx = m->m[2][0];
    v2.vy = m->m[2][1];
    v2.vz = m->m[2][2];

    v0.vx = m->m[0][0];
    v0.vy = m->m[0][1];
    v0.vz = m->m[0][2];

    OuterProduct12(&v2, &v0, &v1);
    OuterProduct12(&v1, &v2, &v0);

    VectorNormal(&v0, &v0);
    VectorNormal(&v1, &v1);
    VectorNormal(&v2, &v2);

    n->m[0][0] = v0.vx;
    n->m[0][1] = v0.vy;
    n->m[0][2] = v0.vz;

    n->m[1][0] = v1.vx;
    n->m[1][1] = v1.vy;
    n->m[1][2] = v1.vz;

    n->m[2][0] = v2.vx;
    n->m[2][1] = v2.vy;
    n->m[2][2] = v2.vz;
}
