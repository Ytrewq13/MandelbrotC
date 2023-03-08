#ifndef __FIXED_POINT_H
#define __FIXED_POINT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* 4x32-bit words gives ~FP128 performance - 96-bits for the fractional part
 */
#define FP_WIDTH_WORDS 4

struct FPReal {
    int sign; // -1, 0, +1
    uint32_t m[FP_WIDTH_WORDS];
};

typedef struct FPReal * fpreal_t;

void setIntK(fpreal_t z, uint32_t k); /* Set the integral part of Z */
void zeroN(int N, fpreal_t z);
void copyN(int N, fpreal_t z, fpreal_t x);
void addN(int N, fpreal_t z, fpreal_t x, uint64_t *in);
void subN(int N, fpreal_t z, fpreal_t x, uint64_t *in);
void mulK(int N, fpreal_t z, uint32_t k, uint64_t *in);
void divK(int N, fpreal_t z, uint32_t k, uint64_t *in);
void shlK(int N, fpreal_t z, uint8_t k, uint32_t in); /* k is 0..31 */
void shrK(int N, fpreal_t z, uint8_t k, uint32_t in); /* k is 0..31 */
int cmpN(int N, fpreal_t x, fpreal_t y);
bool isZeroN(int N, fpreal_t x);
int msbN(int N, fpreal_t x);

#endif
