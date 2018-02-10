/* Author: Hongyu Miao @ Purdue ECE 
 * Date: Nov 9th, 2017 
 * Description: This file provides bitonic sort/merge core functions to sort <key,pointer> by key 
 * */
#ifndef X2_BITONIC_SORT_MERGE_CORE_H
#define X2_BITONIC_SORT_MERGE_CORE_H 
#include <immintrin.h>

/* List of Functions */
void x2_cmp_pd(__m512d k1, __m512d ptr1, __m512d k2, __m512d ptr2, __m512d& min_k, __m512d& min_ptr, __m512d& max_k, __m512d& max_ptr);
void x2_cmp_pd_noequal(__m512d k1, __m512d ptr1, __m512d k2, __m512d ptr2, __m512d& min_k, __m512d& min_ptr, __m512d& max_k, __m512d& max_ptr);

void x2_bitonic_sort_avx512_1x8(__m512d& k, __m512d& ptr);
void x2_bitonic_sort_avx512_1x8(int64_t* k, int64_t* ptr);
void x2_bitonic_sort_avx512_1x8(int64_t* k, int64_t* ptr, int64_t* out_k, int64_t* out_ptr);

void x2_bitonic_merge_avx512_2x8(__m512d& in1_k, __m512d& in1_ptr,  __m512d& in2_k, __m512d& in2_ptr,  __m512d& out1_k, __m512d& out1_ptr, __m512d& out2_k, __m512d& out2_ptr);
void x2_bitonic_merge_avx512_2x8(int64_t* in1_k, int64_t* in1_ptr, int64_t* in2_k, int64_t* in2_ptr, int64_t* out1_k, int64_t* out1_ptr, int64_t* out2_k, int64_t* out2_ptr);

void x2_bitonic_sort_avx512_8x8(int64_t * k, int64_t * ptr);
void x2_bitonic_sort_avx512_8x8(int64_t * k, int64_t * ptr, int64_t* out_k, int64_t* out_ptr);

#endif /* X2_BITONIC_SORT_MERGE_CORE_H */
