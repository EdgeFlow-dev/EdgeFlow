/**
 * @file    avxsort_core.h
 * @author  Cagri Balkesen <cagri.balkesen@inf.ethz.ch>
 * @version $Id $
 *
 * @brief   AVX sorting core kernels, etc.
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>             /* qsort() */
#include <math.h>               /* log2()  */
#ifdef __cplusplus
#include <algorithm>            /* sort()  */
#endif

#include "avxcommon.h"
#include "utils.h"
#include "params.h"
/* #include "iacaMarks.h" */


/*******************************************************************************
 *                                                                             *
 *                               Declarations                                  *
 *                                                                             *
 *******************************************************************************/

/** L2 Cache size of the system in Bytes, used for determining block size */
#ifndef L2_CACHE_SIZE
#define L2_CACHE_SIZE (256*1024)
#endif

/** Number of tuples that can fit into L2 cache divided by 2 */
#ifndef BLOCKSIZE
#define BLOCKSIZE (L2_CACHE_SIZE / (2 * sizeof(int64_t)))
#endif


/** Logarithm base 2 of BLOCK_SIZE */
#ifndef LOG2_BLOCKSIZE
#define LOG2_BLOCKSIZE (log2(BLOCKSIZE))
#endif

/**
 *  Fixed size single-block AVX sorting routine
 */
inline void
avxsort_block(int64_t ** inputptr, int64_t ** outputptr, const int BLOCK_SIZE);

/**
 * In-register sorting of 4x4=16 <32-bit key,32-bit val> pairs pointed
 * by items-ptr. Uses 256-bit AVX registers.
 *
 * @param items 16x64-bit values
 * @param output 4-sorted 16x64-bit output values
 */
inline void
inregister_sort_keyval32(int64_t * items, int64_t * output);

/**
 * Merges two sorted lists of length len into a new sorted list of length
 * outlen. Uses 4-wide bitonic-merge network.
 *
 * @warning Assumes that inputs will have items multiple of 4.
 * @param inpA sorted list A
 * @param inpB sorted list B
 * @param out merged output list
 * @param len length of input
 */
inline void
merge4_eqlen(int64_t * const inpA, int64_t * const inpB,
             int64_t * const out, const uint32_t len);

/**
 * Merges two sorted lists of length len into a new sorted list of length
 * outlen. Uses 16-wide bitonic-merge network.
 *
 * @warning Assumes that inputs will have items multiple of 16.
 * @param inpA sorted list A
 * @param inpB sorted list B
 * @param out merged output list
 * @param len length of input
 */
inline void
merge16_eqlen(int64_t * const inpA, int64_t * const inpAv,
              int64_t * const inpB, int64_t * const inpBv,
              int64_t * const out, int64_t * const outv,
              const uint32_t len);


/*******************************************************************************
 *                                                                             *
 *                               Implementations                               *
 *                                                                             *
 *******************************************************************************/

/**************** Helper Methods************************************************/
inline __attribute__((__always_inline__)) uint32_t
mylog2(const uint32_t n)
{
    register uint32_t res;
    __asm__ ( "\tbsr %1, %0\n" : "=r"(res) : "r"(n) );
    return res;
}
/*******************************************************************************/
// sz: done. There are so many redundant funcs, why don't use template???
// Double: 1 bit sign bit + 11 bits exponent + 52 bits fraction, so it's okay
// to use it to compare NON-NEGATIVE int64_t
inline void __attribute((always_inline))
    merge4_eqlen(int64_t * const inpA, int64_t * const inpAv,
                 int64_t * const inpB, int64_t * const inpBv,
                 int64_t * const out, int64_t * const outv, const uint32_t len)
{
    register block4 * inA  = (block4 *) inpA;
    register block4 * inB  = (block4 *) inpB;
    register block4 * inAv  = (block4 *) inpAv;
    register block4 * inBv  = (block4 *) inpBv;
    block4 * const    endA = (block4 *) (inpA + len);
    block4 * const    endB = (block4 *) (inpB + len);

    block4 * outp = (block4 *) out;
    block4 * outpv = (block4 *) outv;

    register block4 * next = inB;
    register block4 * nextv = inBv;

    register __m256d outreg1, outreg1v;
    register __m256d outreg2, outreg2v;

    register __m256d regA = _mm256_loadu_pd((double const *) inA);
    register __m256d regB = _mm256_loadu_pd((double const *) next);

    register __m256d regAv = _mm256_loadu_pd((double const *) inAv);
    register __m256d regBv = _mm256_loadu_pd((double const *) nextv);

    inA ++;
    inB ++;
    inAv ++;
    inBv ++;

    BITONIC_MERGE4(outreg1, outreg1v, outreg2, outreg2v,
                   regA, regAv, regB, regBv);

    /* store outreg1 */
    _mm256_storeu_pd((double *) outp, outreg1);
    _mm256_storeu_pd((double *) outpv, outreg1v);
    outp ++;
    outpv ++;

    while( inA < endA && inB < endB ) {

        /* 3 Options : normal-if, cmove-with-assembly, sw-predication */
        IFELSECONDMOVE(next, nextv, inA, inAv, inB, inBv, 32);

        regA = outreg2;
        regAv = outreg2v;
        regB = _mm256_loadu_pd((double const *) next);
        regBv = _mm256_loadu_pd((double const *) nextv);

        BITONIC_MERGE4(outreg1, outreg1v, outreg2, outreg2v,
                       regA, regAv, regB, regBv);

        /* store outreg1 */
        _mm256_storeu_pd((double *) outp, outreg1);
        _mm256_storeu_pd((double *) outpv, outreg1v);

        outp ++;
        outpv ++;
    }

    /* handle remaining items */
    while( inA < endA ) {
        __m256d regA = _mm256_loadu_pd((double const *) inA);
        __m256d regAv = _mm256_loadu_pd((double const *) inAv);

        __m256d regB = outreg2;
        __m256d regBv = outreg2v;

        BITONIC_MERGE4(outreg1, outreg1v, outreg2, outreg2v,
                       regA, regAv, regB, regBv);

        _mm256_storeu_pd((double *) outp, outreg1);
        _mm256_storeu_pd((double *) outpv, outreg1v);

        inA ++;
        inAv ++;
        outp ++;
        outpv ++;
    }

    while( inB < endB ) {
        __m256d regA = outreg2;
        __m256d regAv = outreg2v;
        __m256d regB = _mm256_loadu_pd((double const *) inB);
        __m256d regBv = _mm256_loadu_pd((double const *) inBv);

        BITONIC_MERGE4(outreg1, outreg1v, outreg2, outreg2v,
                       regA, regAv, regB, regBv);

        _mm256_storeu_pd((double *) outp, outreg1);
        _mm256_storeu_pd((double *) outpv, outreg1v);

        inB ++;
        inBv ++;
        outp ++;
        outpv ++;
    }

    /* store the last remaining register values */
    _mm256_storeu_pd((double *) outp, outreg2);
    _mm256_storeu_pd((double *) outpv, outreg2v);

}

/**
 * Merges two sorted lists of length len into a new sorted list of length
 * outlen. Uses 8-wide bitonic-merge network.
 *
 * @warning Assumes that inputs will have items multiple of 8.
 * @param inpA sorted list A
 * @param inpB sorted list B
 * @param out merged output list
 * @param len length of input
 */
// sz: done
inline void __attribute((always_inline))
    merge8_eqlen(int64_t * const inpA, int64_t * const inpAv,
                 int64_t * const inpB, int64_t * const inpBv,
                 int64_t * const out, int64_t * const outv, const uint32_t len)
{
    register block8 * inA  = (block8 *) inpA;
    register block8 * inB  = (block8 *) inpB;
    register block8 * inAv  = (block8 *) inpAv;
    register block8 * inBv  = (block8 *) inpBv;
    block8 * const    endA = (block8 *) (inpA + len);
    block8 * const    endB = (block8 *) (inpB + len);

    block8 * outp = (block8 *) out;
    block8 * outpv = (block8 *) outv;

    register block8 * next = inB;
    register block8 * nextv = inBv;

    register __m256d outreg1l, outreg1h;
    register __m256d outreg2l, outreg2h;
    register __m256d outreg1vl, outreg1vh;
    register __m256d outreg2vl, outreg2vh;

    register __m256d regAl, regAh;
    register __m256d regBl, regBh;
    register __m256d regAvl, regAvh;
    register __m256d regBvl, regBvh;

    LOAD8U(regAl, regAh, inA);
    LOAD8U(regBl, regBh, next);
    LOAD8U(regAvl, regAvh, inAv);
    LOAD8U(regBvl, regBvh, nextv);

    inA ++;
    inAv ++;
    inB ++;
    inBv ++;

    BITONIC_MERGE8(outreg1l, outreg1vl, outreg1h, outreg1vh,
                   outreg2l, outreg2vl, outreg2h, outreg2vh,
                   regAl, regAvl, regAh, regAvh, regBl, regBvl, regBh, regBvh);

    /* store outreg1 */
    STORE8U(outp, outreg1l, outreg1h);
    STORE8U(outpv, outreg1vl, outreg1vh);

    outp ++;
    outpv ++;

    while( inA < endA && inB < endB ) {

        /* 3 Options : normal-if, cmove-with-assembly, sw-predication */
        IFELSECONDMOVE(next, nextv, inA, inAv, inB, inBv, 64);

        regAl = outreg2l;
        regAh = outreg2h;
        regAvl = outreg2vl;
        regAvh = outreg2vh;

        LOAD8U(regBl, regBh, next);
        LOAD8U(regBvl, regBvh, nextv);

        BITONIC_MERGE8(outreg1l, outreg1vl, outreg1h, outreg1vh,
                       outreg2l, outreg2vl, outreg2h, outreg2vh,
                       regAl, regAvl, regAh, regAvh, regBl, regBvl, regBh, regBvh);

        /* store outreg1 */
        STORE8U(outp, outreg1l, outreg1h);
        STORE8U(outpv, outreg1vl, outreg1vh);
        outp ++;
        outpv ++;
    };

    /* handle remaining items */
    while( inA < endA ) {
        __m256d regAl, regAh;
        __m256d regAvl, regAvh;

        LOAD8U(regAl, regAh, inA);
        LOAD8U(regAvl, regAvh, inAv);

        __m256d regBl = outreg2l;
        __m256d regBh = outreg2h;
        __m256d regBvl = outreg2vl;
        __m256d regBvh = outreg2vh;

        BITONIC_MERGE8(outreg1l, outreg1vl, outreg1h, outreg1vh,
                       outreg2l, outreg2vl, outreg2h, outreg2vh,
                       regAl, regAvl, regAh, regAvh, regBl, regBvl, regBh, regBvh);

        /* store outreg1 */
        STORE8U(outp, outreg1l, outreg1h);
        STORE8U(outpv, outreg1vl, outreg1vh);

        outp ++;
        inA ++;
        outpv ++;
        inAv ++;
    }

    while( inB < endB ) {
        __m256d regAl = outreg2l;
        __m256d regAvl = outreg2vl;
        __m256d regAh = outreg2h;
        __m256d regAvh = outreg2vh;
        __m256d regBl, regBh;
        __m256d regBvl, regBvh;

        LOAD8U(regBl, regBh, inB);
        LOAD8U(regBvl, regBvh, inBv);

        BITONIC_MERGE8(outreg1l, outreg1vl, outreg1h, outreg1vh,
                       outreg2l, outreg2vl, outreg2h, outreg2vh,
                       regAl, regAvl, regAh, regAvh, regBl, regBvl, regBh, regBvh);

        /* store outreg1 */
        STORE8U(outp, outreg1l, outreg1h);
        STORE8U(outpv, outreg1vl, outreg1vh);
        outp ++;
        inB ++;
        outpv ++;
        inBv ++;
    }

    /* store the last remaining register values */
    STORE8U(outp, outreg2l, outreg2h);
    STORE8U(outpv, outreg2vl, outreg2vh);
}

inline void __attribute((always_inline))
    inregister_sort_keyval32(int64_t * items, int64_t * itemsv,
                             int64_t * output, int64_t * outputv)
{

    __m256d ra = _mm256_loadu_pd ((double const *)(items));
    __m256d rb = _mm256_loadu_pd ((double const *)(items + 4));
    __m256d rc = _mm256_loadu_pd ((double const *)(items + 8));
    __m256d rd = _mm256_loadu_pd ((double const *)(items + 12));

    __m256d rav = _mm256_loadu_pd ((double const *)(itemsv));
    __m256d rbv = _mm256_loadu_pd ((double const *)(itemsv + 4));
    __m256d rcv = _mm256_loadu_pd ((double const *)(itemsv + 8));
    __m256d rdv = _mm256_loadu_pd ((double const *)(itemsv + 12));

    /* odd-even sorting network begins */
    /* 1st level of comparisons */
    // __m256d ra1 = _mm256_min_pd(ra, rb);
    // __m256d rb1 = _mm256_max_pd(ra, rb);
    __m256d ra1, ra1v, rb1, rb1v, rc1, rc1v, rd1, rd1v;
    _mm256_cmp_kv_pd(ra, rav, rb, rbv, rb1, rb1v, ra1, ra1v);
    // __m256d rc1 = _mm256_min_pd(rc, rd);
    // __m256d rd1 = _mm256_max_pd(rc, rd);
    _mm256_cmp_kv_pd(rc, rcv, rd, rdv, rd1, rd1v, rc1, rc1v);

    /* 2nd level of comparisons */
    // rb = _mm256_min_pd(rb1, rd1);
    // rd = _mm256_max_pd(rb1, rd1);
    _mm256_cmp_kv_pd(rb1, rb1v, rd1, rd1v, rd, rdv, rb, rbv);

    /* 3rd level of comparisons */
    // __m256d ra2 = _mm256_min_pd(ra1, rc1);
    // __m256d rc2 = _mm256_max_pd(ra1, rc1);
    __m256d ra2, ra2v, rc2, rc2v;
    _mm256_cmp_kv_pd(ra1, ra1v, rc1, rc1v, rc2, rc2v, ra2, ra2v);

    /* 4th level of comparisons */
    // __m256d rb3 = _mm256_min_pd(rb, rc2);
    // __m256d rc3 = _mm256_max_pd(rb, rc2);
    __m256d rb3, rb3v, rc3, rc3v;
    _mm256_cmp_kv_pd(rb, rbv, rc2, rc2v, rc3, rc3v, rb3, rb3v);

    /* results are in ra2, rb3, rc3, rd */
    /**
     * Initial data and transposed data looks like following:
     *  a2={ x1  x2  x3  x4  }                      a4={ x1 x5 x9  x13 }
     *  b3={ x5  x6  x7  x8  }  === Transpose ===>  b5={ x2 x6 x10 x14 }
     *  c3={ x9  x10 x11 x12 }                      c5={ x3 x7 x11 x15 }
     *  d2={ x13 x14 x15 x16 }                      d4={ x4 x8 x12 x16 }
     */
    /* shuffle x2 and x5 - shuffle x4 and x7 */
    __m256d ra3 = _mm256_unpacklo_pd(ra2, rb3);
    __m256d rb4 = _mm256_unpackhi_pd(ra2, rb3);
    __m256d ra3v = _mm256_unpacklo_pd(ra2v, rb3v);
    __m256d rb4v = _mm256_unpackhi_pd(ra2v, rb3v);

    /* shuffle x10 and x13 - shuffle x12 and x15 */
    __m256d rc4 = _mm256_unpacklo_pd(rc3, rd);
    __m256d rd3 = _mm256_unpackhi_pd(rc3, rd);
    __m256d rc4v = _mm256_unpacklo_pd(rc3v, rdv);
    __m256d rd3v = _mm256_unpackhi_pd(rc3v, rdv);

    /* shuffle (x3,x7) and (x9,x13) pairs */
    __m256d ra4 = _mm256_permute2f128_pd(ra3, rc4, 0x20);
    __m256d rc5 = _mm256_permute2f128_pd(ra3, rc4, 0x31);
    __m256d ra4v = _mm256_permute2f128_pd(ra3v, rc4v, 0x20);
    __m256d rc5v = _mm256_permute2f128_pd(ra3v, rc4v, 0x31);

    /* shuffle (x4,x8) and (x10,x14) pairs */
    __m256d rb5 = _mm256_permute2f128_pd(rb4, rd3, 0x20);
    __m256d rd4 = _mm256_permute2f128_pd(rb4, rd3, 0x31);
    __m256d rb5v = _mm256_permute2f128_pd(rb4v, rd3v, 0x20);
    __m256d rd4v = _mm256_permute2f128_pd(rb4v, rd3v, 0x31);

    /* after this, results are in ra4, rb5, rc5, rd4 */

    /* store */
    _mm256_storeu_pd((double *) output, ra4);
    _mm256_storeu_pd((double *) (output + 4), rb5);
    _mm256_storeu_pd((double *) (output + 8), rc5);
    _mm256_storeu_pd((double *) (output + 12), rd4);

    _mm256_storeu_pd((double *) outputv, ra4v);
    _mm256_storeu_pd((double *) (outputv + 4), rb5v);
    _mm256_storeu_pd((double *) (outputv + 8), rc5v);
    _mm256_storeu_pd((double *) (outputv + 12), rd4v);

}

inline void __attribute((always_inline))
    merge16_eqlen(int64_t * const inpA, int64_t * const inpAv,
                  int64_t * const inpB, int64_t * const inpBv,
                  int64_t * const out, int64_t * const outv,
                  const uint32_t len)
{
    register block16 * inA  = (block16 *) inpA;
    register block16 * inB  = (block16 *) inpB;
    register block16 * inAv  = (block16 *) inpAv;
    register block16 * inBv  = (block16 *) inpBv;
    block16 * const    endA = (block16 *) (inpA + len);
    block16 * const    endB = (block16 *) (inpB + len);

    block16 * outp = (block16 *) out;
    block16 * outpv = (block16 *) outv;

    register block16 * next = inB;
    register block16 * nextv = inBv;

    __m256d outreg1l1, outreg1l2, outreg1h1, outreg1h2;
    __m256d outreg2l1, outreg2l2, outreg2h1, outreg2h2;
    __m256d outreg1l1v, outreg1l2v, outreg1h1v, outreg1h2v;
    __m256d outreg2l1v, outreg2l2v, outreg2h1v, outreg2h2v;

    __m256d regAl1, regAl2, regAh1, regAh2;
    __m256d regBl1, regBl2, regBh1, regBh2;
    __m256d regAl1v, regAl2v, regAh1v, regAh2v;
    __m256d regBl1v, regBl2v, regBh1v, regBh2v;

    LOAD8U(regAl1, regAl2, inA);
    LOAD8U(regAh1, regAh2, ((block8 *)(inA) + 1));
    inA ++;
    LOAD8U(regAl1v, regAl2v, inAv);
    LOAD8U(regAh1v, regAh2v, ((block8 *)(inAv) + 1));
    inAv ++;

    LOAD8U(regBl1, regBl2, inB);
    LOAD8U(regBh1, regBh2, ((block8 *)(inB) + 1));
    inB ++;
    LOAD8U(regBl1v, regBl2v, inBv);
    LOAD8U(regBh1v, regBh2v, ((block8 *)(inBv) + 1));
    inBv ++;

    BITONIC_MERGE16(outreg1l1, outreg1l1v, outreg1l2, outreg1l2v,
                    outreg1h1, outreg1h1v, outreg1h2, outreg1h2v,
                    outreg2l1, outreg2l1v, outreg2l2, outreg2l2v,
                    outreg2h1, outreg2h1v, outreg2h2, outreg2h2v,
                    regAl1, regAl1v, regAl2, regAl2v, regAh1, regAh1v,
                    regAh2, regAh2v, regBl1, regBl1v, regBl2, regBl2v,
                    regBh1, regBh1v, regBh2, regBh2v);

    /* store outreg1 */
    STORE8U(outp, outreg1l1, outreg1l2);
    STORE8U(outpv, outreg1l1v, outreg1l2v);
    STORE8U(((block8 *)outp + 1), outreg1h1, outreg1h2);
    STORE8U(((block8 *)outpv + 1), outreg1h1v, outreg1h2v);
    outp ++;
    outpv ++;

    while( inA < endA && inB < endB ) {

        /** The inline assembly below does exactly the following code: */
        /* Option 3: with assembly */
        IFELSECONDMOVE(next, nextv, inA, inAv, inB, inBv, 128);

        regAl1 = outreg2l1;
        regAl2 = outreg2l2;
        regAh1 = outreg2h1;
        regAh2 = outreg2h2;

        regAl1v = outreg2l1v;
        regAl2v = outreg2l2v;
        regAh1v = outreg2h1v;
        regAh2v = outreg2h2v;

        LOAD8U(regBl1, regBl2, next);
        LOAD8U(regBh1, regBh2, ((block8 *)next + 1));
        LOAD8U(regBl1v, regBl2v, nextv);
        LOAD8U(regBh1v, regBh2v, ((block8 *)nextv + 1));

        BITONIC_MERGE16(outreg1l1, outreg1l1v, outreg1l2, outreg1l2v,
                        outreg1h1, outreg1h1v, outreg1h2, outreg1h2v,
                        outreg2l1, outreg2l1v, outreg2l2, outreg2l2v,
                        outreg2h1, outreg2h1v, outreg2h2, outreg2h2v,
                        regAl1, regAl1v, regAl2, regAl2v, regAh1, regAh1v,
                        regAh2, regAh2v, regBl1, regBl1v, regBl2, regBl2v,
                        regBh1, regBh1v, regBh2, regBh2v);

        /* store outreg1 */
        STORE8U(outp, outreg1l1, outreg1l2);
        STORE8U(((block8 *)outp + 1), outreg1h1, outreg1h2);
        outp ++;
        STORE8U(outpv, outreg1l1v, outreg1l2v);
        STORE8U(((block8 *)outpv + 1), outreg1h1v, outreg1h2v);
        outpv ++;

    }

    /* handle remaining items */
    while( inA < endA ) {
        __m256d regAl1, regAl2, regAh1, regAh2;
        __m256d regAl1v, regAl2v, regAh1v, regAh2v;
        __m256d regBl1 = outreg2l1;
        __m256d regBl2 = outreg2l2;
        __m256d regBh1 = outreg2h1;
        __m256d regBh2 = outreg2h2;
        __m256d regBl1v = outreg2l1v;
        __m256d regBl2v = outreg2l2v;
        __m256d regBh1v = outreg2h1v;
        __m256d regBh2v = outreg2h2v;

        LOAD8U(regAl1, regAl2, inA);
        LOAD8U(regAh1, regAh2, ((block8 *)(inA) + 1));
        inA ++;
        LOAD8U(regAl1v, regAl2v, inAv);
        LOAD8U(regAh1v, regAh2v, ((block8 *)(inAv) + 1));
        inAv ++;

        BITONIC_MERGE16(outreg1l1, outreg1l1v, outreg1l2, outreg1l2v,
                        outreg1h1, outreg1h1v, outreg1h2, outreg1h2v,
                        outreg2l1, outreg2l1v, outreg2l2, outreg2l2v,
                        outreg2h1, outreg2h1v, outreg2h2, outreg2h2v,
                        regAl1, regAl1v, regAl2, regAl2v, regAh1, regAh1v,
                        regAh2, regAh2v, regBl1, regBl1v, regBl2, regBl2v,
                        regBh1, regBh1v, regBh2, regBh2v);

        /* store outreg1 */
        STORE8U(outp, outreg1l1, outreg1l2);
        STORE8U(((block8 *)outp + 1), outreg1h1, outreg1h2);
        outp ++;
        STORE8U(outpv, outreg1l1v, outreg1l2v);
        STORE8U(((block8 *)outpv + 1), outreg1h1v, outreg1h2v);
        outpv ++;

    }

    while( inB < endB ) {
        __m256d regBl1, regBl2, regBh1, regBh2;
        __m256d regBl1v, regBl2v, regBh1v, regBh2v;

        __m256d regAl1 = outreg2l1;
        __m256d regAl2 = outreg2l2;
        __m256d regAh1 = outreg2h1;
        __m256d regAh2 = outreg2h2;
        __m256d regAl1v = outreg2l1v;
        __m256d regAl2v = outreg2l2v;
        __m256d regAh1v = outreg2h1v;
        __m256d regAh2v = outreg2h2v;

        LOAD8U(regBl1, regBl2, inB);
        LOAD8U(regBh1, regBh2, ((block8 *)inB + 1));
        inB ++;
        LOAD8U(regBl1v, regBl2v, inBv);
        LOAD8U(regBh1v, regBh2v, ((block8 *)inBv + 1));
        inBv ++;

        BITONIC_MERGE16(outreg1l1, outreg1l1v, outreg1l2, outreg1l2v,
                        outreg1h1, outreg1h1v, outreg1h2, outreg1h2v,
                        outreg2l1, outreg2l1v, outreg2l2, outreg2l2v,
                        outreg2h1, outreg2h1v, outreg2h2, outreg2h2v,
                        regAl1, regAl1v, regAl2, regAl2v, regAh1, regAh1v,
                        regAh2, regAh2v, regBl1, regBl1v, regBl2, regBl2v,
                        regBh1, regBh1v, regBh2, regBh2v);

        /* store outreg1 */
        STORE8U(outp, outreg1l1, outreg1l2);
        STORE8U(((block8 *)outp + 1), outreg1h1, outreg1h2);
        outp ++;
        STORE8U(outpv, outreg1l1v, outreg1l2v);
        STORE8U(((block8 *)outpv + 1), outreg1h1v, outreg1h2v);
        outpv ++;
    }

    /* store the last remaining register values */
    STORE8U(outp, outreg2l1, outreg2l2);
    STORE8U(((block8 *)outp + 1), outreg2h1, outreg2h2);
    STORE8U(outpv, outreg2l1v, outreg2l2v);
    STORE8U(((block8 *)outpv + 1), outreg2h1v, outreg2h2v);

}

/**
 * Merge two sorted arrays to a final output using 16-way AVX bitonic merge.
 *
 * @param inpA input array A
 * @param inpB input array B
 * @param Out  output array
 * @param lenA size of A
 * @param lenB size of B
 */
inline void __attribute__((always_inline))
merge16_varlen(int64_t * restrict inpA, int64_t * restrict inpAv,
               int64_t * restrict inpB, int64_t * restrict inpBv,
               int64_t * restrict Out, int64_t * restrict Outv,
               const uint32_t lenA, const uint32_t lenB)
{
    uint32_t lenA16 = lenA & ~0xF, lenB16 = lenB & ~0xF;
    uint32_t ai = 0, bi = 0;

    int64_t * out = Out;
    int64_t * outv = Outv;

    if(lenA16 > 16 && lenB16 > 16) {

        register block16 * inA  = (block16 *) inpA;
        register block16 * inB  = (block16 *) inpB;
        register block16 * inAv  = (block16 *) inpAv;
        register block16 * inBv  = (block16 *) inpBv;
        block16 * const    endA = (block16 *) (inpA + lenA) - 1;
        block16 * const    endB = (block16 *) (inpB + lenB) - 1;

        block16 * outp = (block16 *) out;
        block16 * outpv = (block16 *) outv;

        register block16 * next = inB;
        register block16 * nextv = inBv;

        __m256d outreg1l1, outreg1l2, outreg1h1, outreg1h2;
        __m256d outreg2l1, outreg2l2, outreg2h1, outreg2h2;
        __m256d outreg1l1v, outreg1l2v, outreg1h1v, outreg1h2v;
        __m256d outreg2l1v, outreg2l2v, outreg2h1v, outreg2h2v;

        __m256d regAl1, regAl2, regAh1, regAh2;
        __m256d regBl1, regBl2, regBh1, regBh2;
        __m256d regAl1v, regAl2v, regAh1v, regAh2v;
        __m256d regBl1v, regBl2v, regBh1v, regBh2v;

        LOAD8U(regAl1, regAl2, inA);
        LOAD8U(regAh1, regAh2, ((block8 *)(inA) + 1));
        inA ++;
        LOAD8U(regAl1v, regAl2v, inAv);
        LOAD8U(regAh1v, regAh2v, ((block8 *)(inAv) + 1));
        inAv ++;

        LOAD8U(regBl1, regBl2, inB);
        LOAD8U(regBh1, regBh2, ((block8 *)(inB) + 1));
        inB ++;
        LOAD8U(regBl1v, regBl2v, inBv);
        LOAD8U(regBh1v, regBh2v, ((block8 *)(inBv) + 1));
        inBv ++;

        BITONIC_MERGE16(outreg1l1, outreg1l1v, outreg1l2, outreg1l2v,
                        outreg1h1, outreg1h1v, outreg1h2, outreg1h2v,
                        outreg2l1, outreg2l1v, outreg2l2, outreg2l2v,
                        outreg2h1, outreg2h1v, outreg2h2, outreg2h2v,
                        regAl1, regAl1v, regAl2, regAl2v,
                        regAh1, regAh1v, regAh2, regAh2v,
                        regBl1, regBl1v, regBl2, regBl2v,
                        regBh1, regBh1v, regBh2, regBh2v);

        /* store outreg1 */
        STORE8U(outp, outreg1l1, outreg1l2);
        STORE8U(((block8 *)outp + 1), outreg1h1, outreg1h2);
        outp ++;
        STORE8U(outpv, outreg1l1v, outreg1l2v);
        STORE8U(((block8 *)outpv + 1), outreg1h1v, outreg1h2v);
        outpv ++;

        while( inA < endA && inB < endB ) {

            /** The inline assembly below does exactly the following code: */
            /* Option 3: with assembly */
            IFELSECONDMOVE(next, nextv, inA, inAv, inB, inBv, 128);

            regAl1 = outreg2l1;
            regAl2 = outreg2l2;
            regAh1 = outreg2h1;
            regAh2 = outreg2h2;
            regAl1v = outreg2l1v;
            regAl2v = outreg2l2v;
            regAh1v = outreg2h1v;
            regAh2v = outreg2h2v;

            LOAD8U(regBl1, regBl2, next);
            LOAD8U(regBh1, regBh2, ((block8 *)next + 1));
            LOAD8U(regBl1v, regBl2v, nextv);
            LOAD8U(regBh1v, regBh2v, ((block8 *)nextv + 1));

            BITONIC_MERGE16(outreg1l1, outreg1l1v, outreg1l2, outreg1l2v,
                        outreg1h1, outreg1h1v, outreg1h2, outreg1h2v,
                        outreg2l1, outreg2l1v, outreg2l2, outreg2l2v,
                        outreg2h1, outreg2h1v, outreg2h2, outreg2h2v,
                        regAl1, regAl1v, regAl2, regAl2v,
                        regAh1, regAh1v, regAh2, regAh2v,
                        regBl1, regBl1v, regBl2, regBl2v,
                        regBh1, regBh1v, regBh2, regBh2v);

            /* store outreg1 */
            STORE8U(outp, outreg1l1, outreg1l2);
            STORE8U(((block8 *)outp + 1), outreg1h1, outreg1h2);
            outp ++;
            STORE8U(outpv, outreg1l1v, outreg1l2v);
            STORE8U(((block8 *)outpv + 1), outreg1h1v, outreg1h2v);
            outpv ++;
        }

        /* flush the register to one of the lists */
        int64_t hireg[4] __attribute__((aligned(32)));
        _mm256_store_pd ( (double *)hireg, outreg2h2);

        if(*((int64_t *)inA) >= *((int64_t*)(hireg+3))) {
            /* store the last remaining register values to A */
            inA --;
            STORE8U(inA, outreg2l1, outreg2l2);
            STORE8U(((block8 *)inA + 1), outreg2h1, outreg2h2);
            inAv --;
            STORE8U(inAv, outreg2l1v, outreg2l2v);
            STORE8U(((block8 *)inAv + 1), outreg2h1v, outreg2h2v);
        }
        else {
            /* store the last remaining register values to B */
            inB --;
            STORE8U(inB, outreg2l1, outreg2l2);
            STORE8U(((block8 *)inB + 1), outreg2h1, outreg2h2);
            inBv --;
            STORE8U(inBv, outreg2l1v, outreg2l2v);
            STORE8U(((block8 *)inBv + 1), outreg2h1v, outreg2h2v);
        }

        ai = ((int64_t *)inA - inpA);
        bi = ((int64_t *)inB - inpB);

        inpA = (int64_t *)inA;
        inpB = (int64_t *)inB;
        out  = (int64_t *)outp;
        inpAv = (int64_t *)inAv;
        inpBv = (int64_t *)inBv;
        outv  = (int64_t *)outpv;
    }
    /* serial-merge */
    while(ai < lenA && bi < lenB){
        int64_t * in = inpB;
        int64_t * inv = inpBv;
        uint32_t cmp = (*inpA < *inpB);
        uint32_t notcmp = !cmp;

        ai += cmp;
        bi += notcmp;

        if(cmp) {
            in = inpA;
            inv = inpAv;
        }
        *out = *in;
        *outv = *inv;
        out ++;
        outv ++;
        inpA += cmp;
        inpB += notcmp;
        inpAv += cmp;
        inpBv += notcmp;
    }

    if(ai < lenA) {
        /* if A has any more items to be output */

        if((lenA - ai) >= 8) {
            /* if A still has some times to be output with AVX */
            uint32_t lenA8 = ((lenA-ai) & ~0x7);
            register block8 * inA  = (block8 *) inpA;
            block8 * const    endA = (block8 *) (inpA + lenA8);
            block8 * outp = (block8 *) out;
            register block8 * inAv  = (block8 *) inpAv;
            block8 * outpv = (block8 *) outv;

            while(inA < endA) {
                __m256d regAl, regAh;
                __m256d regAlv, regAhv;

                LOAD8U(regAl, regAh, inA);
                STORE8U(outp, regAl, regAh);
                LOAD8U(regAlv, regAhv, inAv);
                STORE8U(outpv, regAlv, regAhv);

                outp ++;
                inA ++;
                outpv ++;
                inAv ++;
            }

            ai   += ((int64_t*)inA - inpA);
            inpA  = (int64_t *)inA;
            out   = (int64_t *)outp;
            inpAv  = (int64_t *)inAv;
            outv   = (int64_t *)outpv;
        }

        while(ai < lenA) {
            *out = *inpA;
            ai++;
            out++;
            inpA++;
            *outv = *inpAv;
            outv++;
            inpAv++;

        }
    }
    else if(bi < lenB) {
        /* if B has any more items to be output */

        if((lenB - bi) >= 8) {
            /* if B still has some times to be output with AVX */
            uint32_t lenB8 = ((lenB-bi) & ~0x7);
            register block8 * inB  = (block8 *) inpB;
            block8 * const    endB = (block8 *) (inpB + lenB8);
            block8 * outp = (block8 *) out;
            register block8 * inBv  = (block8 *) inpBv;
            block8 * outpv = (block8 *) outv;

            while(inB < endB) {
                __m256d regBl, regBh;
                __m256d regBlv, regBhv;
                LOAD8U(regBl, regBh, inB);
                STORE8U(outp, regBl, regBh);
                LOAD8U(regBlv, regBhv, inBv);
                STORE8U(outpv, regBlv, regBhv);
                outp ++;
                inB ++;
                outpv ++;
                inBv ++;
            }

            bi   += ((int64_t*)inB - inpB);
            inpB  = (int64_t *)inB;
            out   = (int64_t *)outp;
            inpBv  = (int64_t *)inBv;
            outv   = (int64_t *)outpv;
        }

        while(bi < lenB) {
            *out = *inpB;
            *outv = *inpBv;
            bi++;
            out++;
            inpB++;
            outv++;
            inpBv++;
        }
    }
}

inline void __attribute__((always_inline))
avxsort_block(int64_t ** inputptr, int64_t ** inputptrv, int64_t ** outputptr,
              int64_t ** outputptrv, int BLOCK_SIZE)
{
    int64_t * ptrs[2];
    int64_t * ptrsv[2];

    const uint64_t logBSZ = log2(BLOCK_SIZE);

    ptrs[0] = *inputptr;
    ptrs[1] = *outputptr;
    ptrsv[0] = *inputptrv;
    ptrsv[1] = *outputptrv;

    /** 1.a) Perform in-register sort to get sorted seq of K(K=4)*/
    block16 * inptr = (block16 *) ptrs[0];
    block16 * inptrv = (block16 *) ptrsv[0];

    block16 * const end = (block16 *) (ptrs[0] + BLOCK_SIZE);

    while(inptr < end) {
        inregister_sort_keyval32((int64_t*)inptr,(int64_t*)inptrv,
                                 (int64_t*)inptr, (int64_t*)inptrv);
        inptr ++;
        inptrv ++;
    }


    /**
     * 1.b) for itr <- [(logK) .. (logM - 3)]
     *  - Simultaneously merge 4 sequences (using a K by K
     *  network) of length 2^itr to obtain sorted seq. of 2^{itr+1}
     */
    uint64_t j;
    const uint64_t jend = logBSZ - 2;

    j = 2;
    {
        int ptridx = j & 1;
        int64_t * inp = (int64_t *) ptrs[ptridx];
        int64_t * out = (int64_t *) ptrs[ptridx ^ 1];
        int64_t * inpv = (int64_t *) ptrsv[ptridx];
        int64_t * outv = (int64_t *) ptrsv[ptridx ^ 1];

        int64_t * const end = (int64_t*) (inp + BLOCK_SIZE);

        /**
         *  merge length 2^j lists beginnig at inp and output a
         *  sorted list of length 2^(j+1) starting at out
         */
        const uint64_t inlen  = (1 << j);
        const uint64_t outlen = (inlen << 1);

        while(inp < end) {

            merge4_eqlen(inp, inpv, inp + inlen, inpv + inlen, out, outv, inlen);
            inp += outlen;
            inpv += outlen;
            out += outlen;
            outv += outlen;
        }
    }
    j = 3;
    {
        int ptridx = j & 1;
        int64_t * inp = (int64_t *) ptrs[ptridx];
        int64_t * out = (int64_t *) ptrs[ptridx ^ 1];
        int64_t * inpv = (int64_t *) ptrsv[ptridx];
        int64_t * outv = (int64_t *) ptrsv[ptridx ^ 1];
        int64_t * const end = (int64_t*) (inp + BLOCK_SIZE);

        /**
         *  merge length 2^j lists beginnig at inp and output a
         *  sorted list of length 2^(j+1) starting at out
         */
        const uint64_t inlen  = (1 << j);
        const uint64_t outlen = (inlen << 1);

        while(inp < end) {

            merge8_eqlen(inp, inpv, inp + inlen, inpv + inlen, out, outv, inlen);
            inp += outlen;
            inpv += outlen;
            out += outlen;
            outv += outlen;
        }
    }
    for(j = 4; j < jend; j++) {
        int ptridx = j & 1;
        int64_t * inp = (int64_t *) ptrs[ptridx];
        int64_t * out = (int64_t *) ptrs[ptridx ^ 1];
        int64_t * inpv = (int64_t *) ptrsv[ptridx];
        int64_t * outv = (int64_t *) ptrsv[ptridx ^ 1];
        int64_t * const end = (int64_t*) (inp + BLOCK_SIZE);

        /**
         *  merge length 2^j lists beginnig at inp and output a
         *  sorted list of length 2^(j+1) starting at out
         */
        const uint64_t inlen  = (1 << j);
        const uint64_t outlen = (inlen << 1);

        while(inp < end) {

            merge16_eqlen(inp, inpv, inp + inlen, inpv + inlen,
                          out, outv, inlen);
            inp += outlen;
            inpv += outlen;
            out += outlen;
            outv += outlen;

            /* TODO: Try following. */
            /* simultaneous merge of 4 list pairs */
            /* merge 4 seqs simultaneously (always >= 4) */
            /* merge 2 seqs simultaneously (always >= 2) */
        }
    }

    /**
     * 1.c) for itr = (logM - 2), simultaneously merge 2 sequences
     *  (using a 2K by 2K network) of length M/4 to obtain sorted
     *  sequences of M/2.
     */
    uint64_t inlen  = (1 << j);
    int64_t * inp;
    int64_t * out;
    int64_t * inpv;
    int64_t * outv;

    int ptridx = j & 1;

    inp = ptrs[ptridx];
    out = ptrs[ptridx ^ 1];
    inpv = ptrsv[ptridx];
    outv = ptrsv[ptridx ^ 1];

    merge16_eqlen(inp, inpv, inp + inlen, inpv + inlen, out, outv, inlen);
    merge16_eqlen(inp+2*inlen, inpv+2*inlen, inp+3*inlen, inpv+3*inlen,
                  out + 2*inlen, outv + 2*inlen, inlen);

    /* TODO: simultaneous merge of 2 list pairs */
    /**
     * 1.d) for itr = (logM - 1), merge 2 final sequences (using a
     * 4K by 4K network) of length M/2 to get sorted seq. of M.
     */
    j++; /* j=(LOG2_BLOCK_SIZE-1); inputsize M/2 --> outputsize M*/
    inlen  = (1 << j);
    /* now we know that input is out from the last pass */
    merge16_eqlen(out, outv, out + inlen, outv + inlen, inp, inpv, inlen);

    /* finally swap input/output ptrs, output is the sorted list */
    * outputptr = inp;
    * inputptr  = out;
    * outputptrv = inpv;
    * inputptrv  = outv;

}

inline __attribute__((__always_inline__))
int keycmp(const void * k1, const void * k2)
{
    int64_t val = (*(int64_t *)k1 - *(int64_t *)k2);

    int ret = 0;
    if(val < 0)
        ret = -1;
    else if(val > 0)
        ret = 1;

    return ret;
}

inline __attribute__((__always_inline__)) void
swap(int64_t ** A, int64_t ** B)
{
    int64_t * tmp = *A;
    *A = *B;
    *B = tmp;
}

struct kvpair {
    int64_t key;
    int64_t val;
};

struct {
    bool operator()(struct kvpair a, struct kvpair b) const
    {
        return a.key < b.key;
    }
} kvcmp;
inline void __attribute__((always_inline))
x2_sort(int64_t *key, int64_t *val, uint32_t nitems)
{
    uint32_t i;
    struct kvpair *arr = (struct kvpair*)malloc(nitems * sizeof(struct kvpair));
    // dump_arr_int64("key", key, nitems);
    // dump_arr_int64("val", val, nitems);
    for (i = 0; i < nitems; ++i) {
        arr[i].key = key[i];
        arr[i].val = val[i];
    }
    std::sort(arr, arr + nitems, kvcmp);
    for (i = 0; i < nitems; ++i) {
        key[i] = arr[i].key;
        val[i] = arr[i].val;
    }
    free(arr);
}
/**
 * Sorts the last chunk of the input, which is less than BLOCKSIZE tuples.
 * @note This function assumes a hard-coded BLOCKSIZE of 16384 and nitems must
 * be less than 16384.
 *
 * @param inputptr
 * @param outputptr
 * @param nitems
 */
inline void __attribute__((always_inline))
avxsort_rem(int64_t ** inputptr, int64_t ** inputptrv,
            int64_t ** outputptr, int64_t ** outputptrv, uint32_t nitems)
{
    int64_t * inp = *inputptr;
    int64_t * out = *outputptr;
    int64_t * inpv = *inputptrv;
    int64_t * outv = *outputptrv;

    /* each chunk keeps track of its temporary memory offset */
    int64_t * ptrs[8][2];/* [chunk-in, chunk-out-tmp] */
    int64_t * ptrsv[8][2];/* [chunk-in, chunk-out-tmp] */

    uint32_t n = nitems, pos = 0, i = 0;
    uint32_t nxtpow = 8192;
    uint32_t sizes[6];

    while(n < nxtpow) {
        nxtpow >>= 1;
    }

    while(nxtpow > 128) {
        ptrs[i][0] = inp + pos;
        ptrs[i][1] = out + pos;
        ptrsv[i][0] = inpv + pos;
        ptrsv[i][1] = outv + pos;

        sizes[i]   = nxtpow;

        avxsort_block(&ptrs[i][0], &ptrsv[i][0], &ptrs[i][1], &ptrsv[i][1],
                      nxtpow);
        pos += nxtpow;
        n   -= nxtpow;
        swap(&ptrs[i][0], &ptrs[i][1]);
        swap(&ptrsv[i][0], &ptrsv[i][1]);
        i++;

        while(n < nxtpow) {
            nxtpow >>= 1;
        }
    }

    if(n > 0) {
        /* sort last n < 128 items using scalar sort */
        ptrs[i][0] = inp + pos;
        ptrs[i][1] = out + pos;
        ptrsv[i][0] = inpv + pos;
        ptrsv[i][1] = outv + pos;
        sizes[i]   = n;

        x2_sort(ptrs[i][0], ptrsv[i][0], n);

        /* no need to swap */
        i++;
    }

    uint32_t nchunks = i;

    /* merge sorted blocks */
    while(nchunks > 1) {
        uint64_t k = 0;
        for(uint64_t j = 0; j < (nchunks-1); j += 2) {
            int64_t * inpA  = ptrs[j][0];
            int64_t * inpB  = ptrs[j+1][0];
            int64_t * out   = ptrs[j][1];
            int64_t * inpAv  = ptrsv[j][0];
            int64_t * inpBv  = ptrsv[j+1][0];
            int64_t * outv   = ptrsv[j][1];

            uint32_t  sizeA = sizes[j];
            uint32_t  sizeB = sizes[j+1];

            merge16_varlen(inpA, inpAv, inpB, inpBv, out, outv,
                           sizeA, sizeB);

            /* setup new pointers */
            ptrs[k][0] = out;
            ptrs[k][1] = inpA;
            ptrsv[k][0] = outv;
            ptrsv[k][1] = inpAv;

            sizes[k]   = sizeA + sizeB;
            k++;
        }

        if((nchunks % 2)) {
            /* just move the pointers */
            ptrs[k][0] = ptrs[nchunks-1][0];
            ptrs[k][1] = ptrs[nchunks-1][1];
            ptrsv[k][0] = ptrsv[nchunks-1][0];
            ptrsv[k][1] = ptrsv[nchunks-1][1];
            sizes[k]   = sizes[nchunks-1];
            k++;
        }

        nchunks = k;
    }

    /* finally swap input/output pointers, where output holds the sorted list */
    * outputptr = ptrs[0][0];
    * inputptr  = ptrs[0][1];
    * outputptrv = ptrsv[0][0];
    * inputptrv  = ptrsv[0][1];
}
