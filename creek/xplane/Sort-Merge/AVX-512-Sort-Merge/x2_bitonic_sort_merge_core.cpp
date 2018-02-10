/* Author: Hongyu Miao @ Purdue ECE 
 * Date: Nov 9th, 2017 
 * Description: This file provides bitonic sort/merge core functions to sort <key,pointer> by key 
 * */
#ifndef X2_BITONIC_SORT_MERGE_CORE_H
#define X2_BITONIC_SORT_MERGE_CORE_H 
#include <immintrin.h>

/*
 * Function: replace _mm512_min_pd(k1, k2) and _mm512_max_pd(k1, k2) to move pointers while calculating min/max keys
 * @k1: one vector includes 8 keys
 * @ptr1: one vector includes 8 key's corresponding pointers
 * @k2: the other vector includes 8 keys
 * @ptr2: the other vector inclues 8 keys' corresponding pointers
 * @min_k: smaller values when comparing k1's and k2's corresponding keys
 * @min_ptr; smaller values' corresponding pointers
 * @max_k: larger values when comparing k1's and k2's corresponding keys
 * @max_ptr: larger values' corresponding pointers
 * Ref: https://stackoverflow.com/questions/16988199/how-to-choose-avx-compare-predicate-variants
 * Note: use _CMP_LE_OQ(<=) and _CMP_GE_OQ(>=) to deal with comparing two same keys
 * */
void x2_cmp_pd(__m512d k1, __m512d ptr1, __m512d k2, __m512d ptr2, 
               __m512d& min_k, __m512d& min_ptr, __m512d& max_k, __m512d& max_ptr){
        
	/* When there there 2 same keys, here is a bug if max and min use the same min_mask:
	__mmask8 min_mask = _mm512_cmp_pd_mask(k1, k2, _CMP_LE_OQ);
	
	min_k = _mm512_mask_mov_pd(k2, min_mask, k1);
	min_ptr = _mm512_mask_mov_pd(ptr2, min_mask, ptr1);

	max_k = _mm512_mask_mov_pd(k1, min_mask, k2);
	max_ptr = _mm512_mask_mov_pd(ptr1, min_mask, ptr2);
	*/

	/*should use min_mask and max_mask, instead of using same min_mask...*/
	__mmask8 min_mask = _mm512_cmp_pd_mask(k1, k2, _CMP_LE_OQ); //<=
	min_k = _mm512_mask_mov_pd(k2, min_mask, k1);
	min_ptr = _mm512_mask_mov_pd(ptr2, min_mask, ptr1);

	__mmask8 max_mask = _mm512_cmp_pd_mask(k1, k2, _CMP_GE_OQ); //>=
	max_k = _mm512_mask_mov_pd(k2, max_mask, k1);
	max_ptr = _mm512_mask_mov_pd(ptr2, max_mask, ptr1);
}


/*
 * Function: replace _mm512_min_pd(k1, k2) and _mm512_max_pd(k1, k2) to move pointers while calculating min/max keys
 * @k1: one vector includes 8 keys
 * @ptr1: one vector includes 8 key's corresponding pointers
 * @k2: the other vector includes 8 keys
 * @ptr2: the other vector inclues 8 keys' corresponding pointers
 * @min_k: smaller values when comparing k1's and k2's corresponding keys
 * @min_ptr; smaller values' corresponding pointers
 * @max_k: larger values when comparing k1's and k2's corresponding keys
 * @max_ptr: larger values' corresponding pointers
 * Ref: https://stackoverflow.com/questions/16988199/how-to-choose-avx-compare-predicate-variants
 * Diff from x_cmp_pd(): use "<=" and ">" [NOT using "<=" and ">="] 
 * */
void x2_cmp_pd_noequal(__m512d k1, __m512d ptr1, __m512d k2, __m512d ptr2, 
               __m512d& min_k, __m512d& min_ptr, __m512d& max_k, __m512d& max_ptr){
        
	__mmask8 min_mask = _mm512_cmp_pd_mask(k1, k2, _CMP_LE_OQ); //<=
	min_k = _mm512_mask_mov_pd(k2, min_mask, k1);
	min_ptr = _mm512_mask_mov_pd(ptr2, min_mask, ptr1);

	max_k = _mm512_mask_mov_pd(k1, min_mask, k2);
	max_ptr = _mm512_mask_mov_pd(ptr1, min_mask, ptr2);
	/*
	__mmask8 max_mask = _mm512_cmp_pd_mask(k1, k2, _CMP_GE_OQ); //>=
	max_k = _mm512_mask_mov_pd(k2, max_mask, k1);
	max_ptr = _mm512_mask_mov_pd(ptr2, max_mask, ptr1);
	*/
}


/*
 * Function: sort 8 values in input_k, while move each key's corresponding pointer when moving a key
 * @input_k: vector inclues 8 keys
 * @input_ptr: vector inclues 8 pointers
 * */
void x2_bitonic_sort_avx512_1x8(__m512d& input_k, __m512d& input_ptr){
    {
        __m512i idxNoNeigh = _mm512_set_epi64(6, 7, 4, 5, 2, 3, 0, 1);
        
	//__m512d permNeigh = _mm512_permutexvar_pd(idxNoNeigh, input);
        __m512d permNeigh_k = _mm512_permutexvar_pd(idxNoNeigh, input_k);
        __m512d permNeigh_ptr = _mm512_permutexvar_pd(idxNoNeigh, input_ptr);
        
	//__m512d permNeighMin = _mm512_min_pd(permNeigh, input);
        //__m512d permNeighMax = _mm512_max_pd(permNeigh, input);
	__m512d permNeighMin_k;
	__m512d permNeighMin_ptr;
	__m512d permNeighMax_k;
	__m512d permNeighMax_ptr;

	//x2_cmp_pd(k1, ptr1, k2, ptr2, min_k, min_ptr, max_k, max_ptr);
	x2_cmp_pd(input_k, input_ptr, permNeigh_k, permNeigh_ptr, 
		permNeighMin_k, permNeighMin_ptr, permNeighMax_k, permNeighMax_ptr);
	
	//input = _mm512_mask_mov_pd(permNeighMin, 0xAA, permNeighMax);
	input_k = _mm512_mask_mov_pd(permNeighMin_k, 0xAA, permNeighMax_k);
	input_ptr = _mm512_mask_mov_pd(permNeighMin_ptr, 0xAA, permNeighMax_ptr);
    }
    {
        __m512i idxNoNeigh = _mm512_set_epi64(4, 5, 6, 7, 0, 1, 2, 3);
        
	//__m512d permNeigh = _mm512_permutexvar_pd(idxNoNeigh, input);
        __m512d permNeigh_k = _mm512_permutexvar_pd(idxNoNeigh, input_k);
        __m512d permNeigh_ptr = _mm512_permutexvar_pd(idxNoNeigh, input_ptr);

        //__m512d permNeighMin = _mm512_min_pd(permNeigh, input);
        //__m512d permNeighMax = _mm512_max_pd(permNeigh, input);
	__m512d permNeighMin_k;
	__m512d permNeighMin_ptr;
	__m512d permNeighMax_k;
	__m512d permNeighMax_ptr;
	x2_cmp_pd(input_k, input_ptr, permNeigh_k, permNeigh_ptr, 
		permNeighMin_k, permNeighMin_ptr, permNeighMax_k, permNeighMax_ptr);
	
	//input = _mm512_mask_mov_pd(permNeighMin, 0xCC, permNeighMax);
	input_k = _mm512_mask_mov_pd(permNeighMin_k, 0xCC, permNeighMax_k);
	input_ptr = _mm512_mask_mov_pd(permNeighMin_ptr, 0xCC, permNeighMax_ptr);
    }
    {
        __m512i idxNoNeigh = _mm512_set_epi64(6, 7, 4, 5, 2, 3, 0, 1);
        //__m512d permNeigh = _mm512_permutexvar_pd(idxNoNeigh, input);
        __m512d permNeigh_k = _mm512_permutexvar_pd(idxNoNeigh, input_k);
        __m512d permNeigh_ptr = _mm512_permutexvar_pd(idxNoNeigh, input_ptr);
        
	//__m512d permNeighMin = _mm512_min_pd(permNeigh, input);
        //__m512d permNeighMax = _mm512_max_pd(permNeigh, input);
	__m512d permNeighMin_k;
	__m512d permNeighMin_ptr;
	__m512d permNeighMax_k;
	__m512d permNeighMax_ptr;
	x2_cmp_pd(input_k, input_ptr, permNeigh_k, permNeigh_ptr, 
		permNeighMin_k, permNeighMin_ptr, permNeighMax_k, permNeighMax_ptr);
        
	//input = _mm512_mask_mov_pd(permNeighMin, 0xAA, permNeighMax);
	input_k = _mm512_mask_mov_pd(permNeighMin_k, 0xAA, permNeighMax_k);
	input_ptr = _mm512_mask_mov_pd(permNeighMin_ptr, 0xAA, permNeighMax_ptr);
    }
    {
        __m512i idxNoNeigh = _mm512_set_epi64(0, 1, 2, 3, 4, 5, 6, 7);
        
	//__m512d permNeigh = _mm512_permutexvar_pd(idxNoNeigh, input);
        __m512d permNeigh_k = _mm512_permutexvar_pd(idxNoNeigh, input_k);
        __m512d permNeigh_ptr = _mm512_permutexvar_pd(idxNoNeigh, input_ptr);
	
	//__m512d permNeighMin = _mm512_min_pd(permNeigh, input);
        //__m512d permNeighMax = _mm512_max_pd(permNeigh, input);
	__m512d permNeighMin_k;
	__m512d permNeighMin_ptr;
	__m512d permNeighMax_k;
	__m512d permNeighMax_ptr;
	x2_cmp_pd(input_k, input_ptr, permNeigh_k, permNeigh_ptr, 
		permNeighMin_k, permNeighMin_ptr, permNeighMax_k, permNeighMax_ptr);
        
	//input = _mm512_mask_mov_pd(permNeighMin, 0xF0, permNeighMax);
	input_k = _mm512_mask_mov_pd(permNeighMin_k, 0xF0, permNeighMax_k);
	input_ptr = _mm512_mask_mov_pd(permNeighMin_ptr, 0xF0, permNeighMax_ptr);
    }
    {
        __m512i idxNoNeigh = _mm512_set_epi64(5, 4, 7, 6, 1, 0, 3, 2);
        
	//__m512d permNeigh = _mm512_permutexvar_pd(idxNoNeigh, input);
        __m512d permNeigh_k = _mm512_permutexvar_pd(idxNoNeigh, input_k);
        __m512d permNeigh_ptr = _mm512_permutexvar_pd(idxNoNeigh, input_ptr);
        
	//__m512d permNeighMin = _mm512_min_pd(permNeigh, input);
        //__m512d permNeighMax = _mm512_max_pd(permNeigh, input);
	__m512d permNeighMin_k;
	__m512d permNeighMin_ptr;
	__m512d permNeighMax_k;
	__m512d permNeighMax_ptr;
	x2_cmp_pd(input_k, input_ptr, permNeigh_k, permNeigh_ptr, 
		permNeighMin_k, permNeighMin_ptr, permNeighMax_k, permNeighMax_ptr);
        
	//input = _mm512_mask_mov_pd(permNeighMin, 0xCC, permNeighMax);
	input_k = _mm512_mask_mov_pd(permNeighMin_k, 0xCC, permNeighMax_k);
	input_ptr = _mm512_mask_mov_pd(permNeighMin_ptr, 0xCC, permNeighMax_ptr);
    }
    {
        __m512i idxNoNeigh = _mm512_set_epi64(6, 7, 4, 5, 2, 3, 0, 1);
        
	//__m512d permNeigh = _mm512_permutexvar_pd(idxNoNeigh, input);
        __m512d permNeigh_k = _mm512_permutexvar_pd(idxNoNeigh, input_k);
        __m512d permNeigh_ptr = _mm512_permutexvar_pd(idxNoNeigh, input_ptr);
	
	//__m512d permNeighMin = _mm512_min_pd(permNeigh, input);
        //__m512d permNeighMax = _mm512_max_pd(permNeigh, input);
	__m512d permNeighMin_k;
	__m512d permNeighMin_ptr;
	__m512d permNeighMax_k;
	__m512d permNeighMax_ptr;
	x2_cmp_pd(input_k, input_ptr, permNeigh_k, permNeigh_ptr, 
		permNeighMin_k, permNeighMin_ptr, permNeighMax_k, permNeighMax_ptr);
        
	//input = _mm512_mask_mov_pd(permNeighMin, 0xAA, permNeighMax);
	input_k = _mm512_mask_mov_pd(permNeighMin_k, 0xAA, permNeighMax_k);
	input_ptr = _mm512_mask_mov_pd(permNeighMin_ptr, 0xAA, permNeighMax_ptr);
    }
}


/*
 * Function: sort 8 values in k, while move each key's corresponding pointer when moving a key
 * @input_k: array inclues 8 keys
 * @input_ptr: array inclues 8 pointers
 * Note: a wrapper of x2_bitonic_sort_avx512_1x8(__m512d& input_k, __m512d& input_ptr) 
 * */
void x2_bitonic_sort_avx512_1x8(int64_t* k, int64_t* ptr){
    __m512d input_k = _mm512_loadu_pd(k);
    __m512d input_ptr = _mm512_loadu_pd(ptr);
    x2_bitonic_sort_avx512_1x8(input_k, input_ptr);    
    _mm512_storeu_pd(k, input_k);
    _mm512_storeu_pd(ptr, input_ptr);
}

/* store sorted k/ptr to out_k/out_ptr*/
void x2_bitonic_sort_avx512_1x8(int64_t* k, int64_t* ptr, int64_t* out_k, int64_t* out_ptr){
    __m512d input_k = _mm512_loadu_pd(k);
    __m512d input_ptr = _mm512_loadu_pd(ptr);
    x2_bitonic_sort_avx512_1x8(input_k, input_ptr);    
    _mm512_storeu_pd(out_k, input_k);
    _mm512_storeu_pd(out_ptr, input_ptr);
}

/*
 * Function: bitonic merge 2x8, move pointers as well when moving keys
 * @in1_k: vector inclues 8 sorted values
 * @in1_ptr: vector of 8 pointers corresponding keys in in1_k
 * @in2_k: vector inclues 8 sorted values
 * @in2_ptr: vector of 8 pointers corresponding keys in in2_k
 * @out1_k: vector of sorted 8 smallest values of in1_k and in2_k
 * @out1_ptr: vector of pointers of corrresponding keys in out1_k
 * @out2_k: vector of sorted 8 largest values of in1_k and in2_k
 * @out2_ptr: vector of pointers of corrresponding keys in out2_k
 * */
void x2_bitonic_merge_avx512_2x8(__m512d& in1_k, __m512d& in1_ptr,  __m512d& in2_k, __m512d& in2_ptr,  __m512d& out1_k, __m512d& out1_ptr, __m512d& out2_k, __m512d& out2_ptr){
	{
		// 1-level comparison
		__m512i idx = _mm512_set_epi64(0, 1, 2, 3, 4, 5, 6, 7);
		
		//__m512d perm_in2 = _mm512_permutexvar_pd(idx, in2);
		__m512d perm_in2_k = _mm512_permutexvar_pd(idx, in2_k); 
		__m512d perm_in2_ptr = _mm512_permutexvar_pd(idx, in2_ptr); 
		
		//out1 = _mm512_min_pd(in1, perm_in2); 		
		//out2 = _mm512_max_pd(in1, perm_in2);
		//x2_cmp_pd(in1_k, in1_ptr, perm_in2_k, perm_in2_ptr, out1_k, out1_ptr, out2_k, out2_ptr);
		x2_cmp_pd_noequal(in1_k, in1_ptr, perm_in2_k, perm_in2_ptr, out1_k, out1_ptr, out2_k, out2_ptr);
	}
	{
		// 2-level comparison
		__m512i idx = _mm512_set_epi64(3, 2, 1, 0, 7, 6, 5, 4);
		
		// out1
		//__m512d tmp1 = _mm512_permutexvar_pd(idx, out1);
		__m512d tmp1_k = _mm512_permutexvar_pd(idx, out1_k);
		__m512d tmp1_ptr = _mm512_permutexvar_pd(idx, out1_ptr);
		
		//__m512d min_1 = _mm512_min_pd(out1, tmp1);
		//__m512d max_1 = _mm512_max_pd(out1, tmp1);
		__m512d min_1_k, min_1_ptr, max_1_k, max_1_ptr;
		x2_cmp_pd(out1_k, out1_ptr, tmp1_k, tmp1_ptr, min_1_k, min_1_ptr, max_1_k, max_1_ptr);
		
		//out1 = _mm512_mask_mov_pd(min_1, 0xF0, max_1);
		out1_k = _mm512_mask_mov_pd(min_1_k, 0xF0, max_1_k);
		out1_ptr = _mm512_mask_mov_pd(min_1_ptr, 0xF0, max_1_ptr);

		// out2
		//__m512d tmp2 = _mm512_permutexvar_pd(idx, out2);
		__m512d tmp2_k = _mm512_permutexvar_pd(idx, out2_k);
		__m512d tmp2_ptr = _mm512_permutexvar_pd(idx, out2_ptr);
		
		//__m512d min_2 = _mm512_min_pd(out2, tmp2);
		//__m512d max_2 = _mm512_max_pd(out2, tmp2);
		__m512d min_2_k, min_2_ptr, max_2_k, max_2_ptr;
		x2_cmp_pd(out2_k, out2_ptr, tmp2_k, tmp2_ptr, min_2_k, min_2_ptr, max_2_k, max_2_ptr);
		
		//out2 = _mm512_mask_mov_pd(min_2, 0xF0, max_2);
		out2_k = _mm512_mask_mov_pd(min_2_k, 0xF0, max_2_k);
		out2_ptr = _mm512_mask_mov_pd(min_2_ptr, 0xF0, max_2_ptr);
	}
	{
		// 3-level comparison
		__m512i idx = _mm512_set_epi64(5, 4, 7, 6, 1, 0, 3, 2);

		//out1
		//__m512d tmp1 = _mm512_permutexvar_pd(idx, out1);
		__m512d tmp1_k = _mm512_permutexvar_pd(idx, out1_k);
		__m512d tmp1_ptr = _mm512_permutexvar_pd(idx, out1_ptr);
		
		//__m512d min_1 = _mm512_min_pd(out1, tmp1);
		//__m512d max_1 = _mm512_max_pd(out1, tmp1);
		__m512d min_1_k, min_1_ptr, max_1_k, max_1_ptr;
		x2_cmp_pd(out1_k, out1_ptr, tmp1_k, tmp1_ptr, min_1_k, min_1_ptr, max_1_k, max_1_ptr);
		
		//out1 = _mm512_mask_mov_pd(min_1, 0xCC, max_1);	
		out1_k = _mm512_mask_mov_pd(min_1_k, 0xCC, max_1_k);
		out1_ptr = _mm512_mask_mov_pd(min_1_ptr, 0xCC, max_1_ptr);

		//out2
		//__m512d tmp2 = _mm512_permutexvar_pd(idx, out2);
		__m512d tmp2_k = _mm512_permutexvar_pd(idx, out2_k);
		__m512d tmp2_ptr = _mm512_permutexvar_pd(idx, out2_ptr);
		
		//__m512d min_2 = _mm512_min_pd(out2, tmp2);
		//__m512d max_2 = _mm512_max_pd(out2, tmp2);
		__m512d min_2_k, min_2_ptr, max_2_k, max_2_ptr;
		x2_cmp_pd(out2_k, out2_ptr, tmp2_k, tmp2_ptr, min_2_k, min_2_ptr, max_2_k, max_2_ptr);
		
		//out2 = _mm512_mask_mov_pd(min_2, 0xCC, max_2);	
		out2_k = _mm512_mask_mov_pd(min_2_k, 0xCC, max_2_k);
		out2_ptr = _mm512_mask_mov_pd(min_2_ptr, 0xCC, max_2_ptr);
	}
	{
		// 4-level comparison
		__m512i idx = _mm512_set_epi64(6, 7, 4, 5, 2, 3, 0, 1);

		// out1
		//__m512d tmp1 = _mm512_permutexvar_pd(idx, out1);
		__m512d tmp1_k = _mm512_permutexvar_pd(idx, out1_k);
		__m512d tmp1_ptr = _mm512_permutexvar_pd(idx, out1_ptr);
		
		//__m512d min_1 = _mm512_min_pd(out1, tmp1);
		//__m512d max_1 = _mm512_max_pd(out1, tmp1);
		__m512d min_1_k, min_1_ptr, max_1_k, max_1_ptr;
		x2_cmp_pd(out1_k, out1_ptr, tmp1_k, tmp1_ptr, min_1_k, min_1_ptr, max_1_k, max_1_ptr);
		
		//out1 = _mm512_mask_mov_pd(min_1, 0xAA, max_1);	
		out1_k = _mm512_mask_mov_pd(min_1_k, 0xAA, max_1_k);
		out1_ptr = _mm512_mask_mov_pd(min_1_ptr, 0xAA, max_1_ptr);

		// out2
		//__m512d tmp2 = _mm512_permutexvar_pd(idx, out2);
		__m512d tmp2_k = _mm512_permutexvar_pd(idx, out2_k);
		__m512d tmp2_ptr = _mm512_permutexvar_pd(idx, out2_ptr);
		
		//__m512d min_2 = _mm512_min_pd(out2, tmp2);
		//__m512d max_2 = _mm512_max_pd(out2, tmp2);
		__m512d min_2_k, min_2_ptr, max_2_k, max_2_ptr;
		x2_cmp_pd(out2_k, out2_ptr, tmp2_k, tmp2_ptr, min_2_k, min_2_ptr, max_2_k, max_2_ptr);
		
		//out2 = _mm512_mask_mov_pd(min_2, 0xAA, max_2);	
		out2_k = _mm512_mask_mov_pd(min_2_k, 0xAA, max_2_k);
		out2_ptr = _mm512_mask_mov_pd(min_2_ptr, 0xAA, max_2_ptr);
	}
}


/*
 * Function: bitonic merge 2x8, move pointers as well when moving keys
 * @in1_k: inclues 8 sorted values
 * @in1_ptr: 8 pointers corresponding keys in in1_k
 * @in2_k: inclues 8 sorted values
 * @in2_ptr: 8 pointers corresponding keys in in2_k
 * @out1_k: sorted 8 smallest values of in1_k and in2_k
 * @out1_ptr: pointers of corrresponding keys in out1_k
 * @out2_k: sorted 8 largest values of in1_k and in2_k
 * @out2_ptr: pointers of corrresponding keys in out2_k
 * Note: a wrapper of void x2_bitonic_merge_avx512_2x8(__m512d& in1_k, __m512d& in1_ptr,  __m512d& in2_k, __m512d& in2_ptr,  __m512d& out1_k, __m512d& out1_ptr, __m512d& out2_k, __m512d& out2_ptr){
 * */
void x2_bitonic_merge_avx512_2x8(int64_t* in1_k, int64_t* in1_ptr, int64_t* in2_k, int64_t* in2_ptr,
			int64_t* out1_k, int64_t* out1_ptr, int64_t* out2_k, int64_t* out2_ptr){
	__m512d input1_k = _mm512_loadu_pd(in1_k);			
	__m512d input1_ptr = _mm512_loadu_pd(in1_ptr);			
	__m512d input2_k = _mm512_loadu_pd(in2_k);			
	__m512d input2_ptr = _mm512_loadu_pd(in2_ptr);			
	__m512d output1_k, output1_ptr, output2_k, output2_ptr;
	
	x2_bitonic_merge_avx512_2x8(input1_k, input1_ptr, input2_k, input2_ptr, 
				output1_k, output1_ptr, output2_k, output2_ptr);
	
	_mm512_storeu_pd(out1_k, output1_k);
	_mm512_storeu_pd(out1_ptr, output1_ptr);
	_mm512_storeu_pd(out2_k, output2_k);
	_mm512_storeu_pd(out2_ptr, output2_ptr);
}


/*
 * Function: bitonic sort 8x8(64) int64_t values. Will get 8 sorted blocks, and each block has 8 values. Move ptr as well when moring k
 * @k: points to 8x8(64) int64_t keys
 * @ptr: points to 8x8(64) int64_t pointers of keys in k
 * */
void x2_bitonic_sort_avx512_8x8(int64_t * k, int64_t * ptr){
	//__m512d r0, r1, r2, r3, r4, r5, r6, r7;
	__m512d r0_k, r1_k, r2_k, r3_k, r4_k, r5_k, r6_k, r7_k;
	__m512d r0_ptr, r1_ptr, r2_ptr, r3_ptr, r4_ptr, r5_ptr, r6_ptr, r7_ptr;
	
	//__m512d t0, t1, t2, t3, t4, t5, t6, t7;
	__m512d t0_k, t1_k, t2_k, t3_k, t4_k, t5_k, t6_k, t7_k;
	__m512d t0_ptr, t1_ptr, t2_ptr, t3_ptr, t4_ptr, t5_ptr, t6_ptr, t7_ptr;
	
	//__m512d m0, m1, m2, m3, m4, m5, m6, m7;
	__m512d m0_k, m1_k, m2_k, m3_k, m4_k, m5_k, m6_k, m7_k;
	__m512d m0_ptr, m1_ptr, m2_ptr, m3_ptr, m4_ptr, m5_ptr, m6_ptr, m7_ptr;
	
	__m512i idx;
	
	// read 64 items to 8 registers
	//r0 = _mm512_loadu_pd ((int64_t const *)(items));
	r0_k = _mm512_loadu_pd ((int64_t const *)(k));
	r0_ptr = _mm512_loadu_pd ((int64_t const *)(ptr));
	
	//r1 = _mm512_loadu_pd ((int64_t const *)(items + 8));
	r1_k = _mm512_loadu_pd ((int64_t const *)(k + 8));
	r1_ptr = _mm512_loadu_pd ((int64_t const *)(ptr + 8));
	
	//r2 = _mm512_loadu_pd ((int64_t const *)(items + 16));
	r2_k = _mm512_loadu_pd ((int64_t const *)(k + 16));
	r2_ptr = _mm512_loadu_pd ((int64_t const *)(ptr + 16));
	
	//r3 = _mm512_loadu_pd ((int64_t const *)(items + 24));
	r3_k = _mm512_loadu_pd ((int64_t const *)(k + 24));
	r3_ptr = _mm512_loadu_pd ((int64_t const *)(ptr + 24));
	
	//r4 = _mm512_loadu_pd ((int64_t const *)(items + 32));
	r4_k = _mm512_loadu_pd ((int64_t const *)(k + 32));
	r4_ptr = _mm512_loadu_pd ((int64_t const *)(ptr + 32));
	
	//r5 = _mm512_loadu_pd ((int64_t const *)(items + 40));
	r5_k = _mm512_loadu_pd ((int64_t const *)(k + 40));
	r5_ptr = _mm512_loadu_pd ((int64_t const *)(ptr + 40));
	
	//r6 = _mm512_loadu_pd ((int64_t const *)(items + 48));
	r6_k = _mm512_loadu_pd ((int64_t const *)(k + 48));
	r6_ptr = _mm512_loadu_pd ((int64_t const *)(ptr + 48));
	
	//r7 = _mm512_loadu_pd ((int64_t const *)(items + 56));
	r7_k = _mm512_loadu_pd ((int64_t const *)(k + 56));
	r7_ptr = _mm512_loadu_pd ((int64_t const *)(ptr + 56));


	// 1st level of comparison
	//t0 = _mm512_min_pd(r0, r1);
	//t1 = _mm512_max_pd(r0, r1);
	x2_cmp_pd_noequal(r0_k, r0_ptr, r1_k, r1_ptr, t0_k, t0_ptr, t1_k, t1_ptr);
	
	//t2 = _mm512_min_pd(r2, r3);
	//t3 = _mm512_max_pd(r2, r3);
	x2_cmp_pd_noequal(r2_k, r2_ptr, r3_k, r3_ptr, t2_k, t2_ptr, t3_k, t3_ptr);
	
	//t4 = _mm512_min_pd(r4, r5);
	//t5 = _mm512_max_pd(r4, r5);
	x2_cmp_pd_noequal(r4_k, r4_ptr, r5_k, r5_ptr, t4_k, t4_ptr, t5_k, t5_ptr);
	
	//t6 = _mm512_min_pd(r6, r7);
	//t7 = _mm512_max_pd(r6, r7);
	x2_cmp_pd_noequal(r6_k, r6_ptr, r7_k, r7_ptr, t6_k, t6_ptr, t7_k, t7_ptr);


	// 2nd level of comparison
	//r0 = _mm512_min_pd(t0, t3);
	//r3 = _mm512_max_pd(t0, t3);
	x2_cmp_pd_noequal(t0_k, t0_ptr, t3_k, t3_ptr, r0_k, r0_ptr, r3_k, r3_ptr);
	
	//r1 = _mm512_min_pd(t1, t2);
	//r2 = _mm512_max_pd(t1, t2);
	x2_cmp_pd_noequal(t1_k, t1_ptr, t2_k, t2_ptr, r1_k, r1_ptr, r2_k, r2_ptr);
	
	//r4 = _mm512_min_pd(t4, t7);
	//r7 = _mm512_max_pd(t4, t7);
	x2_cmp_pd_noequal(t4_k, t4_ptr, t7_k, t7_ptr, r4_k, r4_ptr, r7_k, r7_ptr);

	//r5 = _mm512_min_pd(t5, t6);
	//r6 = _mm512_max_pd(t5, t6);
	x2_cmp_pd_noequal(t5_k, t5_ptr, t6_k, t6_ptr, r5_k, r5_ptr, r6_k, r6_ptr);

	
	// 3rd level of comparison
	//t0 = _mm512_min_pd(r0, r1);
	//t1 = _mm512_max_pd(r0, r1);
	x2_cmp_pd_noequal(r0_k, r0_ptr, r1_k, r1_ptr, t0_k, t0_ptr, t1_k, t1_ptr);

	//t2 = _mm512_min_pd(r2, r3);
	//t3 = _mm512_max_pd(r2, r3);
	x2_cmp_pd_noequal(r2_k, r2_ptr, r3_k, r3_ptr, t2_k, t2_ptr, t3_k, t3_ptr);
	
	//t4 = _mm512_min_pd(r4, r5);
	//t5 = _mm512_max_pd(r4, r5);
	x2_cmp_pd_noequal(r4_k, r4_ptr, r5_k, r5_ptr, t4_k, t4_ptr, t5_k, t5_ptr);

	//t6 = _mm512_min_pd(r6, r7);
	//t7 = _mm512_max_pd(r6, r7);
	x2_cmp_pd_noequal(r6_k, r6_ptr, r7_k, r7_ptr, t6_k, t6_ptr, t7_k, t7_ptr);


	// 4th level of comparision
	//r0 = _mm512_min_pd(t0, t7);
	//r7 = _mm512_max_pd(t0, t7);
	x2_cmp_pd_noequal(t0_k, t0_ptr, t7_k, t7_ptr, r0_k, r0_ptr, r7_k, r7_ptr);
	
	//r1 = _mm512_min_pd(t1, t6);
	//r6 = _mm512_max_pd(t1, t6);
	x2_cmp_pd_noequal(t1_k, t1_ptr, t6_k, t6_ptr, r1_k, r1_ptr, r6_k, r6_ptr);
	
	//r2 = _mm512_min_pd(t2, t5);
	//r5 = _mm512_max_pd(t2, t5);
	x2_cmp_pd_noequal(t2_k, t2_ptr, t5_k, t5_ptr, r2_k, r2_ptr, r5_k, r5_ptr);
	
	//r3 = _mm512_min_pd(t3, t4);
	//r4 = _mm512_max_pd(t3, t4);
	x2_cmp_pd_noequal(t3_k, t3_ptr, t4_k, t4_ptr, r3_k, r3_ptr, r4_k, r4_ptr);


	// 5th level of comparison
	//t1 = _mm512_min_pd(r1, r3);
	//t3 = _mm512_max_pd(r1, r3);
	x2_cmp_pd_noequal(r1_k, r1_ptr, r3_k, r3_ptr, t1_k, t1_ptr, t3_k, t3_ptr);
	
	//t0 = _mm512_min_pd(r0, r2);
	//t2 = _mm512_max_pd(r0, r2);
	x2_cmp_pd_noequal(r0_k, r0_ptr, r2_k, r2_ptr, t0_k, t0_ptr, t2_k, t2_ptr);

	//t5 = _mm512_min_pd(r5, r7);
	//t7 = _mm512_max_pd(r5, r7);
	x2_cmp_pd_noequal(r5_k, r5_ptr, r7_k, r7_ptr, t5_k, t5_ptr, t7_k, t7_ptr);
	
	//t4 = _mm512_min_pd(r4, r6);
	//t6 = _mm512_max_pd(r4, r6);
	x2_cmp_pd_noequal(r4_k, r4_ptr, r6_k, r6_ptr, t4_k, t4_ptr, t6_k, t6_ptr);


	// 6th level of comparison
	//r0 = _mm512_min_pd(t0, t1);
	//r1 = _mm512_max_pd(t0, t1);
	x2_cmp_pd_noequal(t0_k, t0_ptr, t1_k, t1_ptr, r0_k, r0_ptr, r1_k, r1_ptr);
	
	//r2 = _mm512_min_pd(t2, t3);
	//r3 = _mm512_max_pd(t2, t3);
	x2_cmp_pd_noequal(t2_k, t2_ptr, t3_k, t3_ptr, r2_k, r2_ptr, r3_k, r3_ptr);
	
	//r4 = _mm512_min_pd(t4, t5);
	//r5 = _mm512_max_pd(t4, t5);
	x2_cmp_pd_noequal(t4_k, t4_ptr, t5_k, t5_ptr, r4_k, r4_ptr, r5_k, r5_ptr);

	//r6 = _mm512_min_pd(t6, t7);
	//r7 = _mm512_max_pd(t6, t7);
	x2_cmp_pd_noequal(t6_k, t6_ptr, t7_k, t7_ptr, r6_k, r6_ptr, r7_k, r7_ptr);


	//transpose 8x8 matrix
	idx = _mm512_set_epi64(5, 7, 4, 6, 1, 3, 0, 2);	
	//t0 = _mm512_permutexvar_pd(idx, r0);
	t0_k = _mm512_permutexvar_pd(idx, r0_k);
	t0_ptr = _mm512_permutexvar_pd(idx, r0_ptr);
	
	//t1 = _mm512_permutexvar_pd(idx, r1);
	t1_k = _mm512_permutexvar_pd(idx, r1_k);
	t1_ptr = _mm512_permutexvar_pd(idx, r1_ptr);
	
	//t2 = _mm512_permutexvar_pd(idx, r2);
	t2_k = _mm512_permutexvar_pd(idx, r2_k);
	t2_ptr = _mm512_permutexvar_pd(idx, r2_ptr);
	
	//t3 = _mm512_permutexvar_pd(idx, r3);
	t3_k = _mm512_permutexvar_pd(idx, r3_k);
	t3_ptr = _mm512_permutexvar_pd(idx, r3_ptr);
	
	//t4 = _mm512_permutexvar_pd(idx, r4);
	t4_k = _mm512_permutexvar_pd(idx, r4_k);
	t4_ptr = _mm512_permutexvar_pd(idx, r4_ptr);

	//t5 = _mm512_permutexvar_pd(idx, r5);
	t5_k = _mm512_permutexvar_pd(idx, r5_k);
	t5_ptr = _mm512_permutexvar_pd(idx, r5_ptr);
	
	//t6 = _mm512_permutexvar_pd(idx, r6);
	t6_k = _mm512_permutexvar_pd(idx, r6_k);
	t6_ptr = _mm512_permutexvar_pd(idx, r6_ptr);
	
	//t7 = _mm512_permutexvar_pd(idx, r7);
	t7_k = _mm512_permutexvar_pd(idx, r7_k);
	t7_ptr = _mm512_permutexvar_pd(idx, r7_ptr);


	//r0 = _mm512_unpackhi_pd(t0, t1);
	r0_k = _mm512_unpackhi_pd(t0_k, t1_k);
	r0_ptr = _mm512_unpackhi_pd(t0_ptr, t1_ptr);
	
	//r1 = _mm512_unpacklo_pd(t0, t1);
	r1_k = _mm512_unpacklo_pd(t0_k, t1_k);
	r1_ptr = _mm512_unpacklo_pd(t0_ptr, t1_ptr);
	
	//r2 = _mm512_unpackhi_pd(t2, t3);
	r2_k = _mm512_unpackhi_pd(t2_k, t3_k);
	r2_ptr = _mm512_unpackhi_pd(t2_ptr, t3_ptr);
	
	//r3 = _mm512_unpacklo_pd(t2, t3);
	r3_k = _mm512_unpacklo_pd(t2_k, t3_k);
	r3_ptr = _mm512_unpacklo_pd(t2_ptr, t3_ptr);
	
	//r4 = _mm512_unpackhi_pd(t4, t5);
	r4_k = _mm512_unpackhi_pd(t4_k, t5_k);
	r4_ptr = _mm512_unpackhi_pd(t4_ptr, t5_ptr);
	
	//r5 = _mm512_unpacklo_pd(t4, t5);
	r5_k = _mm512_unpacklo_pd(t4_k, t5_k);
	r5_ptr = _mm512_unpacklo_pd(t4_ptr, t5_ptr);
	
	//r6 = _mm512_unpackhi_pd(t6, t7);
	r6_k = _mm512_unpackhi_pd(t6_k, t7_k);
	r6_ptr = _mm512_unpackhi_pd(t6_ptr, t7_ptr);
	
	//r7 = _mm512_unpacklo_pd(t6, t7);
	r7_k = _mm512_unpacklo_pd(t6_k, t7_k);
	r7_ptr = _mm512_unpacklo_pd(t6_ptr, t7_ptr);


	idx = _mm512_set_epi64(7, 6, 3, 2, 5, 4, 1, 0);
	//t0 = _mm512_shuffle_f64x2(r0, r2, _MM_SHUFFLE(2,0,2,0));	
	t0_k = _mm512_shuffle_f64x2(r0_k, r2_k, _MM_SHUFFLE(2,0,2,0));	
	t0_ptr = _mm512_shuffle_f64x2(r0_ptr, r2_ptr, _MM_SHUFFLE(2,0,2,0));	
	//m0 = _mm512_permutexvar_pd(idx, t0);  
	m0_k = _mm512_permutexvar_pd(idx, t0_k);  
	m0_ptr = _mm512_permutexvar_pd(idx, t0_ptr);  
	
	//t1 = _mm512_shuffle_f64x2(r0, r2, _MM_SHUFFLE(3,1,3,1));	
	t1_k = _mm512_shuffle_f64x2(r0_k, r2_k, _MM_SHUFFLE(3,1,3,1));	
	t1_ptr = _mm512_shuffle_f64x2(r0_ptr, r2_ptr, _MM_SHUFFLE(3,1,3,1));	
	//m1 = _mm512_permutexvar_pd(idx, t1);  
	m1_k = _mm512_permutexvar_pd(idx, t1_k);  
	m1_ptr = _mm512_permutexvar_pd(idx, t1_ptr);  
	
	//t2 = _mm512_shuffle_f64x2(r1, r3, _MM_SHUFFLE(2,0,2,0));	
	t2_k = _mm512_shuffle_f64x2(r1_k, r3_k, _MM_SHUFFLE(2,0,2,0));	
	t2_ptr = _mm512_shuffle_f64x2(r1_ptr, r3_ptr, _MM_SHUFFLE(2,0,2,0));	
	//m2 = _mm512_permutexvar_pd(idx, t2); 
	m2_k = _mm512_permutexvar_pd(idx, t2_k); 
	m2_ptr = _mm512_permutexvar_pd(idx, t2_ptr); 

	
	//t3 = _mm512_shuffle_f64x2(r1, r3, _MM_SHUFFLE(3,1,3,1));	
	t3_k = _mm512_shuffle_f64x2(r1_k, r3_k, _MM_SHUFFLE(3,1,3,1));	
	t3_ptr = _mm512_shuffle_f64x2(r1_ptr, r3_ptr, _MM_SHUFFLE(3,1,3,1));	
	//m3 = _mm512_permutexvar_pd(idx, t3);  
	m3_k = _mm512_permutexvar_pd(idx, t3_k);  
	m3_ptr = _mm512_permutexvar_pd(idx, t3_ptr);  
	
	//t4 = _mm512_shuffle_f64x2(r4, r6, _MM_SHUFFLE(2,0,2,0));	
	t4_k = _mm512_shuffle_f64x2(r4_k, r6_k, _MM_SHUFFLE(2,0,2,0));	
	t4_ptr = _mm512_shuffle_f64x2(r4_ptr, r6_ptr, _MM_SHUFFLE(2,0,2,0));	
	//m4 = _mm512_permutexvar_pd(idx, t4);  
	m4_k = _mm512_permutexvar_pd(idx, t4_k);  
	m4_ptr = _mm512_permutexvar_pd(idx, t4_ptr);  
	
	//t5 = _mm512_shuffle_f64x2(r4, r6, _MM_SHUFFLE(3,1,3,1));
	t5_k = _mm512_shuffle_f64x2(r4_k, r6_k, _MM_SHUFFLE(3,1,3,1));
	t5_ptr = _mm512_shuffle_f64x2(r4_ptr, r6_ptr, _MM_SHUFFLE(3,1,3,1));
	//m5 = _mm512_permutexvar_pd(idx, t5);  
	m5_k = _mm512_permutexvar_pd(idx, t5_k);  
	m5_ptr = _mm512_permutexvar_pd(idx, t5_ptr);  
	
	//t6 = _mm512_shuffle_f64x2(r5, r7, _MM_SHUFFLE(2,0,2,0));
	t6_k = _mm512_shuffle_f64x2(r5_k, r7_k, _MM_SHUFFLE(2,0,2,0));
	t6_ptr = _mm512_shuffle_f64x2(r5_ptr, r7_ptr, _MM_SHUFFLE(2,0,2,0));
	//m6 = _mm512_permutexvar_pd(idx, t6);  
	m6_k = _mm512_permutexvar_pd(idx, t6_k);  
	m6_ptr = _mm512_permutexvar_pd(idx, t6_ptr);  
	
	//t7 = _mm512_shuffle_f64x2(r5, r7, _MM_SHUFFLE(3,1,3,1));	
	t7_k = _mm512_shuffle_f64x2(r5_k, r7_k, _MM_SHUFFLE(3,1,3,1));	
	t7_ptr = _mm512_shuffle_f64x2(r5_ptr, r7_ptr, _MM_SHUFFLE(3,1,3,1));	
	//m7 = _mm512_permutexvar_pd(idx, t7);  
	m7_k = _mm512_permutexvar_pd(idx, t7_k);  
	m7_ptr = _mm512_permutexvar_pd(idx, t7_ptr);  
	
	
	idx = _mm512_set_epi64(11, 10, 9, 8, 3, 2, 1, 0);
  	//t0 = _mm512_permutex2var_pd(m0, idx, m4);	
  	t0_k = _mm512_permutex2var_pd(m0_k, idx, m4_k);	
  	t0_ptr = _mm512_permutex2var_pd(m0_ptr, idx, m4_ptr);	
  	
	//t1 = _mm512_permutex2var_pd(m1, idx, m5);	
	t1_k = _mm512_permutex2var_pd(m1_k, idx, m5_k);	
	t1_ptr = _mm512_permutex2var_pd(m1_ptr, idx, m5_ptr);	
  	
	//t2 = _mm512_permutex2var_pd(m2, idx, m6);	
	t2_k = _mm512_permutex2var_pd(m2_k, idx, m6_k);	
	t2_ptr = _mm512_permutex2var_pd(m2_ptr, idx, m6_ptr);	
  	
	//t3 = _mm512_permutex2var_pd(m3, idx, m7);	
	t3_k = _mm512_permutex2var_pd(m3_k, idx, m7_k);	
	t3_ptr = _mm512_permutex2var_pd(m3_ptr, idx, m7_ptr);	


	idx = _mm512_set_epi64(15, 14, 13, 12, 7, 6, 5, 4);
  	//t4 = _mm512_permutex2var_pd(m0, idx, m4);	
  	t4_k = _mm512_permutex2var_pd(m0_k, idx, m4_k);	
  	t4_ptr = _mm512_permutex2var_pd(m0_ptr, idx, m4_ptr);	
  	
	//t5 = _mm512_permutex2var_pd(m1, idx, m5);	
	t5_k = _mm512_permutex2var_pd(m1_k, idx, m5_k);	
	t5_ptr = _mm512_permutex2var_pd(m1_ptr, idx, m5_ptr);	
  	
	//t6 = _mm512_permutex2var_pd(m2, idx, m6);	
	t6_k = _mm512_permutex2var_pd(m2_k, idx, m6_k);	
	t6_ptr = _mm512_permutex2var_pd(m2_ptr, idx, m6_ptr);	
  	
	//t7 = _mm512_permutex2var_pd(m3, idx, m7);	
	t7_k = _mm512_permutex2var_pd(m3_k, idx, m7_k);	
	t7_ptr = _mm512_permutex2var_pd(m3_ptr, idx, m7_ptr);	


	// store to output
	//_mm512_storeu_pd ((int64_t *) output, t0);
	_mm512_storeu_pd ((int64_t *) k, t0_k);
	_mm512_storeu_pd ((int64_t *) ptr, t0_ptr);
	
	//_mm512_storeu_pd ((int64_t *) (output + 8), t1);
	_mm512_storeu_pd ((int64_t *) (k + 8), t1_k);
	_mm512_storeu_pd ((int64_t *) (ptr + 8), t1_ptr);
	
	//_mm512_storeu_pd ((int64_t *) (output + 16), t2);
	_mm512_storeu_pd ((int64_t *) (k + 16), t2_k);
	_mm512_storeu_pd ((int64_t *) (ptr + 16), t2_ptr);
	
	//_mm512_storeu_pd ((int64_t *) (output + 24), t3);
	_mm512_storeu_pd ((int64_t *) (k + 24), t3_k);
	_mm512_storeu_pd ((int64_t *) (ptr + 24), t3_ptr);
	
	//_mm512_storeu_pd ((int64_t *) (output + 32), t4);
	_mm512_storeu_pd ((int64_t *) (k + 32), t4_k);
	_mm512_storeu_pd ((int64_t *) (ptr + 32), t4_ptr);
	
	//_mm512_storeu_pd ((int64_t *) (output + 40), t5);
	_mm512_storeu_pd ((int64_t *) (k + 40), t5_k);
	_mm512_storeu_pd ((int64_t *) (ptr + 40), t5_ptr);
	
	//_mm512_storeu_pd ((int64_t *) (output + 48), t6);
	_mm512_storeu_pd ((int64_t *) (k + 48), t6_k);
	_mm512_storeu_pd ((int64_t *) (ptr + 48), t6_ptr);
	
	//_mm512_storeu_pd ((int64_t *) (output + 56), t7);
	_mm512_storeu_pd ((int64_t *) (k + 56), t7_k);
	_mm512_storeu_pd ((int64_t *) (ptr + 56), t7_ptr);
}

/* store to out_k/out_ptr, instead of input k/ptr*/
void x2_bitonic_sort_avx512_8x8(int64_t * k, int64_t * ptr, int64_t * out_k, int64_t * out_ptr){
	//__m512d r0, r1, r2, r3, r4, r5, r6, r7;
	__m512d r0_k, r1_k, r2_k, r3_k, r4_k, r5_k, r6_k, r7_k;
	__m512d r0_ptr, r1_ptr, r2_ptr, r3_ptr, r4_ptr, r5_ptr, r6_ptr, r7_ptr;
	
	//__m512d t0, t1, t2, t3, t4, t5, t6, t7;
	__m512d t0_k, t1_k, t2_k, t3_k, t4_k, t5_k, t6_k, t7_k;
	__m512d t0_ptr, t1_ptr, t2_ptr, t3_ptr, t4_ptr, t5_ptr, t6_ptr, t7_ptr;
	
	//__m512d m0, m1, m2, m3, m4, m5, m6, m7;
	__m512d m0_k, m1_k, m2_k, m3_k, m4_k, m5_k, m6_k, m7_k;
	__m512d m0_ptr, m1_ptr, m2_ptr, m3_ptr, m4_ptr, m5_ptr, m6_ptr, m7_ptr;
	
	__m512i idx;
	
	// read 64 items to 8 registers
	//r0 = _mm512_loadu_pd ((int64_t const *)(items));
	r0_k = _mm512_loadu_pd ((int64_t const *)(k));
	r0_ptr = _mm512_loadu_pd ((int64_t const *)(ptr));
	
	//r1 = _mm512_loadu_pd ((int64_t const *)(items + 8));
	r1_k = _mm512_loadu_pd ((int64_t const *)(k + 8));
	r1_ptr = _mm512_loadu_pd ((int64_t const *)(ptr + 8));
	
	//r2 = _mm512_loadu_pd ((int64_t const *)(items + 16));
	r2_k = _mm512_loadu_pd ((int64_t const *)(k + 16));
	r2_ptr = _mm512_loadu_pd ((int64_t const *)(ptr + 16));
	
	//r3 = _mm512_loadu_pd ((int64_t const *)(items + 24));
	r3_k = _mm512_loadu_pd ((int64_t const *)(k + 24));
	r3_ptr = _mm512_loadu_pd ((int64_t const *)(ptr + 24));
	
	//r4 = _mm512_loadu_pd ((int64_t const *)(items + 32));
	r4_k = _mm512_loadu_pd ((int64_t const *)(k + 32));
	r4_ptr = _mm512_loadu_pd ((int64_t const *)(ptr + 32));
	
	//r5 = _mm512_loadu_pd ((int64_t const *)(items + 40));
	r5_k = _mm512_loadu_pd ((int64_t const *)(k + 40));
	r5_ptr = _mm512_loadu_pd ((int64_t const *)(ptr + 40));
	
	//r6 = _mm512_loadu_pd ((int64_t const *)(items + 48));
	r6_k = _mm512_loadu_pd ((int64_t const *)(k + 48));
	r6_ptr = _mm512_loadu_pd ((int64_t const *)(ptr + 48));
	
	//r7 = _mm512_loadu_pd ((int64_t const *)(items + 56));
	r7_k = _mm512_loadu_pd ((int64_t const *)(k + 56));
	r7_ptr = _mm512_loadu_pd ((int64_t const *)(ptr + 56));


	// 1st level of comparison
	//t0 = _mm512_min_pd(r0, r1);
	//t1 = _mm512_max_pd(r0, r1);
	x2_cmp_pd_noequal(r0_k, r0_ptr, r1_k, r1_ptr, t0_k, t0_ptr, t1_k, t1_ptr);
	
	//t2 = _mm512_min_pd(r2, r3);
	//t3 = _mm512_max_pd(r2, r3);
	x2_cmp_pd_noequal(r2_k, r2_ptr, r3_k, r3_ptr, t2_k, t2_ptr, t3_k, t3_ptr);
	
	//t4 = _mm512_min_pd(r4, r5);
	//t5 = _mm512_max_pd(r4, r5);
	x2_cmp_pd_noequal(r4_k, r4_ptr, r5_k, r5_ptr, t4_k, t4_ptr, t5_k, t5_ptr);
	
	//t6 = _mm512_min_pd(r6, r7);
	//t7 = _mm512_max_pd(r6, r7);
	x2_cmp_pd_noequal(r6_k, r6_ptr, r7_k, r7_ptr, t6_k, t6_ptr, t7_k, t7_ptr);


	// 2nd level of comparison
	//r0 = _mm512_min_pd(t0, t3);
	//r3 = _mm512_max_pd(t0, t3);
	x2_cmp_pd_noequal(t0_k, t0_ptr, t3_k, t3_ptr, r0_k, r0_ptr, r3_k, r3_ptr);
	
	//r1 = _mm512_min_pd(t1, t2);
	//r2 = _mm512_max_pd(t1, t2);
	x2_cmp_pd_noequal(t1_k, t1_ptr, t2_k, t2_ptr, r1_k, r1_ptr, r2_k, r2_ptr);
	
	//r4 = _mm512_min_pd(t4, t7);
	//r7 = _mm512_max_pd(t4, t7);
	x2_cmp_pd_noequal(t4_k, t4_ptr, t7_k, t7_ptr, r4_k, r4_ptr, r7_k, r7_ptr);

	//r5 = _mm512_min_pd(t5, t6);
	//r6 = _mm512_max_pd(t5, t6);
	x2_cmp_pd_noequal(t5_k, t5_ptr, t6_k, t6_ptr, r5_k, r5_ptr, r6_k, r6_ptr);

	
	// 3rd level of comparison
	//t0 = _mm512_min_pd(r0, r1);
	//t1 = _mm512_max_pd(r0, r1);
	x2_cmp_pd_noequal(r0_k, r0_ptr, r1_k, r1_ptr, t0_k, t0_ptr, t1_k, t1_ptr);

	//t2 = _mm512_min_pd(r2, r3);
	//t3 = _mm512_max_pd(r2, r3);
	x2_cmp_pd_noequal(r2_k, r2_ptr, r3_k, r3_ptr, t2_k, t2_ptr, t3_k, t3_ptr);
	
	//t4 = _mm512_min_pd(r4, r5);
	//t5 = _mm512_max_pd(r4, r5);
	x2_cmp_pd_noequal(r4_k, r4_ptr, r5_k, r5_ptr, t4_k, t4_ptr, t5_k, t5_ptr);

	//t6 = _mm512_min_pd(r6, r7);
	//t7 = _mm512_max_pd(r6, r7);
	x2_cmp_pd_noequal(r6_k, r6_ptr, r7_k, r7_ptr, t6_k, t6_ptr, t7_k, t7_ptr);


	// 4th level of comparision
	//r0 = _mm512_min_pd(t0, t7);
	//r7 = _mm512_max_pd(t0, t7);
	x2_cmp_pd_noequal(t0_k, t0_ptr, t7_k, t7_ptr, r0_k, r0_ptr, r7_k, r7_ptr);
	
	//r1 = _mm512_min_pd(t1, t6);
	//r6 = _mm512_max_pd(t1, t6);
	x2_cmp_pd_noequal(t1_k, t1_ptr, t6_k, t6_ptr, r1_k, r1_ptr, r6_k, r6_ptr);
	
	//r2 = _mm512_min_pd(t2, t5);
	//r5 = _mm512_max_pd(t2, t5);
	x2_cmp_pd_noequal(t2_k, t2_ptr, t5_k, t5_ptr, r2_k, r2_ptr, r5_k, r5_ptr);
	
	//r3 = _mm512_min_pd(t3, t4);
	//r4 = _mm512_max_pd(t3, t4);
	x2_cmp_pd_noequal(t3_k, t3_ptr, t4_k, t4_ptr, r3_k, r3_ptr, r4_k, r4_ptr);


	// 5th level of comparison
	//t1 = _mm512_min_pd(r1, r3);
	//t3 = _mm512_max_pd(r1, r3);
	x2_cmp_pd_noequal(r1_k, r1_ptr, r3_k, r3_ptr, t1_k, t1_ptr, t3_k, t3_ptr);
	
	//t0 = _mm512_min_pd(r0, r2);
	//t2 = _mm512_max_pd(r0, r2);
	x2_cmp_pd_noequal(r0_k, r0_ptr, r2_k, r2_ptr, t0_k, t0_ptr, t2_k, t2_ptr);

	//t5 = _mm512_min_pd(r5, r7);
	//t7 = _mm512_max_pd(r5, r7);
	x2_cmp_pd_noequal(r5_k, r5_ptr, r7_k, r7_ptr, t5_k, t5_ptr, t7_k, t7_ptr);
	
	//t4 = _mm512_min_pd(r4, r6);
	//t6 = _mm512_max_pd(r4, r6);
	x2_cmp_pd_noequal(r4_k, r4_ptr, r6_k, r6_ptr, t4_k, t4_ptr, t6_k, t6_ptr);


	// 6th level of comparison
	//r0 = _mm512_min_pd(t0, t1);
	//r1 = _mm512_max_pd(t0, t1);
	x2_cmp_pd_noequal(t0_k, t0_ptr, t1_k, t1_ptr, r0_k, r0_ptr, r1_k, r1_ptr);
	
	//r2 = _mm512_min_pd(t2, t3);
	//r3 = _mm512_max_pd(t2, t3);
	x2_cmp_pd_noequal(t2_k, t2_ptr, t3_k, t3_ptr, r2_k, r2_ptr, r3_k, r3_ptr);
	
	//r4 = _mm512_min_pd(t4, t5);
	//r5 = _mm512_max_pd(t4, t5);
	x2_cmp_pd_noequal(t4_k, t4_ptr, t5_k, t5_ptr, r4_k, r4_ptr, r5_k, r5_ptr);

	//r6 = _mm512_min_pd(t6, t7);
	//r7 = _mm512_max_pd(t6, t7);
	x2_cmp_pd_noequal(t6_k, t6_ptr, t7_k, t7_ptr, r6_k, r6_ptr, r7_k, r7_ptr);


	//transpose 8x8 matrix
	idx = _mm512_set_epi64(5, 7, 4, 6, 1, 3, 0, 2);	
	//t0 = _mm512_permutexvar_pd(idx, r0);
	t0_k = _mm512_permutexvar_pd(idx, r0_k);
	t0_ptr = _mm512_permutexvar_pd(idx, r0_ptr);
	
	//t1 = _mm512_permutexvar_pd(idx, r1);
	t1_k = _mm512_permutexvar_pd(idx, r1_k);
	t1_ptr = _mm512_permutexvar_pd(idx, r1_ptr);
	
	//t2 = _mm512_permutexvar_pd(idx, r2);
	t2_k = _mm512_permutexvar_pd(idx, r2_k);
	t2_ptr = _mm512_permutexvar_pd(idx, r2_ptr);
	
	//t3 = _mm512_permutexvar_pd(idx, r3);
	t3_k = _mm512_permutexvar_pd(idx, r3_k);
	t3_ptr = _mm512_permutexvar_pd(idx, r3_ptr);
	
	//t4 = _mm512_permutexvar_pd(idx, r4);
	t4_k = _mm512_permutexvar_pd(idx, r4_k);
	t4_ptr = _mm512_permutexvar_pd(idx, r4_ptr);

	//t5 = _mm512_permutexvar_pd(idx, r5);
	t5_k = _mm512_permutexvar_pd(idx, r5_k);
	t5_ptr = _mm512_permutexvar_pd(idx, r5_ptr);
	
	//t6 = _mm512_permutexvar_pd(idx, r6);
	t6_k = _mm512_permutexvar_pd(idx, r6_k);
	t6_ptr = _mm512_permutexvar_pd(idx, r6_ptr);
	
	//t7 = _mm512_permutexvar_pd(idx, r7);
	t7_k = _mm512_permutexvar_pd(idx, r7_k);
	t7_ptr = _mm512_permutexvar_pd(idx, r7_ptr);


	//r0 = _mm512_unpackhi_pd(t0, t1);
	r0_k = _mm512_unpackhi_pd(t0_k, t1_k);
	r0_ptr = _mm512_unpackhi_pd(t0_ptr, t1_ptr);
	
	//r1 = _mm512_unpacklo_pd(t0, t1);
	r1_k = _mm512_unpacklo_pd(t0_k, t1_k);
	r1_ptr = _mm512_unpacklo_pd(t0_ptr, t1_ptr);
	
	//r2 = _mm512_unpackhi_pd(t2, t3);
	r2_k = _mm512_unpackhi_pd(t2_k, t3_k);
	r2_ptr = _mm512_unpackhi_pd(t2_ptr, t3_ptr);
	
	//r3 = _mm512_unpacklo_pd(t2, t3);
	r3_k = _mm512_unpacklo_pd(t2_k, t3_k);
	r3_ptr = _mm512_unpacklo_pd(t2_ptr, t3_ptr);
	
	//r4 = _mm512_unpackhi_pd(t4, t5);
	r4_k = _mm512_unpackhi_pd(t4_k, t5_k);
	r4_ptr = _mm512_unpackhi_pd(t4_ptr, t5_ptr);
	
	//r5 = _mm512_unpacklo_pd(t4, t5);
	r5_k = _mm512_unpacklo_pd(t4_k, t5_k);
	r5_ptr = _mm512_unpacklo_pd(t4_ptr, t5_ptr);
	
	//r6 = _mm512_unpackhi_pd(t6, t7);
	r6_k = _mm512_unpackhi_pd(t6_k, t7_k);
	r6_ptr = _mm512_unpackhi_pd(t6_ptr, t7_ptr);
	
	//r7 = _mm512_unpacklo_pd(t6, t7);
	r7_k = _mm512_unpacklo_pd(t6_k, t7_k);
	r7_ptr = _mm512_unpacklo_pd(t6_ptr, t7_ptr);


	idx = _mm512_set_epi64(7, 6, 3, 2, 5, 4, 1, 0);
	//t0 = _mm512_shuffle_f64x2(r0, r2, _MM_SHUFFLE(2,0,2,0));	
	t0_k = _mm512_shuffle_f64x2(r0_k, r2_k, _MM_SHUFFLE(2,0,2,0));	
	t0_ptr = _mm512_shuffle_f64x2(r0_ptr, r2_ptr, _MM_SHUFFLE(2,0,2,0));	
	//m0 = _mm512_permutexvar_pd(idx, t0);  
	m0_k = _mm512_permutexvar_pd(idx, t0_k);  
	m0_ptr = _mm512_permutexvar_pd(idx, t0_ptr);  
	
	//t1 = _mm512_shuffle_f64x2(r0, r2, _MM_SHUFFLE(3,1,3,1));	
	t1_k = _mm512_shuffle_f64x2(r0_k, r2_k, _MM_SHUFFLE(3,1,3,1));	
	t1_ptr = _mm512_shuffle_f64x2(r0_ptr, r2_ptr, _MM_SHUFFLE(3,1,3,1));	
	//m1 = _mm512_permutexvar_pd(idx, t1);  
	m1_k = _mm512_permutexvar_pd(idx, t1_k);  
	m1_ptr = _mm512_permutexvar_pd(idx, t1_ptr);  
	
	//t2 = _mm512_shuffle_f64x2(r1, r3, _MM_SHUFFLE(2,0,2,0));	
	t2_k = _mm512_shuffle_f64x2(r1_k, r3_k, _MM_SHUFFLE(2,0,2,0));	
	t2_ptr = _mm512_shuffle_f64x2(r1_ptr, r3_ptr, _MM_SHUFFLE(2,0,2,0));	
	//m2 = _mm512_permutexvar_pd(idx, t2); 
	m2_k = _mm512_permutexvar_pd(idx, t2_k); 
	m2_ptr = _mm512_permutexvar_pd(idx, t2_ptr); 

	
	//t3 = _mm512_shuffle_f64x2(r1, r3, _MM_SHUFFLE(3,1,3,1));	
	t3_k = _mm512_shuffle_f64x2(r1_k, r3_k, _MM_SHUFFLE(3,1,3,1));	
	t3_ptr = _mm512_shuffle_f64x2(r1_ptr, r3_ptr, _MM_SHUFFLE(3,1,3,1));	
	//m3 = _mm512_permutexvar_pd(idx, t3);  
	m3_k = _mm512_permutexvar_pd(idx, t3_k);  
	m3_ptr = _mm512_permutexvar_pd(idx, t3_ptr);  
	
	//t4 = _mm512_shuffle_f64x2(r4, r6, _MM_SHUFFLE(2,0,2,0));	
	t4_k = _mm512_shuffle_f64x2(r4_k, r6_k, _MM_SHUFFLE(2,0,2,0));	
	t4_ptr = _mm512_shuffle_f64x2(r4_ptr, r6_ptr, _MM_SHUFFLE(2,0,2,0));	
	//m4 = _mm512_permutexvar_pd(idx, t4);  
	m4_k = _mm512_permutexvar_pd(idx, t4_k);  
	m4_ptr = _mm512_permutexvar_pd(idx, t4_ptr);  
	
	//t5 = _mm512_shuffle_f64x2(r4, r6, _MM_SHUFFLE(3,1,3,1));
	t5_k = _mm512_shuffle_f64x2(r4_k, r6_k, _MM_SHUFFLE(3,1,3,1));
	t5_ptr = _mm512_shuffle_f64x2(r4_ptr, r6_ptr, _MM_SHUFFLE(3,1,3,1));
	//m5 = _mm512_permutexvar_pd(idx, t5);  
	m5_k = _mm512_permutexvar_pd(idx, t5_k);  
	m5_ptr = _mm512_permutexvar_pd(idx, t5_ptr);  
	
	//t6 = _mm512_shuffle_f64x2(r5, r7, _MM_SHUFFLE(2,0,2,0));
	t6_k = _mm512_shuffle_f64x2(r5_k, r7_k, _MM_SHUFFLE(2,0,2,0));
	t6_ptr = _mm512_shuffle_f64x2(r5_ptr, r7_ptr, _MM_SHUFFLE(2,0,2,0));
	//m6 = _mm512_permutexvar_pd(idx, t6);  
	m6_k = _mm512_permutexvar_pd(idx, t6_k);  
	m6_ptr = _mm512_permutexvar_pd(idx, t6_ptr);  
	
	//t7 = _mm512_shuffle_f64x2(r5, r7, _MM_SHUFFLE(3,1,3,1));	
	t7_k = _mm512_shuffle_f64x2(r5_k, r7_k, _MM_SHUFFLE(3,1,3,1));	
	t7_ptr = _mm512_shuffle_f64x2(r5_ptr, r7_ptr, _MM_SHUFFLE(3,1,3,1));	
	//m7 = _mm512_permutexvar_pd(idx, t7);  
	m7_k = _mm512_permutexvar_pd(idx, t7_k);  
	m7_ptr = _mm512_permutexvar_pd(idx, t7_ptr);  
	
	
	idx = _mm512_set_epi64(11, 10, 9, 8, 3, 2, 1, 0);
  	//t0 = _mm512_permutex2var_pd(m0, idx, m4);	
  	t0_k = _mm512_permutex2var_pd(m0_k, idx, m4_k);	
  	t0_ptr = _mm512_permutex2var_pd(m0_ptr, idx, m4_ptr);	
  	
	//t1 = _mm512_permutex2var_pd(m1, idx, m5);	
	t1_k = _mm512_permutex2var_pd(m1_k, idx, m5_k);	
	t1_ptr = _mm512_permutex2var_pd(m1_ptr, idx, m5_ptr);	
  	
	//t2 = _mm512_permutex2var_pd(m2, idx, m6);	
	t2_k = _mm512_permutex2var_pd(m2_k, idx, m6_k);	
	t2_ptr = _mm512_permutex2var_pd(m2_ptr, idx, m6_ptr);	
  	
	//t3 = _mm512_permutex2var_pd(m3, idx, m7);	
	t3_k = _mm512_permutex2var_pd(m3_k, idx, m7_k);	
	t3_ptr = _mm512_permutex2var_pd(m3_ptr, idx, m7_ptr);	


	idx = _mm512_set_epi64(15, 14, 13, 12, 7, 6, 5, 4);
  	//t4 = _mm512_permutex2var_pd(m0, idx, m4);	
  	t4_k = _mm512_permutex2var_pd(m0_k, idx, m4_k);	
  	t4_ptr = _mm512_permutex2var_pd(m0_ptr, idx, m4_ptr);	
  	
	//t5 = _mm512_permutex2var_pd(m1, idx, m5);	
	t5_k = _mm512_permutex2var_pd(m1_k, idx, m5_k);	
	t5_ptr = _mm512_permutex2var_pd(m1_ptr, idx, m5_ptr);	
  	
	//t6 = _mm512_permutex2var_pd(m2, idx, m6);	
	t6_k = _mm512_permutex2var_pd(m2_k, idx, m6_k);	
	t6_ptr = _mm512_permutex2var_pd(m2_ptr, idx, m6_ptr);	
  	
	//t7 = _mm512_permutex2var_pd(m3, idx, m7);	
	t7_k = _mm512_permutex2var_pd(m3_k, idx, m7_k);	
	t7_ptr = _mm512_permutex2var_pd(m3_ptr, idx, m7_ptr);	


	// store to output
	//_mm512_storeu_pd ((int64_t *) output, t0);
	_mm512_storeu_pd ((int64_t *) out_k, t0_k);
	_mm512_storeu_pd ((int64_t *) out_ptr, t0_ptr);
	
	//_mm512_storeu_pd ((int64_t *) (output + 8), t1);
	_mm512_storeu_pd ((int64_t *) (out_k + 8), t1_k);
	_mm512_storeu_pd ((int64_t *) (out_ptr + 8), t1_ptr);
	
	//_mm512_storeu_pd ((int64_t *) (output + 16), t2);
	_mm512_storeu_pd ((int64_t *) (out_k + 16), t2_k);
	_mm512_storeu_pd ((int64_t *) (out_ptr + 16), t2_ptr);
	
	//_mm512_storeu_pd ((int64_t *) (output + 24), t3);
	_mm512_storeu_pd ((int64_t *) (out_k + 24), t3_k);
	_mm512_storeu_pd ((int64_t *) (out_ptr + 24), t3_ptr);
	
	//_mm512_storeu_pd ((int64_t *) (output + 32), t4);
	_mm512_storeu_pd ((int64_t *) (out_k + 32), t4_k);
	_mm512_storeu_pd ((int64_t *) (out_ptr + 32), t4_ptr);
	
	//_mm512_storeu_pd ((int64_t *) (output + 40), t5);
	_mm512_storeu_pd ((int64_t *) (out_k + 40), t5_k);
	_mm512_storeu_pd ((int64_t *) (out_ptr + 40), t5_ptr);
	
	//_mm512_storeu_pd ((int64_t *) (output + 48), t6);
	_mm512_storeu_pd ((int64_t *) (out_k + 48), t6_k);
	_mm512_storeu_pd ((int64_t *) (out_ptr + 48), t6_ptr);
	
	//_mm512_storeu_pd ((int64_t *) (output + 56), t7);
	_mm512_storeu_pd ((int64_t *) (out_k + 56), t7_k);
	_mm512_storeu_pd ((int64_t *) (out_ptr + 56), t7_ptr);
}


#endif /* X2_BITONIC_SORT_MERGE_CORE_H */
