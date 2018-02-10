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
#include <cstdint>
#include <tgmath.h>

/*
 * Function: sort <k,ptr> by k. 
 * Requirment: len doesn't need to be 8 aligned
 * @input_k: key array of input
 * @input_ptr: pointer array of input
 * @len: length of input_k and input_ptr
 * */
typedef struct{ int64_t k; int64_t ptr;} k_ptr_t;
int k_cmp(const void *l, const void *r){ return (((k_ptr_t *)l)->k - ((k_ptr_t *)r)->k );}
void x2_qsort(int64_t* input_k, int64_t* input_ptr, int64_t len){
	k_ptr_t * kptr = (k_ptr_t *) malloc(sizeof(k_ptr_t) * len); 	
	
	for(int64_t i = 0; i < len; i++){
		kptr[i].k = input_k[i];
		kptr[i].ptr = input_ptr[i];
	}
	
	qsort(kptr, len, sizeof(k_ptr_t), k_cmp);
	
	for(int64_t i = 0; i < len; i++){
		input_k[i] = kptr[i].k;
		input_ptr[i] = kptr[i].ptr;
	}
}

void x2_qsort(int64_t* input_k, int64_t* input_ptr, int64_t* output_k, int64_t* output_ptr, int64_t len){
	k_ptr_t * kptr = (k_ptr_t *) malloc(sizeof(k_ptr_t) * len); 	
	
	for(int64_t i = 0; i < len; i++){
		kptr[i].k = input_k[i];
		kptr[i].ptr = input_ptr[i];
	}
	
	qsort(kptr, len, sizeof(k_ptr_t), k_cmp);
	
	for(int64_t i = 0; i < len; i++){
		output_k[i] = kptr[i].k;
		output_ptr[i] = kptr[i].ptr;
	}
}

/*
 * Function: sort every 8-element blocks by key 
 * @k: key array
 * @ptr: pointer array
 * @len: # of keys in key array or # of pointers in pointer array
 * */
void x2_sort_every_8(int64_t* k, int64_t* ptr, uint64_t len){
	if(len%8 != 0){	
		printf("len is not 8 aligned in x2_sort_every_8()\n");
		abort();
	}
	int64_t* idx_k = k;
	int64_t* idx_ptr = ptr;
#if 0
	// only use x2_bitonic_sort_avx512_1x8()
	uint64_t num_blocks = len/8;
	for(uint64_t i = 0; i < num_blocks; i++){
		x2_bitonic_sort_avx512_1x8(idx_k, idx_ptr);
		idx_k = idx_k + 8;
		idx_ptr = idx_ptr + 8;
	}
#endif
	// use x2_bitonic_sort_avx512_1x8() and x2_bitonic_sort_avx512_8x8()
	uint64_t num_blocks_64 = len/64;
	uint64_t num_blocks_8 = (len - num_blocks_64*64)/8;
	
	for(uint64_t i = 0; i < num_blocks_64; i++){
		x2_bitonic_sort_avx512_8x8(idx_k, idx_ptr);
		idx_k = idx_k + 64;
		idx_ptr = idx_ptr + 64;
	}

	for(uint64_t i = 0; i < num_blocks_8; i++){
		x2_bitonic_sort_avx512_1x8(idx_k, idx_ptr);
		idx_k = idx_k + 8;
		idx_ptr = idx_ptr + 8;
	}
}

void x2_sort_every_8(int64_t* k, int64_t* ptr, int64_t* out_k, int64_t* out_ptr, uint64_t len){
	if(len%8 != 0){	
		printf("len is not 8 aligned in x2_sort_every_8()\n");
		abort();
	}
	int64_t* idx_k = k;
	int64_t* idx_ptr = ptr;
	int64_t* idx_k_out = out_k;
	int64_t* idx_ptr_out = out_ptr;
#if 0
	// only use x2_bitonic_sort_avx512_1x8()
	uint64_t num_blocks = len/8;
	for(uint64_t i = 0; i < num_blocks; i++){
		x2_bitonic_sort_avx512_1x8(idx_k, idx_ptr);
		idx_k = idx_k + 8;
		idx_ptr = idx_ptr + 8;
	}
#endif
	// use x2_bitonic_sort_avx512_1x8() and x2_bitonic_sort_avx512_8x8()
	uint64_t num_blocks_64 = len/64;
	uint64_t num_blocks_8 = (len - num_blocks_64*64)/8;
	
	for(uint64_t i = 0; i < num_blocks_64; i++){
		x2_bitonic_sort_avx512_8x8(idx_k, idx_ptr, idx_k_out, idx_ptr_out);
		idx_k = idx_k + 64;
		idx_ptr = idx_ptr + 64;
		idx_k_out = idx_k_out + 64;
		idx_ptr_out = idx_ptr_out + 64;
	}

	for(uint64_t i = 0; i < num_blocks_8; i++){
		x2_bitonic_sort_avx512_1x8(idx_k, idx_ptr, idx_k_out, idx_ptr_out);
		idx_k = idx_k + 8;
		idx_ptr = idx_ptr + 8;
		idx_k_out = idx_k_out + 8;
		idx_ptr_out = idx_ptr_out + 8;
	}
}

/*
 * Function: merge <in1_k, in1_ptr> with <in2_k, in2_ptr> to <out_k, out_ptr>
 * Requirement: len1 and len 2 are 8-aligned, in1_k and in2_k are sorted
 * @input1_k: key array of input1
 * @input1_ptr: pointer array of input1 
 * @input2_k: key array of input2
 * @input2_ptr: pointer array of input2 
 * @output_k: key array of merged sorted output 
 * @output_ptr: pointer array of merged sorted output
 * @len1: length of input1_k and input1_ptr
 * @len2: lenght of input2_k and input2_ptr
 * */
void x2_merge_8_aligned(int64_t* input1_k, int64_t* input1_ptr, int64_t* input2_k,
	int64_t* input2_ptr, int64_t* output_k, int64_t* output_ptr, int len1, int len2){
	
	int64_t* in1_k = input1_k;
	int64_t* in1_ptr = input1_ptr;
	int64_t* in2_k = input2_k;
	int64_t* in2_ptr = input2_ptr;
	int64_t* out_k = output_k;
	int64_t* out_ptr = output_ptr;
	
	int64_t* in1_k_end = in1_k + len1;
	int64_t* in2_k_end = in2_k + len2;

	__m512d reg_out_small_k, reg_out_small_ptr, reg_out_large_k, reg_out_large_ptr;
	__m512d reg1_k = _mm512_loadu_pd(in1_k);			
	__m512d reg1_ptr = _mm512_loadu_pd(in1_ptr);			
	__m512d reg2_k = _mm512_loadu_pd(in2_k);			
	__m512d reg2_ptr = _mm512_loadu_pd(in2_ptr);			
	
	x2_bitonic_merge_avx512_2x8(reg1_k, reg1_ptr, reg2_k, reg2_ptr, 
		reg_out_small_k, reg_out_small_ptr, reg_out_large_k, reg_out_large_ptr);
	
	_mm512_storeu_pd(out_k, reg_out_small_k);
	_mm512_storeu_pd(out_ptr, reg_out_small_ptr);
	
	in1_k = in1_k + 8;
	in2_k = in2_k + 8;
	out_k = out_k + 8;
	in1_ptr = in1_ptr + 8;
	in2_ptr = in2_ptr + 8;
	out_ptr = out_ptr + 8;
	
	while(in1_k < in1_k_end && in2_k < in2_k_end){
		if(*(int64_t *)in1_k < *(int64_t *)in2_k){
			reg1_k = _mm512_loadu_pd(in1_k);	
			reg1_ptr = _mm512_loadu_pd(in1_ptr);	
			
			x2_bitonic_merge_avx512_2x8(reg1_k, reg1_ptr, reg_out_large_k, reg_out_large_ptr, 
					reg_out_small_k, reg_out_small_ptr, reg_out_large_k, reg_out_large_ptr);
			
			_mm512_storeu_pd(out_k, reg_out_small_k);
			_mm512_storeu_pd(out_ptr, reg_out_small_ptr);
			
			in1_k = in1_k + 8;
			out_k = out_k + 8;
			in1_ptr = in1_ptr + 8;
			out_ptr = out_ptr + 8;
		}else{
			reg2_k = _mm512_loadu_pd(in2_k);	
			reg2_ptr = _mm512_loadu_pd(in2_ptr);	
			
			x2_bitonic_merge_avx512_2x8(reg2_k, reg2_ptr, reg_out_large_k, reg_out_large_ptr, 
					reg_out_small_k, reg_out_small_ptr, reg_out_large_k, reg_out_large_ptr);
			
			_mm512_storeu_pd(out_k, reg_out_small_k);
			_mm512_storeu_pd(out_ptr, reg_out_small_ptr);
			
			in2_k = in2_k + 8;
			out_k = out_k + 8;
			in2_ptr = in2_ptr + 8;
			out_ptr = out_ptr + 8;
		}
	}
	
	/* continue merge */
	while(in1_k < in1_k_end){
		reg1_k = _mm512_loadu_pd(in1_k);	
		reg1_ptr = _mm512_loadu_pd(in1_ptr);	

		x2_bitonic_merge_avx512_2x8(reg1_k, reg1_ptr, reg_out_large_k, reg_out_large_ptr, 
				reg_out_small_k, reg_out_small_ptr, reg_out_large_k, reg_out_large_ptr);

		_mm512_storeu_pd(out_k, reg_out_small_k);
		_mm512_storeu_pd(out_ptr, reg_out_small_ptr);
		
		in1_k = in1_k + 8;
		out_k = out_k + 8;
		in1_ptr = in1_ptr + 8;
		out_ptr = out_ptr + 8;
	}

	while(in2_k < in2_k_end){
		reg2_k = _mm512_loadu_pd(in2_k);	
		reg2_ptr = _mm512_loadu_pd(in2_ptr);	

		x2_bitonic_merge_avx512_2x8(reg2_k, reg2_ptr, reg_out_large_k, reg_out_large_ptr, 
				reg_out_small_k, reg_out_small_ptr, reg_out_large_k, reg_out_large_ptr);

		_mm512_storeu_pd(out_k, reg_out_small_k);
		_mm512_storeu_pd(out_ptr, reg_out_small_ptr);

		in2_k = in2_k + 8;
		out_k = out_k + 8;
		in2_ptr = in2_ptr + 8;
		out_ptr = out_ptr + 8;
	}

	_mm512_storeu_pd(out_k, reg_out_large_k);
	_mm512_storeu_pd(out_ptr, reg_out_large_ptr);
}


/*
 * Function: merge <in1_k, in1_ptr> with <in2_k, in2_ptr> to <out_k, out_ptr>
 * Requirement: len1 and len 2 are NOT 8-aligned, in1_k and in2_k are sorted
 * @in1_k: key array of input1
 * @in1_ptr: pointer array of input1 
 * @in2_k: key array of input2
 * @in2_ptr: pointer array of input2 
 * @out_k: key array of merged sorted output 
 * @in1_ptr: pointer array of merged sorted output
 * @len1: length of in1_k and in1_ptr
 * @len2: lenght of in2_k and in2_ptr
 * XXX: update x2_merge_2x8_to_16 to avoid extra storing/loading to register.
 * */
void x2_merge_8_unaligned(int64_t* input1_k, int64_t* input1_ptr, int64_t* input2_k,
	int64_t* input2_ptr, int64_t* output_k, int64_t* output_ptr, int len1, int len2){

	int64_t* in1_k = input1_k;
	int64_t* in1_ptr = input1_ptr;
	int64_t* in2_k = input2_k;
	int64_t* in2_ptr = input2_ptr;
	int64_t* out_k = output_k;
	int64_t* out_ptr = output_ptr;

	/* 8-aligned len */
	int64_t len8_1 = (len1/8)*8;
	int64_t len8_2 = (len2/8)*8;
	
	/* end of 8-aligned part */
	int64_t * k_end8_1 = in1_k + len8_1;
	int64_t * k_end8_2 = in2_k + len8_2;

	/* end of  the whole array */
	int64_t * k_end_1 = in1_k + len1;
	int64_t * k_end_2 = in2_k + len2;

	if(len8_1 > 8 && len8_2 > 8){
		__m512d reg_out_small_k, reg_out_small_ptr, reg_out_large_k, reg_out_large_ptr;
		__m512d reg1_k = _mm512_loadu_pd(in1_k);			
		__m512d reg1_ptr = _mm512_loadu_pd(in1_ptr);			
		__m512d reg2_k = _mm512_loadu_pd(in2_k);			
		__m512d reg2_ptr = _mm512_loadu_pd(in2_ptr);			

		x2_bitonic_merge_avx512_2x8(reg1_k, reg1_ptr, reg2_k, reg2_ptr, 
				reg_out_small_k, reg_out_small_ptr, reg_out_large_k, reg_out_large_ptr);

		_mm512_storeu_pd(out_k, reg_out_small_k);
		_mm512_storeu_pd(out_ptr, reg_out_small_ptr);
		
		in1_k = in1_k + 8;
		in2_k = in2_k + 8;
		out_k = out_k + 8;
		in1_ptr = in1_ptr + 8;
		in2_ptr = in2_ptr + 8;
		out_ptr = out_ptr + 8;

		while(in1_k < k_end8_1 && in2_k < k_end8_2){
			if(*(int64_t *)in1_k < *(int64_t *)in2_k){
				reg1_k = _mm512_loadu_pd(in1_k);	
				reg1_ptr = _mm512_loadu_pd(in1_ptr);	

				x2_bitonic_merge_avx512_2x8(reg1_k, reg1_ptr, reg_out_large_k, reg_out_large_ptr, 
						reg_out_small_k, reg_out_small_ptr, reg_out_large_k, reg_out_large_ptr);

				_mm512_storeu_pd(out_k, reg_out_small_k);
				_mm512_storeu_pd(out_ptr, reg_out_small_ptr);

				in1_k = in1_k + 8;
				out_k = out_k + 8;
				in1_ptr = in1_ptr + 8;
				out_ptr = out_ptr + 8;
			}else{
				reg2_k = _mm512_loadu_pd(in2_k);	
				reg2_ptr = _mm512_loadu_pd(in2_ptr);	

				x2_bitonic_merge_avx512_2x8(reg2_k, reg2_ptr, reg_out_large_k, reg_out_large_ptr, 
						reg_out_small_k, reg_out_small_ptr, reg_out_large_k, reg_out_large_ptr);

				_mm512_storeu_pd(out_k, reg_out_small_k);
				_mm512_storeu_pd(out_ptr, reg_out_small_ptr);

				in2_k = in2_k + 8;
				out_k = out_k + 8;
				in2_ptr = in2_ptr + 8;
				out_ptr = out_ptr + 8;
			}
		}

		/* store larger 8 elements in out_k to the larger one of in1_k and in2_k */
		if(*((int64_t *)(in1_k - 1)) < *((int64_t *)(in2_k -1))){
			in2_k = in2_k - 8;
			in2_ptr = in2_ptr - 8;
			_mm512_storeu_pd(in2_k, reg_out_large_k);
			_mm512_storeu_pd(in2_ptr, reg_out_large_ptr);
		}else{
			in1_k = in1_k - 8;
			in1_ptr = in1_ptr - 8;
			_mm512_storeu_pd(in1_k, reg_out_large_k);
			_mm512_storeu_pd(in1_ptr, reg_out_large_ptr);
			
		}
		
		/* serial merge */
		while(in1_k < k_end_1 && in2_k < k_end_2){
			if(*(in1_k) < *(in2_k)){
				*(out_k++) = *(in1_k++);
				*(out_ptr++) = *(in1_ptr++);
			}else{
				*(out_k++) = *(in2_k++);
				*(out_ptr++) = *(in2_ptr++);
			}
		}
		
		while(in1_k < k_end_1){
			*(out_k++) = *(in1_k++);
			*(out_ptr++) = *(in1_ptr++);
		}
		
		while(in2_k < k_end_2){
			*(out_k++) = *(in2_k++);
			*(out_ptr++) = *(in2_ptr++);
		}
	}else{
		/* either in_1's or in_2's len is < 8 */
		/* serial merge */
		while(in1_k < k_end_1 && in2_k < k_end_2){
			if(*(in1_k) < *(in2_k)){
				*(out_k++) = *(in1_k++);
				*(out_ptr++) = *(in1_ptr++);
			}else{
				*(out_k++) = *(in2_k++);
				*(out_ptr++) = *(in2_ptr++);
			}
		}
		
		while(in1_k < k_end_1){
			*(out_k++) = *(in1_k++);
			*(out_ptr++) = *(in1_ptr++);
		}
		
		while(in2_k < k_end_2){
			*(out_k++) = *(in2_k++);
			*(out_ptr++) = *(in2_ptr++);
		}
	}
}


/*
 * Function: sort key array(and pointer array) by key 
 * @k: key array
 * @ptr: pointer array
 * @len: # of keys in key array or # of pointers in pointer array
 * */
void x2_sort(int64_t* k, int64_t* ptr, uint64_t len){
	/* if len <= 8, just use x2_qsort directly */
	if(len <= 8){
		x2_qsort(k, ptr, len);
		return;
	}

	/* else if len > 8, use sort and merge*/
	uint64_t aligned_len = (len/8)*8;
	uint64_t unaligned_len = len - aligned_len;
	uint64_t blocks_num = len/8;

	/* sort 8-aligned blocks and probably 1 non-8-aligned block*/
	x2_sort_every_8(k, ptr, aligned_len);
	if(unaligned_len){
		x2_qsort(k + aligned_len, ptr + aligned_len, unaligned_len);
		blocks_num ++;
	}
	
	/* calculate how many layers in merge */
	int layer; // total merge layers
	double log_d = std::log2(blocks_num);
	int log_i = (int) log_d;
	if(log_d - log_i == 0){
		layer = log_i;
	}else{
		layer = log_i + 1;
	}

	/* allocate swap arrays for merge */
	int64_t* out_k = (int64_t *) malloc(sizeof(int64_t) * len);
	int64_t* out_ptr = (int64_t *) malloc(sizeof(int64_t) * len);
	/* for free memory */
	int64_t* free_out_k = out_k;
	int64_t* free_out_ptr = out_ptr;


	/* start merge */
	int64_t* in_k = k;
	int64_t* in_ptr = ptr;
	uint64_t start_1, start_2;
	uint64_t len_1, len_2;
	int64_t* swap_k;
	int64_t* swap_ptr;
	
	bool direct_copy; //last part alone, no merge, directly copy
	uint64_t layer_index;

	uint64_t block_size = 8; //starts from 8, 16, 32...	
	for(int i = 0; i < layer; i++){
		layer_index = 0;
		
		while(layer_index < len){
			direct_copy = false;
		
			/* set merge region 1 */
			start_1 = layer_index;
			if(start_1 + block_size >= len){
				direct_copy = true;
				len_1 = len - start_1;
			}else{
				len_1 = block_size;
			}

			layer_index = layer_index + block_size;

			/* set merge region 2 */
			if(layer_index < len){
				start_2 = layer_index;
				if(start_2 + block_size >= len){
					len_2 = len - start_2;
				}else{
					len_2 = block_size;
				}
			}
			layer_index = layer_index + block_size;
		
			if(direct_copy){
				for(uint64_t m = 0; m < len_1; m++){
					out_k[start_1 + m] = in_k[start_1 + m];
					out_ptr[start_1 + m] = in_ptr[start_1 + m];
				}
			}else{
				if(len_2 == block_size){
					x2_merge_8_aligned(in_k + start_1, in_ptr + start_1, in_k + start_2, in_ptr + start_2, out_k + start_1, out_ptr + start_1, len_1, len_2);
				}else{
					x2_merge_8_unaligned(in_k + start_1, in_ptr + start_1, in_k + start_2, in_ptr + start_2, out_k + start_1, out_ptr + start_1, len_1, len_2);
				}
			}
		}
		
		//swap input and output
		swap_k = in_k;
		swap_ptr = in_ptr;
		in_k = out_k;
		in_ptr = out_ptr;
		out_k = swap_k;
		out_ptr = swap_ptr;
		
		// double block_size
		block_size = block_size * 2;
	}
		
	// have to copy in this case	
	if(layer % 2 == 1){
		for(uint64_t i = 0; i < len; i++){
			k[i] = in_k[i];
			ptr[i] = in_ptr[i];
		}
	}
	
	free(free_out_k);
	free(free_out_ptr);
}

void x2_sort(int64_t* k, int64_t* ptr, int64_t* output_k, int64_t* output_ptr, uint64_t len){
	/* if len <= 8, just use x2_qsort directly */
	if(len <= 8){
		x2_qsort(k, ptr, output_k, output_ptr, len);
		return;
	}

	/* else if len > 8, use sort and merge*/
	uint64_t aligned_len = (len/8)*8;
	uint64_t unaligned_len = len - aligned_len;
	uint64_t blocks_num = len/8;

	/* sort 8-aligned blocks and probably 1 non-8-aligned block*/
	x2_sort_every_8(k, ptr, output_k, output_ptr, aligned_len);

	if(unaligned_len){
		x2_qsort(k + aligned_len, ptr + aligned_len, output_k + aligned_len, output_ptr + aligned_len, unaligned_len);
		blocks_num ++;
	}
	
	/* calculate how many layers in merge */
	int layer; // total merge layers
	double log_d = std::log2(blocks_num);
	int log_i = (int) log_d;
	if(log_d - log_i == 0){
		layer = log_i;
	}else{
		layer = log_i + 1;
	}

	/* start merge */
	//after x2_sort_every_8() and x2_qsort, sorted results are sroted in output_k and output_ptr
	int64_t* in_k = output_k;
	int64_t* in_ptr = output_ptr;
	
	/* allocate swap arrays for merge */
	int64_t* out_k = k;
	int64_t* out_ptr = ptr;
	
	uint64_t start_1, start_2;
	uint64_t len_1, len_2;
	int64_t* swap_k;
	int64_t* swap_ptr;
	
	bool direct_copy; //last part alone, no merge, directly copy
	uint64_t layer_index;

	uint64_t block_size = 8; //starts from 8, 16, 32...	
	for(int i = 0; i < layer; i++){
		layer_index = 0;
		
		while(layer_index < len){
			direct_copy = false;
		
			/* set merge region 1 */
			start_1 = layer_index;
			if(start_1 + block_size >= len){
				direct_copy = true;
				len_1 = len - start_1;
			}else{
				len_1 = block_size;
			}

			layer_index = layer_index + block_size;

			/* set merge region 2 */
			if(layer_index < len){
				start_2 = layer_index;
				if(start_2 + block_size >= len){
					len_2 = len - start_2;
				}else{
					len_2 = block_size;
				}
			}
			layer_index = layer_index + block_size;
		
			if(direct_copy){
				for(uint64_t m = 0; m < len_1; m++){
					out_k[start_1 + m] = in_k[start_1 + m];
					out_ptr[start_1 + m] = in_ptr[start_1 + m];
				}
			}else{
				if(len_2 == block_size){
					x2_merge_8_aligned(in_k + start_1, in_ptr + start_1, in_k + start_2, in_ptr + start_2, out_k + start_1, out_ptr + start_1, len_1, len_2);
				}else{
					x2_merge_8_unaligned(in_k + start_1, in_ptr + start_1, in_k + start_2, in_ptr + start_2, out_k + start_1, out_ptr + start_1, len_1, len_2);
				}
			}
		}
		
		//swap input and output
		swap_k = in_k;
		swap_ptr = in_ptr;
		in_k = out_k;
		in_ptr = out_ptr;
		out_k = swap_k;
		out_ptr = swap_ptr;
		
		// double block_size
		block_size = block_size * 2;

	}
		
	// have to copy in this case	
	if(layer % 2 == 1){
		for(uint64_t i = 0; i < len; i++){
			output_k[i] = in_k[i];
			output_ptr[i] = in_ptr[i];
		}
	}
}

/*
 * Function: check whether key array is sorted or not
 * @k: key array
 * @len: length of key array 
 * */
bool check_sort_k(int64_t* k, uint64_t len){
	for(uint64_t i = 0; i+1 < len; i++){
		if(k[i] > k[i+1]){
			return false;
		}
	}
	return true;
}

/*
 * Function: check whether key array is sorted or not, and whether move ptr correctly when sorting
 * Requirement: k[i] = ptr[i]
 * @k: key array
 * @ptr: ptr array
 * @len: length of key array and ptr array 
 * */
bool check_sort_kptr(int64_t* k, int64_t* ptr, uint64_t len){
	for(uint64_t i = 0; i+1 < len; i++){
		if(k[i] > k[i+1] || k[i] != ptr[i]){
			return false;
		}
	}
	return true;
}

#endif /* X2_SORT_MERGE_H */
