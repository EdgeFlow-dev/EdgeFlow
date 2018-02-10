/**
 * @file    avxcommon.h
 * @author  Cagri Balkesen <cagri.balkesen@inf.ethz.ch>
 * @date    Tue Dec 11 18:24:10 2012
 * @version $Id $
 * 
 * @brief   Common AVX code, kernels etc. used by implementations.
 * 
 * 
 */
#ifndef AVXCOMMON_H
#define AVXCOMMON_H

#include <immintrin.h> /* AVX intrinsics */

/* just to enable compilation with g++ */
#if defined(__cplusplus)
#undef restrict
#define restrict __restrict__
#endif

typedef struct block4  {int64_t val[4]; } block4;
typedef struct block8  {int64_t val[8]; } block8;
typedef struct block16 {int64_t val[16];} block16;

/** 
 * There are 2 ways to implement branches: 
 *     1) With conditional move instr.s using inline assembly (IFELSEWITHCMOVE).
 *     2) With software predication (IFELSEWITHPREDICATION).
 *     3) With normal if-else
 */
#define IFELSEWITHCMOVE       0
#define IFELSEWITHPREDICATION 1
#define IFELSEWITHNORMAL      0

/** Load 2 AVX 256-bit registers from the given address */
#define LOAD8(REGL, REGH, ADDR)                                         \
    do {                                                                \
        REGL = _mm256_load_pd((double const *) ADDR);                   \
        REGH = _mm256_load_pd((double const *)(((block4 *)ADDR) + 1));  \
    } while(0)

/** Load unaligned 2 AVX 256-bit registers from the given address */
#define LOAD8U(REGL, REGH, ADDR)                                        \
    do {                                                                \
        REGL = _mm256_loadu_pd((double const *) ADDR);                  \
        REGH = _mm256_loadu_pd((double const *)(((block4 *)ADDR) + 1)); \
    } while(0)

/** Store 2 AVX 256-bit registers to the given address */
#define STORE8(ADDR, REGL, REGH)                                        \
    do {                                                                \
        _mm256_store_pd((double *) ADDR, REGL);                         \
        _mm256_store_pd((double *)(((block4 *) ADDR) + 1), REGH);       \
    } while(0)

/** Store unaligned 2 AVX 256-bit registers to the given address */
#define STORE8U(ADDR, REGL, REGH)                                       \
    do {                                                                \
        _mm256_storeu_pd((double *) ADDR, REGL);                        \
        _mm256_storeu_pd((double *)(((block4 *) ADDR) + 1), REGH);      \
    } while(0)


/**
 * @note Reversing 64-bit values in an AVX register. It will be possible with
 * single _mm256_permute4x64_pd() instruction in AVX2.
 */
#define REVERSE(REG)                                    \
    do {                                                \
        /* first reverse each 128-bit lane */           \
        REG = _mm256_permute_pd(REG, 0x5);              \
        /* now shuffle 128-bit lanes */                 \
        REG = _mm256_permute2f128_pd(REG, REG, 0x1);    \
    } while(0)

static inline void _mm256_cmp_kv_pd(const __m256d keyA, const __m256d valA,
                                     const __m256d keyB, const __m256d valB,
                                     __m256d& keyh, __m256d& valh,
                                     __m256d& keyl, __m256d& vall)
{
    // sz: avx2 does not have mask_move???
    __m256d maskh = _mm256_cmp_pd(keyA, keyB, _CMP_LE_OQ);
    // __m256d maskl = _mm256_cmp_pd(keyA, keyB, _CMP_GT_OQ);

    // keyl = _mm256_or_pd(_mm256_and_pd(keyA, maskh), _mm256_and_pd(keyB, maskl));
    // keyh = _mm256_or_pd(_mm256_and_pd(keyB, maskh), _mm256_and_pd(keyA, maskl));

    // vall = _mm256_or_pd(_mm256_and_pd(valA, maskh), _mm256_and_pd(valB, maskl));
    // valh = _mm256_or_pd(_mm256_and_pd(valB, maskh), _mm256_and_pd(valA, maskl));
    keyl = _mm256_or_pd(_mm256_and_pd(keyA, maskh), _mm256_andnot_pd(maskh, keyB));
    keyh = _mm256_or_pd(_mm256_and_pd(keyB, maskh), _mm256_andnot_pd(maskh, keyA));

    vall = _mm256_or_pd(_mm256_and_pd(valA, maskh), _mm256_andnot_pd(maskh, valB));
    valh = _mm256_or_pd(_mm256_and_pd(valB, maskh), _mm256_andnot_pd(maskh, valA));

}
/** Bitonic merge kernel for 2 x 4 elements after the reversing step. */
#define BITONIC4(O1, O1v, O2, O2v, A, Av, B, Bv)                        \
    do {                                                                \
        /* Level-1 comparisons */                                       \
        __m256d l1, h1, l1v, h1v;                                       \
        _mm256_cmp_kv_pd(A, Av, B, Bv, h1, h1v, l1, l1v);               \
        /* Level-1 shuffles */                                          \
        __m256d l1p = _mm256_permute2f128_pd(l1, h1, 0x31);             \
        __m256d h1p = _mm256_permute2f128_pd(l1, h1, 0x20);             \
        __m256d l1pv = _mm256_permute2f128_pd(l1v, h1v, 0x31);          \
        __m256d h1pv = _mm256_permute2f128_pd(l1v, h1v, 0x20);          \
                                                                        \
        /* Level-2 comparisons */                                       \
        __m256d l2, h2, l2v, h2v;                                       \
        _mm256_cmp_kv_pd(l1p, l1pv, h1p, h1pv, h2, h2v, l2, l2v);       \
                                                                        \
        /* Level-2 shuffles */                                          \
        __m256d l2p = _mm256_shuffle_pd(l2, h2, 0x0);                   \
        __m256d h2p = _mm256_shuffle_pd(l2, h2, 0xF);                   \
        __m256d l2pv = _mm256_shuffle_pd(l2v, h2v, 0x0);                \
        __m256d h2pv = _mm256_shuffle_pd(l2v, h2v, 0xF);                \
                                                                        \
        /* Level-3 comparisons */                                       \
        __m256d l3, h3, l3v, h3v;                                       \
        _mm256_cmp_kv_pd(l2p, l2pv, h2p, h2pv, h3, h3v, l3, l3v);       \
                                                                        \
        /* Level-3 shuffles implemented with unpcklps unpckhps */       \
        /* AVX cannot shuffle both inputs from same 128-bit lane */     \
        /* so we need 2 more instructions for this operation. */        \
        __m256d l4 = _mm256_unpacklo_pd(l3, h3);                        \
        __m256d h4 = _mm256_unpackhi_pd(l3, h3);                        \
        __m256d l4v = _mm256_unpacklo_pd(l3v, h3v);                     \
        __m256d h4v = _mm256_unpackhi_pd(l3v, h3v);                     \
        O1 = _mm256_permute2f128_pd(l4, h4, 0x20);                      \
        O2 = _mm256_permute2f128_pd(l4, h4, 0x31);                      \
        O1v = _mm256_permute2f128_pd(l4v, h4v, 0x20);                   \
        O2v = _mm256_permute2f128_pd(l4v, h4v, 0x31);                   \
    } while(0)


/** Bitonic merge network for 2 x 8 elements without reversing B */
#define BITONIC8(O1, O1v, O2, O2v, O3, O3v, O4, O4v, A1, A1v, A2, A2v,  \
                 B1, B1v, B2, B2v)                                      \
    do {                                                                \
        /* Level-0 comparisons */                                       \
        __m256d l11, l11v, l12, l12v, h11, h11v, h12, h12v;             \
        _mm256_cmp_kv_pd(A1, A1v, B1, B1v, h11, h11v, l11, l11v);       \
        _mm256_cmp_kv_pd(A2, A2v, B2, B2v, h12, h12v, l12, l12v);       \
                                                                        \
        BITONIC4(O1, O1v, O2, O2v, l11, l11v, l12, l12v);               \
        BITONIC4(O3, O3v, O4, O4v, h11, h11v, h12, h12v);               \
    } while(0)


/** Bitonic merge kernel for 2 x 4 elements */
#define BITONIC_MERGE4(O1, O1v, O2, O2v, A, Av, B, Bv)                  \
    do {                                                                \
        /* reverse the order of input register B */                     \
        REVERSE(B);                                                     \
        REVERSE(Bv);                                                    \
        BITONIC4(O1, O1v, O2, O2v, A, Av, B, Bv);                       \
    } while(0)


/** Bitonic merge kernel for 2 x 8 elements */
#define BITONIC_MERGE8(O1, O1v, O2, O2v, O3, O3v, O4, O4v,              \
                       A1, A1v, A2, A2v, B1, B1v, B2, B2v)              \
        do {                                                            \
            /* reverse the order of input B */                          \
            REVERSE(B1);                                                \
            REVERSE(B2);                                                \
            REVERSE(B1v);                                               \
            REVERSE(B2v);                                               \
                                                                        \
            /* Level-0 comparisons */                                   \
            __m256d l11, l11v, l12, l12v, h11, h11v, h12, h12v;         \
            _mm256_cmp_kv_pd(A1, A1v, B2, B2v, h11, h11v, l11, l11v);   \
            _mm256_cmp_kv_pd(A2, A2v, B1, B1v, h12, h12v, l12, l12v);   \
                                                                        \
            BITONIC4(O1, O1v, O2, O2v, l11, l11v, l12, l12v);           \
            BITONIC4(O3, O3v, O4, O4v, h11, h11v, h12, h12v);           \
        } while(0)

/** Bitonic merge kernel for 2 x 16 elements */
#define BITONIC_MERGE16(O1, O1v, O2, O2v, O3, O3v, O4, O4v,             \
                        O5, O5v, O6, O6v, O7, O7v, O8, O8v,             \
                        A1, A1v, A2, A2v, A3, A3v, A4, A4v,             \
                        B1, B1v, B2, B2v, B3, B3v, B4, B4v)             \
        do {                                                            \
            /** Bitonic merge kernel for 2 x 16 elemenets */            \
            /* reverse the order of input B */                          \
            REVERSE(B1);                                                \
            REVERSE(B2);                                                \
            REVERSE(B3);                                                \
            REVERSE(B4);                                                \
            REVERSE(B1v);                                               \
            REVERSE(B2v);                                               \
            REVERSE(B3v);                                               \
            REVERSE(B4v);                                               \
                                                                        \
             __m256d l01, l01v, l02, l02v, l03, l03v, l04, l04v,         \
                h01, h01v, h02, h02v, h03, h03v, h04, h04v;             \
            _mm256_cmp_kv_pd(A1, A1v, B4, B4v, h01, h01v, l01, l01v);   \
            _mm256_cmp_kv_pd(A2, A2v, B3, B3v, h02, h02v, l02, l02v);   \
            _mm256_cmp_kv_pd(A3, A3v, B2, B2v, h03, h03v, l03, l03v);   \
            _mm256_cmp_kv_pd(A4, A4v, B1, B1v, h04, h04v, l04, l04v);   \
                                                                        \
            BITONIC8(O1, O1v, O2, O2v, O3, O3v, O4, O4v,                \
                     l01, l01v, l02, l02v, l03, l03v, l04, l04v);       \
            BITONIC8(O5, O5v, O6, O6v, O7, O7v, O8, O8v,                \
                     h01, h01v, h02, h02v, h03, h03v, h04, h04v);       \
        } while(0)


/** 
 * There are 2 ways to implement branches: 
 *     1) With conditional move instr.s using inline assembly (IFELSEWITHCMOVE).
 *     2) With software predication (IFELSEWITHPREDICATION).
 *     3) With normal if-else
 */
// haven't finish
#if IFELSEWITHCMOVE
#define IFELSECONDMOVE(NXT, NXTV, INA, INAV, INB, INBV, INCR)           \
    do {                                                                \
        register block4 * tmpA, * tmpB, * tmpAv, *tmpBv;                \
        register int64_t tmpKey;                                        \
                                                                        \
        __asm__ ( "mov %[A], %[tmpA]\n"         /* tmpA <-- inA      */ \
                  "mov %[Av], %[tmpAv]\n"       /* tmpAv <-- inAv    */ \
                  "add %[INC], %[A]\n"          /* inA += 4          */ \
                  "mov %[B], %[tmpB]\n"         /* tmpB <-- inB      */ \
                  "mov (%[tmpA]), %[tmpKey]\n"  /* tmpKey <-- *inA   */ \
                  "add %[INC], %[B]\n"          /* inB += 4          */ \
                  "mov %[tmpA], %[NEXT]\n"      /* next <-- A        */ \
                  "cmp (%[tmpB]), %[tmpKey]\n"  /* cmp(tmpKey,*inB ) */ \
                  "cmovnc %[tmpB], %[NEXT]\n"   /* if(A>=B) next<--B */ \
                  "cmovnc %[tmpA], %[A]\n"      /* if(A>=B) A<--oldA */ \
                  "cmovc %[tmpB], %[B]\n"       /* if(A<B)  B<--oldB */ \
                  : [A] "=r" (INA), [Av] "=r" (INAV), [B] "=r" (INB),   \
                    [Bv] "=r" (INBV), [NEXT] "=r" (NXT),                \
                    [NEXTV] "=r" (NXTV), [tmpA] "=r" (tmpA),            \
                    [tmpAv] "=r" (tmpAv), [tmpB] "=r" (tmpB),           \
                    [tmpBv] "=r" (tmpBv), [tmpKey] "=r" (tmpKey),       \
                  : "0" (INA), "1" (INB), [INC] "i" (INCR)              \
                  :                                                     \
                  );                                                    \
    } while(0)

#elif IFELSEWITHPREDICATION
#define IFELSECONDMOVE(NXT, NXTV, INA, INAV, INB, INBV, INCR)           \
            do {                                                        \
                int8_t cmp = *((int64_t *)INA) < *((int64_t *)INB);     \
                NXT  = cmp ? INA : INB;                                 \
                NXTV = cmp ? INAV : INBV;                               \
                INA += cmp;                                             \
                INAV += cmp;                                            \
                INB += !cmp;                                            \
                INBV += !cmp;                                           \
            } while(0)

#elif IFELSEWITHNORMAL
#define IFELSECONDMOVE(NXT, INA, INB, INCR)                 \
            do {                                            \
                if(*((int64_t *)INA) < *((int64_t *)INB)) { \
                    NXT = INA;                              \
                    INA ++;                                 \
                }                                           \
                else {                                      \
                    NXT = INB;                              \
                    INB ++;                                 \
                }                                           \
            } while(0)                                      \

#endif
#endif /* AVXCOMMON_H */
