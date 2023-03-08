#include "fixed-point.h"
#include <stdio.h>

/* Set the integral part of Z */
void setIntK(fpreal_t z, uint32_t k)
{
    z->m[0] = k;
}

void zeroN(int N, fpreal_t z)
{
    z->sign = 0;
    memset(z->m, 0, sizeof(z->m[0])*N);
}

void copyN(int N, fpreal_t z, fpreal_t x)
{
    z->sign = x->sign;
    memcpy(z->m, x->m, sizeof(x->m[0])*N);
}

void addN(int N, fpreal_t z, fpreal_t x, uint64_t *in)
{
    // TODO: is the sign handled correctly here?
    if (in != NULL)
        for (int i = N-1; i >= 0; i--) {
            *in = z->sign * z->m[i] + x->sign * x->m[i] + *in;
            z->m[i] = *in & 0xFFFFFFFF;
            *in >>= 32;
        }
    else
        for (int i = N-1; i >= 0; i--)
            z->m[i] += x->m[i];
}

void subN(int N, fpreal_t z, fpreal_t x, uint64_t *in)
{
    struct FPReal x_neg;
    copyN(N, &x_neg, x);
    x_neg.sign *= -1;
    addN(N, z, &x_neg, in);
}

void mulK(int N, fpreal_t z, uint32_t k, uint64_t *in)
{
}

void divK(int N, fpreal_t z, uint32_t k, uint64_t *in)
{
}

void shlK(int N, fpreal_t z, uint8_t k, uint32_t in)
{
}

void shrK(int N, fpreal_t z, uint8_t k, uint32_t in)
{
}

int cmpN(int N, fpreal_t x, fpreal_t y)
{
    if (x->sign < y->sign)
        return -1;
    if (x->sign > y->sign)
        return 1;
    for (int i = N-1; i >= 0; i--) {
        if (x->m[i] < y->m[i])
            return -1;
        if (x->m[i] > y->m[i])
            return 1;
    }
    return 0;
}

bool isZeroN(int N, fpreal_t x)
{
    for (int i = 0; i < N; i++)
        if (x->m[i] != 0)
            return false;
    return true;
}

int msbN(int N, fpreal_t x)
{
    for (int i = N-1; i >= 0; i--) {
        if (x->m[i] != 0)
            return 31 - __builtin_clz(x->m[i]);
    }
    return 0;
}

