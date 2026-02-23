#include "libsnd_private.h"

SPU_RXX* _svm_sreg = (SPU_RXX*)0x1F801C00;
SPU_VOICE_REG _svm_sreg_buf[NUM_VOICES];
char _svm_sreg_dirty[NUM_VOICES];
unsigned short _svm_okon1;
unsigned short _svm_okon2;
unsigned short _svm_okof1;
unsigned short _svm_okof2;
unsigned short _svm_orev1;
unsigned short _svm_orev2;
struct SpuVoice _svm_voice[NUM_VOICES];
char _svm_envx_ptr[8];
char _svm_envx_hist[0x40];
int _autovol;
int _autopan;

INCLUDE_ASM("asm/nonmatchings/libsnd/vm_f", _SsVmFlush);
