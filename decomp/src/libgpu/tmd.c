#include <common.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>

#ifdef __psyz
#include <psyz/log.h>
#define printf INFOF
#endif

#pragma pack(push, 4)
typedef struct {
    int vert;
    int nvert;
    int norm;
    int nnorm;
    int prim;
    int nprim;
    int scaling;
} TmdObj;
typedef struct {
    int id;
    int flags;
    int nobj;
    TmdObj obj[1];
} TMD;
#pragma pack(pop)

static u_long* tim;
static SVECTOR* v_ofs;
static SVECTOR* n_ofs;
static u_long* t_prim;
static int n_prim;
static int unused;

int get_tim_addr(unsigned int* addr, TIM_IMAGE*);
int get_tmd_addr(
    TMD* tmd, int obj_no, u_long** t_prim, SVECTOR** v_ofs, SVECTOR** n_ofs);

int OpenTIM(u_long* addr) {
    tim = addr;
    return 0;
}

TIM_IMAGE* ReadTIM(TIM_IMAGE* timimg) {
    u_long len = get_tim_addr((unsigned int*)tim, timimg);
    if (len == -1) {
        return NULL;
    }
    tim += len;
    return timimg;
}

int OpenTMD(u_long* tmd, int obj_no) {
    u_long len = get_tmd_addr((TMD*)tmd, obj_no, &t_prim, &v_ofs, &n_ofs);
    n_prim = len;
    return len;
}

int unpack_packet(u_long*, TMD_PRIM*);

TMD_PRIM* ReadTMD(TMD_PRIM* tmdprim) {
    int len = unpack_packet(t_prim, tmdprim);

    if (len < 0) {
        return NULL;
    }

    t_prim = (char*)t_prim + len;

    tmdprim->v_ofs = v_ofs;
    tmdprim->n_ofs = n_ofs;

    copyVector(&tmdprim->n0, &n_ofs[tmdprim->norm0]);
    copyVector(&tmdprim->n1, &n_ofs[tmdprim->norm1]);
    copyVector(&tmdprim->n2, &n_ofs[tmdprim->norm2]);
    copyVector(&tmdprim->n3, &n_ofs[tmdprim->norm3]);

    copyVector(&tmdprim->x0, &v_ofs[tmdprim->vert0]);
    copyVector(&tmdprim->x1, &v_ofs[tmdprim->vert1]);
    copyVector(&tmdprim->x2, &v_ofs[tmdprim->vert2]);
    copyVector(&tmdprim->x3, &v_ofs[tmdprim->vert3]);

    return tmdprim;
}

// https://www.psxdev.net/forum/viewtopic.php?t=109
int get_tim_addr(unsigned int* timaddr, TIM_IMAGE* img) {
    unsigned int clut_len;
    unsigned int img_len;
    if (*(int*)timaddr++ != 0x10) {
        return -1;
    }
    img->mode = *timaddr++;
    if (GetGraphDebug() == 2) {
        printf("id  =%08x\n", 0x10);
    }
    if (GetGraphDebug() == 2) {
        printf("mode=%08x\n", img->mode);
    }
    if (GetGraphDebug() == 2) {
        printf("timaddr=%08x\n", timaddr);
    }
    if (img->mode & 8) {
        clut_len = *timaddr >> 2;
        img->crect = (RECT*)(timaddr + 1);
        img->caddr = (u_long*)(timaddr + 3);
        timaddr = &timaddr[clut_len];
    } else {
        img->crect = NULL;
        img->caddr = NULL;
        clut_len = 0;
    }
    img_len = *timaddr >> 2;
    img->prect = (RECT*)(timaddr + 1);
    img->paddr = (u_long*)(timaddr + 3);
    return (int)(2 + clut_len + img_len);
}

int get_tmd_addr(
    TMD* tmd, int objid, u_long** t_prim, SVECTOR** v_ofs, SVECTOR** n_ofs) {
    TmdObj* obj = tmd->obj;
    if (GetGraphDebug() == 2) {
        printf("analizing TMD...\n");
    }
    if (GetGraphDebug() == 2) {
        printf("\tid=%08X, flags=%d, nobj=%d, objid=%d\n", tmd->id, tmd->flags,
               tmd->nobj, objid);
    }
    if (GetGraphDebug() == 2) {
        printf("\tvert=%08X, nvert=%d\n", obj[objid].vert, obj[objid].nvert);
    }
    if (GetGraphDebug() == 2) {
        printf("\tnorm=%08X, nnorm=%d\n", obj[objid].norm, obj[objid].nnorm);
    }
    if (GetGraphDebug() == 2) {
        printf("\tprim=%08X, nprim=%d\n", obj[objid].prim, obj[objid].nprim);
    }
    *v_ofs = (u_long*)((unsigned char*)obj + obj[objid].vert);
    *n_ofs = (u_long*)((unsigned char*)obj + obj[objid].norm);
    *t_prim = (u_long*)((unsigned char*)obj + obj[objid].prim);
    return obj[objid].nprim;
}

int unpack_packet(u_long* packet, TMD_PRIM* tmdprim) {
    memset(tmdprim, 0, sizeof(TMD_PRIM));
    tmdprim->id = *packet;

    switch (tmdprim->id & 0xFDFFFFFF) {
    case 0x20000304:
    case 0x20020304: {
        TMD_P_F3* p = (TMD_P_F3*)packet;
        if (GetGraphDebug() == 2) {
            printf("F3L ");
        }
        setRGB0(tmdprim, p->r0, p->g0, p->b0);
        setRGB1(tmdprim, p->r0, p->g0, p->b0);
        setRGB2(tmdprim, p->r0, p->g0, p->b0);
        tmdprim->vert0 = p->v0;
        tmdprim->vert1 = p->v1;
        tmdprim->vert2 = p->v2;
        tmdprim->norm0 = p->n0;
        tmdprim->norm1 = p->n0;
        tmdprim->norm2 = p->n0;
        return sizeof(TMD_P_F3);
    }
    case 0x30000406:
    case 0x30020406: {
        TMD_P_G3* p = (TMD_P_G3*)packet;
        if (GetGraphDebug() == 2) {
            printf("G3L ");
        }
        setRGB0(tmdprim, p->r0, p->g0, p->b0);
        setRGB1(tmdprim, p->r0, p->g0, p->b0);
        setRGB2(tmdprim, p->r0, p->g0, p->b0);
        tmdprim->vert0 = p->v0;
        tmdprim->vert1 = p->v1;
        tmdprim->vert2 = p->v2;
        tmdprim->norm0 = p->n0;
        tmdprim->norm1 = p->n1;
        tmdprim->norm2 = p->n2;
        return sizeof(TMD_P_G3);
    }
    case 0x24000507:
    case 0x24020507: {
        TMD_P_TF3* p = (TMD_P_TF3*)packet;
        if (GetGraphDebug() == 2) {
            printf("FT3L ");
        }
        tmdprim->tpage = p->tpage;
        tmdprim->clut = p->clut;
        setUV3(tmdprim, p->tu0, p->tv0, p->tu1, p->tv1, p->tu2, p->tv2);
        tmdprim->vert0 = p->v0;
        tmdprim->vert1 = p->v1;
        tmdprim->vert2 = p->v2;
        tmdprim->norm0 = p->n0;
        tmdprim->norm1 = p->n0;
        tmdprim->norm2 = p->n0;
        return sizeof(TMD_P_TF3);
    }
    case 0x34000609:
    case 0x34020609: {
        TMD_P_TG3* p = (TMD_P_TG3*)packet;
        if (GetGraphDebug() == 2) {
            printf("GT3L ");
        }
        tmdprim->tpage = p->tpage;
        tmdprim->clut = p->clut;
        setUV3(tmdprim, p->tu0, p->tv0, p->tu1, p->tv1, p->tu2, p->tv2);
        tmdprim->vert0 = p->v0;
        tmdprim->vert1 = p->v1;
        tmdprim->vert2 = p->v2;
        tmdprim->norm0 = p->n0;
        tmdprim->norm1 = p->n1;
        tmdprim->norm2 = p->n2;
        return sizeof(TMD_P_TG3);
    }
    case 0x21010304:
    case 0x21030304: {
        TMD_P_NF3* p = (TMD_P_NF3*)packet;
        if (GetGraphDebug() == 2) {
            printf("F3 ");
        }
        setRGB0(tmdprim, p->r0, p->g0, p->b0);
        setRGB1(tmdprim, p->r0, p->g0, p->b0);
        setRGB2(tmdprim, p->r0, p->g0, p->b0);
        tmdprim->vert0 = p->v0;
        tmdprim->vert1 = p->v1;
        tmdprim->vert2 = p->v2;
        return sizeof(TMD_P_NF3);
    }
    case 0x31010506:
    case 0x31030506: {
        TMD_P_NG3* p = (TMD_P_NG3*)packet;
        if (GetGraphDebug() == 2) {
            printf("G3 ");
        }
        setRGB0(tmdprim, p->r0, p->g0, p->b0);
        setRGB1(tmdprim, p->r1, p->g1, p->b1);
        setRGB2(tmdprim, p->r2, p->g2, p->b2);
        tmdprim->vert0 = p->v0;
        tmdprim->vert1 = p->v1;
        tmdprim->vert2 = p->v2;
        return sizeof(TMD_P_NG3);
    }
    case 0x25010607:
    case 0x25030607: {
        TMD_P_TNF3* p = (TMD_P_TNF3*)packet;
        if (GetGraphDebug() == 2) {
            printf("FT3 ");
        }
        tmdprim->tpage = p->tpage;
        tmdprim->clut = p->clut;
        setUV3(tmdprim, p->tu0, p->tv0, p->tu1, p->tv1, p->tu2, p->tv2);
        setRGB0(tmdprim, p->r0, p->g0, p->b0);
        setRGB1(tmdprim, p->r0, p->g0, p->b0);
        setRGB2(tmdprim, p->r0, p->g0, p->b0);
        tmdprim->vert0 = p->v0;
        tmdprim->vert1 = p->v1;
        tmdprim->vert2 = p->v2;
        return sizeof(TMD_P_TNF3);
    }
    case 0x35010809:
    case 0x35030809: {
        TMD_P_TNG3* p = (TMD_P_TNG3*)packet;
        if (GetGraphDebug() == 2) {
            printf("GT3 ");
        }
        tmdprim->tpage = p->tpage;
        tmdprim->clut = p->clut;
        setUV3(tmdprim, p->tu0, p->tv0, p->tu1, p->tv1, p->tu2, p->tv2);
        tmdprim->vert0 = p->v0;
        tmdprim->vert1 = p->v1;
        tmdprim->vert2 = p->v2;
        setRGB0(tmdprim, p->r0, p->g0, p->b0);
        setRGB1(tmdprim, p->r1, p->g1, p->b1);
        setRGB2(tmdprim, p->r2, p->g2, p->b2);
        return sizeof(TMD_P_TNG3);
    }
    case 0x28000405:
    case 0x28020405: {
        TMD_P_F4* p = (TMD_P_F4*)packet;
        if (GetGraphDebug() == 2) {
            printf("F4L ");
        }
        setRGB0(tmdprim, p->r0, p->g0, p->b0);
        setRGB1(tmdprim, p->r0, p->g0, p->b0);
        setRGB2(tmdprim, p->r0, p->g0, p->b0);
        setRGB3(tmdprim, p->r0, p->g0, p->b0);
        tmdprim->vert0 = p->v0;
        tmdprim->vert1 = p->v1;
        tmdprim->vert2 = p->v2;
        tmdprim->vert3 = p->v3;
        tmdprim->norm0 = p->n0;
        tmdprim->norm1 = p->n0;
        tmdprim->norm2 = p->n0;
        tmdprim->norm3 = p->n0;
        return sizeof(TMD_P_F4);
    }
    case 0x38000508:
    case 0x38020508: {
        TMD_P_G4* p = (TMD_P_G4*)packet;
        if (GetGraphDebug() == 2) {
            printf("G4L ");
        }
        setRGB0(tmdprim, p->r0, p->g0, p->b0);
        setRGB1(tmdprim, p->r0, p->g0, p->b0);
        setRGB2(tmdprim, p->r0, p->g0, p->b0);
        setRGB3(tmdprim, p->r0, p->g0, p->b0);
        tmdprim->vert0 = p->v0;
        tmdprim->vert1 = p->v1;
        tmdprim->vert2 = p->v2;
        tmdprim->vert3 = p->v3;
        tmdprim->norm0 = p->n0;
        tmdprim->norm1 = p->n1;
        tmdprim->norm2 = p->n2;
        tmdprim->norm3 = p->n3;
        return sizeof(TMD_P_G4);
    }
    case 0x2C000709:
    case 0x2C020709: {
        TMD_P_TF4* p = (TMD_P_TF4*)packet;
        if (GetGraphDebug() == 2) {
            printf("FT4L ");
        }
        tmdprim->tpage = p->tpage;
        tmdprim->clut = p->clut;
        setUV4(tmdprim, p->tu0, p->tv0, p->tu1, p->tv1, p->tu2, p->tv2, p->tu3,
               p->tv3);
        tmdprim->vert0 = p->v0;
        tmdprim->vert1 = p->v1;
        tmdprim->vert2 = p->v2;
        tmdprim->vert3 = p->v3;
        tmdprim->norm0 = p->n0;
        tmdprim->norm1 = p->n0;
        tmdprim->norm2 = p->n0;
        tmdprim->norm3 = p->n0;
        return sizeof(TMD_P_TF4);
    }
    case 0x3C00080C:
    case 0x3C02080C: {
        TMD_P_TG4* p = (TMD_P_TG4*)packet;
        if (GetGraphDebug() == 2) {
            printf("GT4L ");
        }
        tmdprim->tpage = p->tpage;
        tmdprim->clut = p->clut;
        setUV4(tmdprim, p->tu0, p->tv0, p->tu1, p->tv1, p->tu2, p->tv2, p->tu3,
               p->tv3);
        tmdprim->vert0 = p->v0;
        tmdprim->vert1 = p->v1;
        tmdprim->vert2 = p->v2;
        tmdprim->vert3 = p->v3;
        tmdprim->norm0 = p->n0;
        tmdprim->norm1 = p->n1;
        tmdprim->norm2 = p->n2;
        tmdprim->norm3 = p->n3;
        return sizeof(TMD_P_TG4);
    }
    case 0x29010305:
    case 0x29030305: {
        TMD_P_NF4* p = (TMD_P_NF4*)packet;
        if (GetGraphDebug() == 2) {
            printf("F4 ");
        }
        setRGB0(tmdprim, p->r0, p->g0, p->b0);
        setRGB1(tmdprim, p->r0, p->g0, p->b0);
        setRGB2(tmdprim, p->r0, p->g0, p->b0);
        setRGB3(tmdprim, p->r0, p->g0, p->b0);
        tmdprim->vert0 = p->v0;
        tmdprim->vert1 = p->v1;
        tmdprim->vert2 = p->v2;
        tmdprim->vert3 = p->v3;
        return sizeof(TMD_P_NF4);
    }
    case 0x39010608:
    case 0x39030608: {
        TMD_P_NG4* p = (TMD_P_NG4*)packet;
        if (GetGraphDebug() == 2) {
            printf("G4 ");
        }
        setRGB0(tmdprim, p->r0, p->g0, p->b0);
        setRGB1(tmdprim, p->r1, p->g1, p->b1);
        setRGB2(tmdprim, p->r2, p->g2, p->b2);
        setRGB3(tmdprim, p->r3, p->g3, p->b3);
        tmdprim->vert0 = p->v0;
        tmdprim->vert1 = p->v1;
        tmdprim->vert2 = p->v2;
        tmdprim->vert3 = p->v3;
        return sizeof(TMD_P_NG4);
    }
    case 0x2D010709:
    case 0x2D030709: {
        TMD_P_TNF4* p = (TMD_P_TNF4*)packet;
        if (GetGraphDebug() == 2) {
            printf("FT4 ");
        }
        tmdprim->tpage = p->tpage;
        tmdprim->clut = p->clut;
        setUV4(tmdprim, p->tu0, p->tv0, p->tu1, p->tv1, p->tu2, p->tv2, p->tu3,
               p->tv3);
        setRGB0(tmdprim, p->r0, p->g0, p->b0);
        setRGB1(tmdprim, p->r0, p->g0, p->b0);
        setRGB2(tmdprim, p->r0, p->g0, p->b0);
        setRGB3(tmdprim, p->r0, p->g0, p->b0);
        tmdprim->vert0 = p->v0;
        tmdprim->vert1 = p->v1;
        tmdprim->vert2 = p->v2;
        tmdprim->vert3 = p->v3;
        return sizeof(TMD_P_TNF4);
    }
    case 0x3D010A0C:
    case 0x3D030A0C: {
        TMD_P_TNG4* p = (TMD_P_TNG4*)packet;
        if (GetGraphDebug() == 2) {
            printf("GT4 ");
        }
        tmdprim->tpage = p->tpage;
        tmdprim->clut = p->clut;
        setUV4(tmdprim, p->tu0, p->tv0, p->tu1, p->tv1, p->tu2, p->tv2, p->tu3,
               p->tv3);
        tmdprim->vert0 = p->v0;
        tmdprim->vert1 = p->v1;
        tmdprim->vert2 = p->v2;
        tmdprim->vert3 = p->v3;
        setRGB0(tmdprim, p->r0, p->g0, p->b0);
        setRGB1(tmdprim, p->r1, p->g1, p->b1);
        setRGB2(tmdprim, p->r2, p->g2, p->b2);
        setRGB3(tmdprim, p->r3, p->g3, p->b3);
        return sizeof(TMD_P_TNG4);
    }
    default:
        printf("unsupported type (%08x)\n", tmdprim->id & 0xFDFFFFFF);
        return -1;
    }
}
