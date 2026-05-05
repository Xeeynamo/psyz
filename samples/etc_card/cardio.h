/* $PSLibId: Run-time Library Release 4.0$ */
#ifndef _CARDIO_H_
#define _CARDIO_H_

/*****************************************************************
 *
 * file: cardio.h
 *
 * 	Copyright (C) 1994,1995 by Sony Computer Entertainment Inc.
 *				          All Rights Reserved.
 *
 *	Sony Computer Entertainment Inc. Development Department
 *
 *****************************************************************/

typedef struct {
    int fileEntry;
    char fileName[15][64];
} _FINF;

typedef struct {
    char Magic[2];
    char Type;
    char BlockEntry;
    char Title[64];
    char reserve[28];
    char Clut[32];
    char Icon[3][128];
} _CARD;

#endif /* _CARDIO_H_ */
/* DON'T ADD STUFF AFTER THIS */
