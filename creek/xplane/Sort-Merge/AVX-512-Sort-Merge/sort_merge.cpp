//hym: this file provides sort() and merge() functions

//#include "sort_merge.h"
#include "bitonic_sort_merge_core.h"
#include <memory.h>
#include <stdio.h>
#include <cmath>
#include <iostream>
/* List of Functions in bitonic_sort_merge_core.h*/
/*
void bitonic_sort_avx512_1x8(int64_t* ptr);
void bitonic_sort_avx512_8x8(int64_t* ptr);
void bitonic_merge_avx512_2x8(int64_t * input1, int64_t * input2, int64_t * output1, int64_t * output2);
void transpose_8x8(int64_t * mat, int64_t *matT);
*/

//void sort_every_8(int64_t* ptr, uint64_t len){
void sort_every_8(int64_t* in_ptr, int64_t* out_ptr, uint64_t len){
	if(len%8 != 0){	
		printf("len is not 8 aligned in sort_every_8()\n");
		abort();
	}
	int64_t* idx_ptr_in = in_ptr;
	int64_t* idx_ptr_out = out_ptr;
	
	uint64_t num_blocks_64 = len/64;
	uint64_t num_blocks_8 = (len - num_blocks_64*64)/8;
	
	for(uint64_t i = 0; i < num_blocks_64; i++){
		bitonic_sort_avx512_8x8(idx_ptr_in, idx_ptr_out);
		idx_ptr_in = idx_ptr_in + 64;
		idx_ptr_out = idx_ptr_out + 64;
	}

	for(uint64_t i = 0; i < num_blocks_8; i++){
		bitonic_sort_avx512_1x8(idx_ptr_in, idx_ptr_out);
		idx_ptr_in = idx_ptr_in + 8;
		idx_ptr_out = idx_ptr_out + 8;
	}
}


void merge_8_aligned(int64_t* input1_k, int64_t* input2_k, int64_t* output_k, int len1, int len2){
	
	if(len1%8 != 0 || len2%8 != 0){
		printf("len1 or len2 is not 8 aligned in merge_8_aligned\n");
		abort();
	}

	if(len1 == 0 && len2 == 0){
		return;
	}

	int64_t* in1_k = input1_k;
	int64_t* in2_k = input2_k;
	int64_t* out_k = output_k;
	
	int64_t* in1_k_end = in1_k + len1;
	int64_t* in2_k_end = in2_k + len2;

	__m512d reg_out_small_k, reg_out_large_k;
	__m512d reg1_k = _mm512_loadu_pd(in1_k);			
	__m512d reg2_k = _mm512_loadu_pd(in2_k);			
	
	bitonic_merge_avx512_2x8(reg1_k, reg2_k, reg_out_small_k, reg_out_large_k);
	
	_mm512_storeu_pd(out_k, reg_out_small_k);
	
	in1_k = in1_k + 8;
	in2_k = in2_k + 8;
	out_k = out_k + 8;
	
	while(in1_k < in1_k_end && in2_k < in2_k_end){
		if(*(int64_t *)in1_k < *(int64_t *)in2_k){
			reg1_k = _mm512_loadu_pd(in1_k);	
			
			bitonic_merge_avx512_2x8(reg1_k, reg_out_large_k, 
					reg_out_small_k, reg_out_large_k);
			
			_mm512_storeu_pd(out_k, reg_out_small_k);
			
			in1_k = in1_k + 8;
			out_k = out_k + 8;
		}else{
			reg2_k = _mm512_loadu_pd(in2_k);	
			
			bitonic_merge_avx512_2x8(reg2_k, reg_out_large_k, 
					reg_out_small_k, reg_out_large_k);
			
			_mm512_storeu_pd(out_k, reg_out_small_k);
			
			in2_k = in2_k + 8;
			out_k = out_k + 8;
		}
	}
	
	/* continue merge */
	while(in1_k < in1_k_end){
		reg1_k = _mm512_loadu_pd(in1_k);	

		bitonic_merge_avx512_2x8(reg1_k, reg_out_large_k, 
				reg_out_small_k, reg_out_large_k);

		_mm512_storeu_pd(out_k, reg_out_small_k);
		
		in1_k = in1_k + 8;
		out_k = out_k + 8;
	}

	while(in2_k < in2_k_end){
		reg2_k = _mm512_loadu_pd(in2_k);	

		bitonic_merge_avx512_2x8(reg2_k, reg_out_large_k, 
				reg_out_small_k, reg_out_large_k);

		_mm512_storeu_pd(out_k, reg_out_small_k);

		in2_k = in2_k + 8;
		out_k = out_k + 8;
	}

	_mm512_storeu_pd(out_k, reg_out_large_k);
}


void merge_8_unaligned(int64_t* input1_k, int64_t* input2_k,
	int64_t* output_k, int len1, int len2){

	int64_t* in1_k = input1_k;
	int64_t* in2_k = input2_k;
	int64_t* out_k = output_k;

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
		__m512d reg_out_small_k, reg_out_large_k;
		__m512d reg1_k = _mm512_loadu_pd(in1_k);			
		__m512d reg2_k = _mm512_loadu_pd(in2_k);			

		bitonic_merge_avx512_2x8(reg1_k, reg2_k, 
				reg_out_small_k, reg_out_large_k);

		_mm512_storeu_pd(out_k, reg_out_small_k);
		
		in1_k = in1_k + 8;
		in2_k = in2_k + 8;
		out_k = out_k + 8;

		while(in1_k < k_end8_1 && in2_k < k_end8_2){
			if(*(int64_t *)in1_k < *(int64_t *)in2_k){
				reg1_k = _mm512_loadu_pd(in1_k);	

				bitonic_merge_avx512_2x8(reg1_k, reg_out_large_k, 
						reg_out_small_k, reg_out_large_k);

				_mm512_storeu_pd(out_k, reg_out_small_k);

				in1_k = in1_k + 8;
				out_k = out_k + 8;
			}else{
				reg2_k = _mm512_loadu_pd(in2_k);	

				bitonic_merge_avx512_2x8(reg2_k, reg_out_large_k, 
						reg_out_small_k, reg_out_large_k);

				_mm512_storeu_pd(out_k, reg_out_small_k);

				in2_k = in2_k + 8;
				out_k = out_k + 8;
			}
		}

		/* store larger 8 elements in out_k to the larger one of in1_k and in2_k */
		if(*((int64_t *)(in1_k - 1)) < *((int64_t *)(in2_k -1))){
			in2_k = in2_k - 8;
			_mm512_storeu_pd(in2_k, reg_out_large_k);
		}else{
			in1_k = in1_k - 8;
			_mm512_storeu_pd(in1_k, reg_out_large_k);
			
		}
		
		/* serial merge */
		while(in1_k < k_end_1 && in2_k < k_end_2){
			if(*(in1_k) < *(in2_k)){
				*(out_k++) = *(in1_k++);
			}else{
				*(out_k++) = *(in2_k++);
			}
		}
		
		while(in1_k < k_end_1){
			*(out_k++) = *(in1_k++);
		}
		
		while(in2_k < k_end_2){
			*(out_k++) = *(in2_k++);
		}
	}else{
		/* either in_1's or in_2's len is < 8 */
		/* serial merge */
		while(in1_k < k_end_1 && in2_k < k_end_2){
			if(*(in1_k) < *(in2_k)){
				*(out_k++) = *(in1_k++);
			}else{
				*(out_k++) = *(in2_k++);
			}
		}
		
		while(in1_k < k_end_1){
			*(out_k++) = *(in1_k++);
		}
		
		while(in2_k < k_end_2){
			*(out_k++) = *(in2_k++);
		}
	}
}

int compare(const void * a, const void * b)
{
    return ( *(int64_t*)a - *(int64_t*)b );
}

void sort(int64_t* k, int64_t* output_k, uint64_t len){
	/* if len <= 8, just use x2_qsort directly */
	if(len <= 8){
		//x2_qsort(k, ptr, output_k, output_ptr, len);
		qsort(k, len, sizeof(int64_t), compare);
		
		//for(int64_t i = 0; i < len; i++){
		//	output_k[i] = k[i];
		//}
		memcpy(output_k, k, sizeof(int64_t) * len);
		return;
	}

	/* else if len > 8, use sort and merge*/
	uint64_t aligned_len = (len/8)*8;
	uint64_t unaligned_len = len - aligned_len;
	uint64_t blocks_num = len/8;

	/* sort 8-aligned blocks and probably 1 non-8-aligned block*/
	sort_every_8(k, output_k, aligned_len);

	if(unaligned_len){
		qsort(k + aligned_len, unaligned_len, sizeof(int64_t), compare);
		memcpy(output_k+aligned_len, k + aligned_len, sizeof(int64_t) * unaligned_len);
		blocks_num ++;
	}
/*	
	std::cout << "after qsort: " << std::endl;
	for(int i = 0; i < unaligned_len; i++){
		std::cout << *(k + aligned_len + i) << ",";
	}
	std::cout << std::endl;
*/
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
	//int64_t* in_k = k;
	int64_t* in_k = output_k;
	/* allocate swap arrays for merge */
	//int64_t* out_k = output_k;
	int64_t* out_k = k;
	
	uint64_t start_1, start_2;
	uint64_t len_1, len_2;
	int64_t* swap_k;
	
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
				}
			}else{
				if(len_2 == block_size){
					merge_8_aligned(in_k + start_1, in_k + start_2, out_k + start_1, len_1, len_2);
				}else{
					merge_8_unaligned(in_k + start_1, in_k + start_2, out_k + start_1, len_1, len_2);
				}
			}
		}
		
		//swap input and output
		swap_k = in_k;
		in_k = out_k;
		out_k = swap_k;
		
		// double block_size
		block_size = block_size * 2;

	}
		
	// have to copy in this case	
	if(layer % 2 == 1){
		for(uint64_t i = 0; i < len; i++){
			output_k[i] = in_k[i];
		}
	}
}
