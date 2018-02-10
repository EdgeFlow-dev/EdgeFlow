/* To Build
   g++ -O3 -std=c++11 -mavx2 -I../../include/ test.c utils.cpp -o test
*/

#include <immintrin.h> /* AVX intrinsics */
#include "avxcommon.h"
#include <iostream>
#include "avxsort_core.h"
#include <cstdio>
#include "utils.h"
using namespace std;

void test_cmp_kv_pd(void)
{
    double inA[4] = {5.1, 7, 6, 2};
    double inAv[4] = {5, 7, 6, 2};
    double inB[4] = {3, 9, 8, 2};
    double inBv[4] = {3, 9, 8, 4};

    double ret[4];
    __m256d inA_r = _mm256_loadu_pd(inA);
    __m256d inAv_r = _mm256_loadu_pd(inAv);
    __m256d inB_r = _mm256_loadu_pd(inB);
    __m256d inBv_r = _mm256_loadu_pd(inBv);
    __m256d keyh, keyl, valh, vall;

    _mm256_storeu_pd(ret, inA_r);
    dump_arr_double("inA_r", ret);
    _mm256_storeu_pd(ret, inAv_r);
    dump_arr_double("inAv_r", ret);
    _mm256_storeu_pd(ret, inB_r);
    dump_arr_double("inB_r", ret);
    _mm256_storeu_pd(ret, inBv_r);
    dump_arr_double("inBv_r", ret);

    _mm256_cmp_kv_pd(inA_r, inAv_r, inB_r, inBv_r, keyh, valh, keyl, vall);

    _mm256_storeu_pd(ret, keyh);
    dump_arr_double("keyh", ret);
    _mm256_storeu_pd(ret, valh);
    dump_arr_double("valh", ret);
    _mm256_storeu_pd(ret, keyl);
    dump_arr_double("keyl", ret);
    _mm256_storeu_pd(ret, vall);
    dump_arr_double("vall", ret);
}
void test_inregister_sort(void)
{
    int64_t in[16] =  { 4, 12, 345, 54,    2,  534,  24,  24, 132,  23, 123,  14, 14,  41, 53};
    int64_t inv[16] = {13, 43,  34, 12, 2352, 2431, 423, 143, 124, 234, 134, 124, 14, 414, 46};
    int64_t out[16];
    int64_t outv[16];
    int64_t out1[16];
    int64_t outv1[16];
    inregister_sort_keyval32(in, inv, out, outv);
    dump_arr_int64("out", out, 16);
    dump_arr_int64("outv", outv, 16);

}

#define LENA 100
#define LENB 100
void test_merge16_varlen(void)
{
    int64_t inA[LENA] =  {44,  49,  70,  74,  82,  108,  115,  130,  163,  165,  180,  185,  187,  190,  195,  203,  225,  234,  273,  303,  320,  330,  333,  339,  341,  345,  349,  350,  371,  373,  381,  404,  406,  407,  425,  432,  435,  445,  453,  458,  469,  501,  517,  521,  536,  557,  558,  561,  565,  591,  599,  603,  613,  619,  621,  629,  639,  644,  649,  650,  662,  667,  679,  680,  683,  700,  704,  706,  707,  710,  722,  723,  724,  725,  725,  728,  730,  739,  742,  769,  773,  774,  777,  807,  838,  863,  866,  879,  884,  889,  895,  895,  919,  934,  942,  946,  951,  960,  960,  980};
    int64_t inAv[LENA] = {44,  49,  70,  74,  82,  108,  115,  130,  163,  165,  180,  185,  187,  190,  195,  203,  225,  234,  273,  303,  320,  330,  333,  339,  341,  345,  349,  350,  371,  373,  381,  404,  406,  407,  425,  432,  435,  445,  453,  458,  469,  501,  517,  521,  536,  557,  558,  561,  565,  591,  599,  603,  613,  619,  621,  629,  639,  644,  649,  650,  662,  667,  679,  680,  683,  700,  704,  706,  707,  710,  722,  723,  724,  725,  725,  728,  730,  739,  742,  769,  773,  774,  777,  807,  838,  863,  866,  879,  884,  889,  895,  895,  919,  934,  942,  946,  951,  960,  960,  980};
    int64_t inB[LENB] = {18,  28,  60,  68,  79,  80,  86,  89,  96,  129,  170,  179,  180,  202,  202,  221,  225,  227,  234,  235,  287,  288,  309,  314,  320,  335,  337,  343,  343,  345,  357,  357,  362,  390,  399,  400,  414,  414,  421,  445,  454,  456,  484,  510,  511,  522,  522,  532,  551,  561,  574,  585,  594,  609,  618,  628,  634,  639,  643,  683,  698,  700,  705,  736,  758,  762,  774,  775,  779,  799,  805,  811,  814,  816,  820,  841,  845,  847,  847,  851,  865,  874,  875,  879,  896,  897,  903,  908,  922,  927,  935,  942,  956,  962,  969,  970,  977,  981,  988,  997};
    int64_t inBv[LENB] = {18,  28,  60,  68,  79,  80,  86,  89,  96,  129,  170,  179,  180,  202,  202,  221,  225,  227,  234,  235,  287,  288,  309,  314,  320,  335,  337,  343,  343,  345,  357,  357,  362,  390,  399,  400,  414,  414,  421,  445,  454,  456,  484,  510,  511,  522,  522,  532,  551,  561,  574,  585,  594,  609,  618,  628,  634,  639,  643,  683,  698,  700,  705,  736,  758,  762,  774,  775,  779,  799,  805,  811,  814,  816,  820,  841,  845,  847,  847,  851,  865,  874,  875,  879,  896,  897,  903,  908,  922,  927,  935,  942,  956,  962,  969,  970,  977,  981,  988,  997};
    int64_t out[LENA + LENB];
    int64_t outv[LENA + LENB];

    merge16_varlen(inA, inAv, inB, inBv, out, outv, LENA, LENB);
    dump_arr_int64("out", out, LENA + LENB);
    dump_arr_int64("outv", outv, LENA + LENB);
}

extern void
avxsort_unaligned(int64_t ** inputptr, int64_t ** inputptrv, 
                  int64_t ** outputptr, int64_t ** outputptrv, uint64_t nitems);

#define NUM_ITEMS (1024 * 1024)  /* 2 * L2_CACHE_SIZE */

int main(int argc, char *argv[])
{
    /* FILE *fh; */
    /* int64_t record[8]; */
    /* uint32_t count; */
    /* int64_t *inA, *inAv, *outA, *outAv; */
    /* inA = (int64_t*)malloc(NUM_ITEMS * sizeof(int64_t)); */
    /* inAv = (int64_t*)malloc(NUM_ITEMS * sizeof(int64_t)); */
    /* outA = (int64_t*)malloc(NUM_ITEMS * sizeof(int64_t)); */
    /* outAv = (int64_t*)malloc(NUM_ITEMS * sizeof(int64_t)); */
    /* if (argc != 2) { */
    /*     cout << "Usage test Filename" << endl; */
    /*     return -1; */
    /* } */

    /* fh = fopen(argv[1], "r+"); */
    /* if (!fh) { */
    /*     cout << "File " << argv[1] << "cannot be opened." << endl; */
    /*     return 1; */
    /* } */

    /* for (count = 0; count < NUM_ITEMS; ++count) { */
    /*     if (fscanf(fh, "%ld, %ld", &record[0], &record[1]) == EOF) { */
    /*         cout << "Error!" << endl; */
    /*         return 1; */
    /*     } */
    /*     inA[count] = record[0]; */
    /*     inAv[count] = record[1]; */
    /* } */
    /* dump_arr_int64("inA", inA, 256); */
    /* dump_arr_int64("inAv", inAv, 256); */
    /* avxsort_unaligned(&inA, &inAv, &outA, &outAv, NUM_ITEMS); */
    /* dump_arr_int64("outA", outA, 256); */
    /* dump_arr_int64("outAv", outAv, 256); */
    
    /* free(inA); */
    /* free(inAv); */
    /* free(outA); */
    /* free(outAv); */
    test_merge16_varlen();
    return 0;
}
