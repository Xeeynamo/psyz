#ifndef LIBPRESS_PRIVATE_H
#define LIBPRESS_PRIVATE_H
#include <libpress.h>
void MDEC_reset(int mode);
void MDEC_in(u_long* buf, int size);
void MDEC_out(u_long* buf, int size);
int MDEC_in_sync(void);
int MDEC_out_sync(void);
u_long MDEC_status(void);
#endif
