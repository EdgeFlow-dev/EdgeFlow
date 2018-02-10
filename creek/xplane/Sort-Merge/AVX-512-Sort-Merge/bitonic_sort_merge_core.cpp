#include "bitonic_sort_merge_core.h"
/*
 * Function: sort 8 int64_t values
 * @ptr: points to 8 int64_t values
 * Ref paper: https://arxiv.org/pdf/1704.08579.pdf
 * Ref code: https://gitlab.mpcdf.mpg.de/bbramas/avx-512-sort 
 * */
//void bitonic_sort_avx512_1x8(int64_t* ptr){
void bitonic_sort_avx512_1x8(int64_t* in_ptr, int64_t* out_ptr){
	//__m512d input = _mm512_loadu_pd(ptr);
	__m512d input = _mm512_loadu_pd(in_ptr);

    {
        __m512i idxNoNeigh = _mm512_set_epi64(6, 7, 4, 5, 2, 3, 0, 1);
        __m512d permNeigh = _mm512_permutexvar_pd(idxNoNeigh, input);
        __m512d permNeighMin = _mm512_min_pd(permNeigh, input);
        __m512d permNeighMax = _mm512_max_pd(permNeigh, input);
        input = _mm512_mask_mov_pd(permNeighMin, 0xAA, permNeighMax);
    }
    {
        __m512i idxNoNeigh = _mm512_set_epi64(4, 5, 6, 7, 0, 1, 2, 3);
        __m512d permNeigh = _mm512_permutexvar_pd(idxNoNeigh, input);
        __m512d permNeighMin = _mm512_min_pd(permNeigh, input);
        __m512d permNeighMax = _mm512_max_pd(permNeigh, input);
        input = _mm512_mask_mov_pd(permNeighMin, 0xCC, permNeighMax);
    }
    {
        __m512i idxNoNeigh = _mm512_set_epi64(6, 7, 4, 5, 2, 3, 0, 1);
        __m512d permNeigh = _mm512_permutexvar_pd(idxNoNeigh, input);
        __m512d permNeighMin = _mm512_min_pd(permNeigh, input);
        __m512d permNeighMax = _mm512_max_pd(permNeigh, input);
        input = _mm512_mask_mov_pd(permNeighMin, 0xAA, permNeighMax);
    }
    {
        __m512i idxNoNeigh = _mm512_set_epi64(0, 1, 2, 3, 4, 5, 6, 7);
        __m512d permNeigh = _mm512_permutexvar_pd(idxNoNeigh, input);
        __m512d permNeighMin = _mm512_min_pd(permNeigh, input);
        __m512d permNeighMax = _mm512_max_pd(permNeigh, input);
        input = _mm512_mask_mov_pd(permNeighMin, 0xF0, permNeighMax);
    }
    {
        __m512i idxNoNeigh = _mm512_set_epi64(5, 4, 7, 6, 1, 0, 3, 2);
        __m512d permNeigh = _mm512_permutexvar_pd(idxNoNeigh, input);
        __m512d permNeighMin = _mm512_min_pd(permNeigh, input);
        __m512d permNeighMax = _mm512_max_pd(permNeigh, input);
        input = _mm512_mask_mov_pd(permNeighMin, 0xCC, permNeighMax);
    }
    {
        __m512i idxNoNeigh = _mm512_set_epi64(6, 7, 4, 5, 2, 3, 0, 1);
        __m512d permNeigh = _mm512_permutexvar_pd(idxNoNeigh, input);
        __m512d permNeighMin = _mm512_min_pd(permNeigh, input);
        __m512d permNeighMax = _mm512_max_pd(permNeigh, input);
        input = _mm512_mask_mov_pd(permNeighMin, 0xAA, permNeighMax);
    }

	//_mm512_storeu_pd(ptr, input);
	_mm512_storeu_pd(out_ptr, input);
}


/*
 * Function: bitonic sort 8x8(64) int64_t values. Will get 8 sorted blocks, and each block has 8 values
 * @ptr: points to 8x8(64) int64_t values
 * */
//void bitonic_sort_avx512_8x8(int64_t* ptr){
void bitonic_sort_avx512_8x8(int64_t* in_ptr, int64_t* out_ptr){
	__m512d r0, r1, r2, r3, r4, r5, r6, r7;
	__m512d t0, t1, t2, t3, t4, t5, t6, t7;
	__m512d m0, m1, m2, m3, m4, m5, m6, m7;
	__m512i idx;
	
	// read 64 items to 8 registers
	r0 = _mm512_loadu_pd ((int64_t const *)(in_ptr));
	r1 = _mm512_loadu_pd ((int64_t const *)(in_ptr + 8));
	r2 = _mm512_loadu_pd ((int64_t const *)(in_ptr + 16));
	r3 = _mm512_loadu_pd ((int64_t const *)(in_ptr + 24));
	r4 = _mm512_loadu_pd ((int64_t const *)(in_ptr + 32));
	r5 = _mm512_loadu_pd ((int64_t const *)(in_ptr + 40));
	r6 = _mm512_loadu_pd ((int64_t const *)(in_ptr + 48));
	r7 = _mm512_loadu_pd ((int64_t const *)(in_ptr + 56));

	// 1st level of comparison
	t0 = _mm512_min_pd(r0, r1);
	t1 = _mm512_max_pd(r0, r1);
	t2 = _mm512_min_pd(r2, r3);
	t3 = _mm512_max_pd(r2, r3);
	t4 = _mm512_min_pd(r4, r5);
	t5 = _mm512_max_pd(r4, r5);
	t6 = _mm512_min_pd(r6, r7);
	t7 = _mm512_max_pd(r6, r7);

	// 2nd level of comparison
	r0 = _mm512_min_pd(t0, t3);
	r3 = _mm512_max_pd(t0, t3);
	r1 = _mm512_min_pd(t1, t2);
	r2 = _mm512_max_pd(t1, t2);
	r4 = _mm512_min_pd(t4, t7);
	r7 = _mm512_max_pd(t4, t7);
	r5 = _mm512_min_pd(t5, t6);
	r6 = _mm512_max_pd(t5, t6);

	// 3rd level of comparison
	t0 = _mm512_min_pd(r0, r1);
	t1 = _mm512_max_pd(r0, r1);
	t2 = _mm512_min_pd(r2, r3);
	t3 = _mm512_max_pd(r2, r3);
	t4 = _mm512_min_pd(r4, r5);
	t5 = _mm512_max_pd(r4, r5);
	t6 = _mm512_min_pd(r6, r7);
	t7 = _mm512_max_pd(r6, r7);

	// 4th level of comparision
	r0 = _mm512_min_pd(t0, t7);
	r7 = _mm512_max_pd(t0, t7);
	r1 = _mm512_min_pd(t1, t6);
	r6 = _mm512_max_pd(t1, t6);
	r2 = _mm512_min_pd(t2, t5);
	r5 = _mm512_max_pd(t2, t5);
	r3 = _mm512_min_pd(t3, t4);
	r4 = _mm512_max_pd(t3, t4);
	
	// 5th level of comparison
	t1 = _mm512_min_pd(r1, r3);
	t3 = _mm512_max_pd(r1, r3);
	t0 = _mm512_min_pd(r0, r2);
	t2 = _mm512_max_pd(r0, r2);
	t5 = _mm512_min_pd(r5, r7);
	t7 = _mm512_max_pd(r5, r7);
	t4 = _mm512_min_pd(r4, r6);
	t6 = _mm512_max_pd(r4, r6);

	// 6th level of comparison
	r0 = _mm512_min_pd(t0, t1);
	r1 = _mm512_max_pd(t0, t1);
	r2 = _mm512_min_pd(t2, t3);
	r3 = _mm512_max_pd(t2, t3);
	r4 = _mm512_min_pd(t4, t5);
	r5 = _mm512_max_pd(t4, t5);
	r6 = _mm512_min_pd(t6, t7);
	r7 = _mm512_max_pd(t6, t7);

	//transpose 8x8 matrix
	idx = _mm512_set_epi64(5, 7, 4, 6, 1, 3, 0, 2);	
	t0 = _mm512_permutexvar_pd(idx, r0);
	t1 = _mm512_permutexvar_pd(idx, r1);
	t2 = _mm512_permutexvar_pd(idx, r2);
	t3 = _mm512_permutexvar_pd(idx, r3);
	t4 = _mm512_permutexvar_pd(idx, r4);
	t5 = _mm512_permutexvar_pd(idx, r5);
	t6 = _mm512_permutexvar_pd(idx, r6);
	t7 = _mm512_permutexvar_pd(idx, r7);

	r0 = _mm512_unpackhi_pd(t0, t1);
	r1 = _mm512_unpacklo_pd(t0, t1);
	r2 = _mm512_unpackhi_pd(t2, t3);
	r3 = _mm512_unpacklo_pd(t2, t3);
	r4 = _mm512_unpackhi_pd(t4, t5);
	r5 = _mm512_unpacklo_pd(t4, t5);
	r6 = _mm512_unpackhi_pd(t6, t7);
	r7 = _mm512_unpacklo_pd(t6, t7);

	idx = _mm512_set_epi64(7, 6, 3, 2, 5, 4, 1, 0);
	t0 = _mm512_shuffle_f64x2(r0, r2, _MM_SHUFFLE(2,0,2,0));	
	m0 = _mm512_permutexvar_pd(idx, t0);  
	t1 = _mm512_shuffle_f64x2(r0, r2, _MM_SHUFFLE(3,1,3,1));	
	m1 = _mm512_permutexvar_pd(idx, t1);  
	t2 = _mm512_shuffle_f64x2(r1, r3, _MM_SHUFFLE(2,0,2,0));	
	m2 = _mm512_permutexvar_pd(idx, t2);  
	t3 = _mm512_shuffle_f64x2(r1, r3, _MM_SHUFFLE(3,1,3,1));	
	m3 = _mm512_permutexvar_pd(idx, t3);  
	t4 = _mm512_shuffle_f64x2(r4, r6, _MM_SHUFFLE(2,0,2,0));	
	m4 = _mm512_permutexvar_pd(idx, t4);  
	t5 = _mm512_shuffle_f64x2(r4, r6, _MM_SHUFFLE(3,1,3,1));
	m5 = _mm512_permutexvar_pd(idx, t5);  
	t6 = _mm512_shuffle_f64x2(r5, r7, _MM_SHUFFLE(2,0,2,0));
	m6 = _mm512_permutexvar_pd(idx, t6);  
	t7 = _mm512_shuffle_f64x2(r5, r7, _MM_SHUFFLE(3,1,3,1));	
	m7 = _mm512_permutexvar_pd(idx, t7);  
	
	idx = _mm512_set_epi64(11, 10, 9, 8, 3, 2, 1, 0);
  	t0 = _mm512_permutex2var_pd(m0, idx, m4);	
  	t1 = _mm512_permutex2var_pd(m1, idx, m5);	
  	t2 = _mm512_permutex2var_pd(m2, idx, m6);	
  	t3 = _mm512_permutex2var_pd(m3, idx, m7);	
	
	idx = _mm512_set_epi64(15, 14, 13, 12, 7, 6, 5, 4);
  	t4 = _mm512_permutex2var_pd(m0, idx, m4);	
  	t5 = _mm512_permutex2var_pd(m1, idx, m5);	
  	t6 = _mm512_permutex2var_pd(m2, idx, m6);	
  	t7 = _mm512_permutex2var_pd(m3, idx, m7);	
	
	// store sorted results 
	_mm512_storeu_pd ((int64_t *) out_ptr, t0);
	_mm512_storeu_pd ((int64_t *) (out_ptr + 8), t1);
	_mm512_storeu_pd ((int64_t *) (out_ptr + 16), t2);
	_mm512_storeu_pd ((int64_t *) (out_ptr + 24), t3);
	_mm512_storeu_pd ((int64_t *) (out_ptr + 32), t4);
	_mm512_storeu_pd ((int64_t *) (out_ptr + 40), t5);
	_mm512_storeu_pd ((int64_t *) (out_ptr + 48), t6);
	_mm512_storeu_pd ((int64_t *) (out_ptr + 56), t7);
}

/*
 * Function: bitonic merge 2x8 
 * @input1: points to 8 sorted values
 * @input2: points to 8 sorted values
 * @output1: points to sorted 8 smallest values of input1 and input2
 * @output2: points to sorted 8 larger values of input1 and input2
 * e.g. input1: 1,3,5,7,9,11,13,15 
 *      input2: 2,4,6,8,10,12,14,16
 *      output1: 1,2,3,4,5,6,7,8
 *      output2: 9,10,11,12,13,14,15,16
 * */
void bitonic_merge_avx512_2x8(int64_t * input1, int64_t * input2, int64_t * output1, int64_t * output2){
	__m512d in1 = _mm512_loadu_pd ((int64_t const *)(input1));
	__m512d in2 = _mm512_loadu_pd ((int64_t const *)(input2));
	__m512d out1, out2;

	{
		// 1-level comparison
		__m512i idx = _mm512_set_epi64(0, 1, 2, 3, 4, 5, 6, 7);
		__m512d perm_in2 = _mm512_permutexvar_pd(idx, in2);
		out1 = _mm512_min_pd(in1, perm_in2); 		
		out2 = _mm512_max_pd(in1, perm_in2); 	
	}
	{
		// 2-level comparison
		__m512i idx = _mm512_set_epi64(3, 2, 1, 0, 7, 6, 5, 4);
		
		// out1
		__m512d tmp1 = _mm512_permutexvar_pd(idx, out1);
		__m512d min_1 = _mm512_min_pd(out1, tmp1);
		__m512d max_1 = _mm512_max_pd(out1, tmp1);
		out1 = _mm512_mask_mov_pd(min_1, 0xF0, max_1);

		// out2
		__m512d tmp2 = _mm512_permutexvar_pd(idx, out2);
		__m512d min_2 = _mm512_min_pd(out2, tmp2);
		__m512d max_2 = _mm512_max_pd(out2, tmp2);
		out2 = _mm512_mask_mov_pd(min_2, 0xF0, max_2);
	}
	{
		// 3-level comparison
		__m512i idx = _mm512_set_epi64(5, 4, 7, 6, 1, 0, 3, 2);

		//out1
		__m512d tmp1 = _mm512_permutexvar_pd(idx, out1);
		__m512d min_1 = _mm512_min_pd(out1, tmp1);
		__m512d max_1 = _mm512_max_pd(out1, tmp1);
		out1 = _mm512_mask_mov_pd(min_1, 0xCC, max_1);	

		//out2
		__m512d tmp2 = _mm512_permutexvar_pd(idx, out2);
		__m512d min_2 = _mm512_min_pd(out2, tmp2);
		__m512d max_2 = _mm512_max_pd(out2, tmp2);
		out2 = _mm512_mask_mov_pd(min_2, 0xCC, max_2);	
	}
	{
		// 4-level comparison
		__m512i idx = _mm512_set_epi64(6, 7, 4, 5, 2, 3, 0, 1);

		// out1
		__m512d tmp1 = _mm512_permutexvar_pd(idx, out1);
		__m512d min_1 = _mm512_min_pd(out1, tmp1);
		__m512d max_1 = _mm512_max_pd(out1, tmp1);
		out1 = _mm512_mask_mov_pd(min_1, 0xAA, max_1);	

		// out2
		__m512d tmp2 = _mm512_permutexvar_pd(idx, out2);
		__m512d min_2 = _mm512_min_pd(out2, tmp2);
		__m512d max_2 = _mm512_max_pd(out2, tmp2);
		out2 = _mm512_mask_mov_pd(min_2, 0xAA, max_2);	
	}
	
	_mm512_storeu_pd ((int64_t *) output1, out1);
	_mm512_storeu_pd ((int64_t *) output2, out2);
}

void bitonic_merge_avx512_2x8(__m512d &in1, __m512d &in2, __m512d &out1, __m512d &out2){

	{
		// 1-level comparison
		__m512i idx = _mm512_set_epi64(0, 1, 2, 3, 4, 5, 6, 7);
		__m512d perm_in2 = _mm512_permutexvar_pd(idx, in2);
		out1 = _mm512_min_pd(in1, perm_in2); 		
		out2 = _mm512_max_pd(in1, perm_in2); 	
	}
	{
		// 2-level comparison
		__m512i idx = _mm512_set_epi64(3, 2, 1, 0, 7, 6, 5, 4);
		
		// out1
		__m512d tmp1 = _mm512_permutexvar_pd(idx, out1);
		__m512d min_1 = _mm512_min_pd(out1, tmp1);
		__m512d max_1 = _mm512_max_pd(out1, tmp1);
		out1 = _mm512_mask_mov_pd(min_1, 0xF0, max_1);

		// out2
		__m512d tmp2 = _mm512_permutexvar_pd(idx, out2);
		__m512d min_2 = _mm512_min_pd(out2, tmp2);
		__m512d max_2 = _mm512_max_pd(out2, tmp2);
		out2 = _mm512_mask_mov_pd(min_2, 0xF0, max_2);
	}
	{
		// 3-level comparison
		__m512i idx = _mm512_set_epi64(5, 4, 7, 6, 1, 0, 3, 2);

		//out1
		__m512d tmp1 = _mm512_permutexvar_pd(idx, out1);
		__m512d min_1 = _mm512_min_pd(out1, tmp1);
		__m512d max_1 = _mm512_max_pd(out1, tmp1);
		out1 = _mm512_mask_mov_pd(min_1, 0xCC, max_1);	

		//out2
		__m512d tmp2 = _mm512_permutexvar_pd(idx, out2);
		__m512d min_2 = _mm512_min_pd(out2, tmp2);
		__m512d max_2 = _mm512_max_pd(out2, tmp2);
		out2 = _mm512_mask_mov_pd(min_2, 0xCC, max_2);	
	}
	{
		// 4-level comparison
		__m512i idx = _mm512_set_epi64(6, 7, 4, 5, 2, 3, 0, 1);

		// out1
		__m512d tmp1 = _mm512_permutexvar_pd(idx, out1);
		__m512d min_1 = _mm512_min_pd(out1, tmp1);
		__m512d max_1 = _mm512_max_pd(out1, tmp1);
		out1 = _mm512_mask_mov_pd(min_1, 0xAA, max_1);	

		// out2
		__m512d tmp2 = _mm512_permutexvar_pd(idx, out2);
		__m512d min_2 = _mm512_min_pd(out2, tmp2);
		__m512d max_2 = _mm512_max_pd(out2, tmp2);
		out2 = _mm512_mask_mov_pd(min_2, 0xAA, max_2);	
	}
}

/*
 * Function: transpose a 8x8 matrix using AVX-512
 * @mat: input 8x8 int64_t matrix
 * @matT: transposed 8x8 matrix 
 * Ref: https://stackoverflow.com/questions/25622745/transpose-an-8x8-float-using-avx-avx2
 */
void transpose_8x8(int64_t * mat, int64_t *matT){
	__m512d r0, r1, r2, r3, r4, r5, r6, r7;
	__m512d t0, t1, t2, t3, t4, t5, t6, t7;
	__m512d m0, m1, m2, m3, m4, m5, m6, m7;
	__m512i idx;

	r0 = _mm512_loadu_pd(&mat[0*8]);
	r1 = _mm512_loadu_pd(&mat[1*8]);
	r2 = _mm512_loadu_pd(&mat[2*8]);
	r3 = _mm512_loadu_pd(&mat[3*8]);
	r4 = _mm512_loadu_pd(&mat[4*8]);
	r5 = _mm512_loadu_pd(&mat[5*8]);
	r6 = _mm512_loadu_pd(&mat[6*8]);
	r7 = _mm512_loadu_pd(&mat[7*8]);

	idx = _mm512_set_epi64(5, 7, 4, 6, 1, 3, 0, 2);	
	t0 = _mm512_permutexvar_pd(idx, r0);
	t1 = _mm512_permutexvar_pd(idx, r1);
	t2 = _mm512_permutexvar_pd(idx, r2);
	t3 = _mm512_permutexvar_pd(idx, r3);
	t4 = _mm512_permutexvar_pd(idx, r4);
	t5 = _mm512_permutexvar_pd(idx, r5);
	t6 = _mm512_permutexvar_pd(idx, r6);
	t7 = _mm512_permutexvar_pd(idx, r7);

	r0 = _mm512_unpackhi_pd(t0, t1);
	r1 = _mm512_unpacklo_pd(t0, t1);
	r2 = _mm512_unpackhi_pd(t2, t3);
	r3 = _mm512_unpacklo_pd(t2, t3);
	r4 = _mm512_unpackhi_pd(t4, t5);
	r5 = _mm512_unpacklo_pd(t4, t5);
	r6 = _mm512_unpackhi_pd(t6, t7);
	r7 = _mm512_unpacklo_pd(t6, t7);

	idx = _mm512_set_epi64(7, 6, 3, 2, 5, 4, 1, 0);
	t0 = _mm512_shuffle_f64x2(r0, r2, _MM_SHUFFLE(2,0,2,0));	
	m0 = _mm512_permutexvar_pd(idx, t0);  
	t1 = _mm512_shuffle_f64x2(r0, r2, _MM_SHUFFLE(3,1,3,1));	
	m1 = _mm512_permutexvar_pd(idx, t1);  
	t2 = _mm512_shuffle_f64x2(r1, r3, _MM_SHUFFLE(2,0,2,0));	
	m2 = _mm512_permutexvar_pd(idx, t2);  
	t3 = _mm512_shuffle_f64x2(r1, r3, _MM_SHUFFLE(3,1,3,1));	
	m3 = _mm512_permutexvar_pd(idx, t3);  
	t4 = _mm512_shuffle_f64x2(r4, r6, _MM_SHUFFLE(2,0,2,0));	
	m4 = _mm512_permutexvar_pd(idx, t4);  
	t5 = _mm512_shuffle_f64x2(r4, r6, _MM_SHUFFLE(3,1,3,1));
	m5 = _mm512_permutexvar_pd(idx, t5);  
	t6 = _mm512_shuffle_f64x2(r5, r7, _MM_SHUFFLE(2,0,2,0));
	m6 = _mm512_permutexvar_pd(idx, t6);  
	t7 = _mm512_shuffle_f64x2(r5, r7, _MM_SHUFFLE(3,1,3,1));	
	m7 = _mm512_permutexvar_pd(idx, t7);  
	
	idx = _mm512_set_epi64(11, 10, 9, 8, 3, 2, 1, 0);
  	t0 = _mm512_permutex2var_pd(m0, idx, m4);	
  	t1 = _mm512_permutex2var_pd(m1, idx, m5);	
  	t2 = _mm512_permutex2var_pd(m2, idx, m6);	
  	t3 = _mm512_permutex2var_pd(m3, idx, m7);	

	idx = _mm512_set_epi64(15, 14, 13, 12, 7, 6, 5, 4);
  	t4 = _mm512_permutex2var_pd(m0, idx, m4);	
  	t5 = _mm512_permutex2var_pd(m1, idx, m5);	
  	t6 = _mm512_permutex2var_pd(m2, idx, m6);	
  	t7 = _mm512_permutex2var_pd(m3, idx, m7);	

	_mm512_storeu_pd(&matT[0*8], t0);
	_mm512_storeu_pd(&matT[1*8], t1);
	_mm512_storeu_pd(&matT[2*8], t2);
	_mm512_storeu_pd(&matT[3*8], t3);
	_mm512_storeu_pd(&matT[4*8], t4);
	_mm512_storeu_pd(&matT[5*8], t5);
	_mm512_storeu_pd(&matT[6*8], t6);
	_mm512_storeu_pd(&matT[7*8], t7);
}

