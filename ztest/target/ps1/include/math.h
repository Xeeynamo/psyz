#ifndef ZTEST_PS1_MATH_H
#define ZTEST_PS1_MATH_H
// stb_image_write includes math.h for its HDR writer; nothing links it.
double frexp(double x, int* e);
#endif
