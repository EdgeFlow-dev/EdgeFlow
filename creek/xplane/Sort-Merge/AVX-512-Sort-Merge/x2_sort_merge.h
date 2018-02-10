/* Author: Hongyu Miao @ Purdue ECE 
 * Date: Nov 9th, 2017 
 * Description: This file provides sort/merge functions to sort <key,pointer> by key 
 * */
#ifndef X2_SORT_MERGE_H
#define X2_SORT_MERGE_H 
#include "x2_bitonic_sort_merge_core.h"
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <immintrin.h>
#include <cstdint>
/* List of Functions */
void x2_qsort(int64_t* input_k, int64_t* input_ptr, int64_t len);
void x2_qsort(int64_t* input_k, int64_t* input_ptr, int64_t* output_k, int64_t* output_ptr, int64_t len);
void x2_sort_every_8(int64_t* k, int64_t* ptr, uint64_t len);
void x2_sort_every_8(int64_t* k, int64_t* ptr, int64_t* out_k, int64_t* out_ptr, uint64_t len);
void x2_merge_8_aligned(int64_t* input1_k, int64_t* input1_ptr, int64_t* input2_k, int64_t* input2_ptr, int64_t* output_k, int64_t* output_ptr, int len1, int len2);
void x2_merge_8_unaligned(int64_t* input1_k, int64_t* input1_ptr, int64_t* input2_k, int64_t* input2_ptr, int64_t* output_k, int64_t* output_ptr, int len1, int len2);
void x2_sort(int64_t* k, int64_t* ptr, uint64_t len);
void x2_sort(int64_t* k, int64_t* ptr, int64_t* out_k, int64_t* out_ptr, uint64_t len);
bool check_sort_k(int64_t* k, uint64_t len);
bool check_sort_kptr(int64_t* k, int64_t* ptr, uint64_t len);


#endif /* X2_SORT_MERGE_H */
