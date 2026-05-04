#include <psyz.h>
#include <libgpu.h>
#include <libgte.h>
#include <libetc.h>
#include <stdio.h>
#include <stdlib.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))

#define OT_LENGTH 12
#define OTSIZE (1 << OT_LENGTH)
#define SCREEN_Z 512
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

typedef struct DB {
    DRAWENV draw;
    DISPENV disp;
    OT_TYPE ot[OTSIZE];
    POLY_F4 s[6];
} DB;

#define CUBESIZE 196
static SVECTOR cube_vertices[] = {
    {-CUBESIZE / 2, -CUBESIZE / 2, -CUBESIZE / 2, 0},
    {CUBESIZE / 2, -CUBESIZE / 2, -CUBESIZE / 2, 0},
    {CUBESIZE / 2, CUBESIZE / 2, -CUBESIZE / 2, 0},
    {-CUBESIZE / 2, CUBESIZE / 2, -CUBESIZE / 2, 0},
    {-CUBESIZE / 2, -CUBESIZE / 2, CUBESIZE / 2, 0},
    {CUBESIZE / 2, -CUBESIZE / 2, CUBESIZE / 2, 0},
    {CUBESIZE / 2, CUBESIZE / 2, CUBESIZE / 2, 0},
    {-CUBESIZE / 2, CUBESIZE / 2, CUBESIZE / 2, 0}};
static int cube_indices[] = {
    0, 1, 2, 3, 1, 5, 6, 2, 5, 4, 7, 6, 4, 0, 3, 7, 4, 5, 1, 0, 6, 7, 3, 2};
static void init_cube(DB* db, CVECTOR* col) {
    size_t i;

    for (i = 0; i < ARRAY_SIZE(db->s); ++i) {
        SetPolyF4(&db->s[i]);
        setRGB0(&db->s[i], col[i].r, col[i].g, col[i].b);
    }
}
static void add_cube(OT_TYPE* ot, POLY_F4* s, MATRIX* transform) {
    int p, otz, flg;
    int nclip;
    size_t i;

    SetRotMatrix(transform);
    SetTransMatrix(transform);

    otz = 0;
    for (i = 0; i < ARRAY_SIZE(cube_indices); i += 4, ++s) {
        nclip = RotAverageNclip4(
            &cube_vertices[cube_indices[i + 0]],
            &cube_vertices[cube_indices[i + 1]],
            &cube_vertices[cube_indices[i + 2]],
            &cube_vertices[cube_indices[i + 3]], (int*)&s->x0, (int*)&s->x1,
            (int*)&s->x3, (int*)&s->x2, &p, &otz, &flg);

        if (nclip <= 0)
            continue;

        if ((otz > 0) && (otz < OTSIZE))
            AddPrim(&ot[otz], s);
    }
}

//#define USE_REVERSE_OT

static void initGraphics(DB* db, int is_pal) {
    SetDefDrawEnv(&db[0].draw, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDrawEnv(&db[1].draw, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDispEnv(&db[0].disp, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDispEnv(&db[1].disp, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    if (is_pal) {
        db[0].disp.isinter = 1;
        db[1].disp.isinter = 1;
        SetVideoMode(MODE_PAL);
    } else {
        SetVideoMode(MODE_NTSC);
    }
    ResetGraph(0);
    PutDrawEnv(&db[0].draw);
    PutDispEnv(&db[0].disp);

#ifdef USE_REVERSE_OT
    ClearOTagR(db[0].ot, OTSIZE);
    ClearOTagR(db[1].ot, OTSIZE);
#else
    ClearOTag(db[0].ot, OTSIZE);
    ClearOTag(db[1].ot, OTSIZE);
#endif

    SetDispMask(1);
    SetGraphDebug(0);
}

int main(void) {
    DB db[2];
    DB* cdb;
    SVECTOR rotation = {0};
    VECTOR translation = {0, 0, (SCREEN_Z * 3) / 2, 0};
    MATRIX transform;
    CVECTOR col[6];
    size_t i;

    StopCallback();
    ResetCallback();
    initGraphics(db, 0);

    FntLoad(960, 256);
    SetDumpFnt(FntOpen(32, 32, 320, 64, 0, 512));

    InitGeom();
    SetGeomOffset(320, 240);
    SetGeomScreen(SCREEN_Z);

    srand(0);
    for (i = 0; i < ARRAY_SIZE(col); ++i) {
        col[i].r = rand();
        col[i].g = rand();
        col[i].b = rand();
    }
    init_cube(&db[0], col);
    init_cube(&db[1], col);

    while (1) {
        cdb = (cdb == &db[0]) ? &db[1] : &db[0];

        rotation.vy += 16;
        rotation.vz += 16;

        RotMatrix(&rotation, &transform);
        TransMatrix(&transform, &translation);

#ifdef USE_REVERSE_OT
        ClearOTagR(cdb->ot, OTSIZE);
#else
        ClearOTag(cdb->ot, OTSIZE);
#endif

        FntPrint("Modified cube sample from PCSX-Redux\n\n");
        FntPrint("https://bit.ly/pcsx-redux");

        add_cube(cdb->ot, cdb->s, &transform);

        ClearImage(&cdb->draw.clip, 60, 120, 120);

#ifdef USE_REVERSE_OT
        DrawOTag(&cdb->ot[OTSIZE - 1]);
#else
        DrawOTag(&cdb->ot[0]);
#endif
        FntFlush(-1);

        DrawSync(0);
        VSync(0);
        SetDispMask(1);
    }

    return 0;
}
