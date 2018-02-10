/* Author: Hongyu Miao @ Purdue ECE 
 * Date: Nov 9th, 2017 
 * Description: This file provides sort/merge functions for xplan interface 
 * */
#include "xplane_lib.h"
#include <cassert>
#include "x2_sort_merge.h"
//#include "xplane-types.h"
#include <cstring>
//extern "C"{
#include "mm/batch.h"
//}
#include "xplane/qsort_cmp.h"
#ifdef __x86_64
#define BATCH_NEW_PARAMETER	1
/*
 * Function: splits src into n_outputs parts, then sort each part and store sorted parts to dests 
 * Notes: x_addr stores the address of buffer descriptor(batch_t). batch_t stores a buffer's start address, size...
 * Requirement: NO
 * Return: if error happens, return vale < 0. this is just for secure world errors
 * @dests: array of x_addr, which stores the address of a descriptor of batch_t
 * @n_outputs: # of outputs. # of elements in dests[]
 * @src: stores the address of first input's descriptor(batch_t)
 * @keypos: key offset in a record. starts from 0
 * @reclen: # of elements in a record
 * */
int32_t sort(x_addr *dests, uint32_t n_outputs, x_addr src, idx_t keypos, idx_t reclen){
	batch_t *src_in = (batch_t *) src;
	uint64_t len = src_in->size/reclen; // # of records
	
	/* first (n_outputs - 1) partition have the same len */	
	uint64_t each_len = len/n_outputs;
	/* last partition may has different len with previous partitions' len */
	uint64_t last_len = len - (n_outputs - 1)*each_len;
/*	
	int64_t *each_k = (int64_t *) malloc(sizeof(int64_t) * each_len);
	int64_t *each_ptr = (int64_t *) malloc(sizeof(int64_t) * each_len);
	int64_t *last_k = (int64_t *) malloc(sizeof(int64_t) * last_len);
	int64_t *last_ptr = (int64_t *) malloc(sizeof(int64_t) * last_len);
*/	
	int64_t *each_k;
	int64_t *each_ptr;
	int64_t *last_k;
	int64_t *last_ptr;
	uint64_t buf_size = sizeof(int64_t) * last_len;
/*	
	if(buf_size <= BUFF_SIZE_1M){
		each_k = (int64_t *) get_bundle_slow_1M();	
		each_ptr = (int64_t *) get_bundle_slow_1M();	
		last_k = (int64_t *) get_bundle_slow_1M();	
		last_ptr = (int64_t *) get_bundle_slow_1M();	
	}else if(buf_size <= BUFF_SIZE_128M){
		each_k = (int64_t *) get_bundle_slow_128M();	
		each_ptr = (int64_t *) get_bundle_slow_128M();	
		last_k = (int64_t *) get_bundle_slow_128M();	
		last_ptr = (int64_t *) get_bundle_slow_128M();	
	
	}else{
		printf("unsupported buf_size!!!!!!!\n");
		abort();
	}
*/
	batch_t *b_each_k = batch_new(0, buf_size); 
	batch_t *b_each_ptr = batch_new(0, buf_size); 
	batch_t *b_last_k = batch_new(0, buf_size); 
	batch_t *b_last_ptr = batch_new(0, buf_size); 

	each_k = b_each_k->start;
	each_ptr = b_each_ptr->start;
	last_k = b_last_k->start;
	last_ptr = b_last_ptr->start;
	
	int64_t* idx = src_in->start;
	int64_t* store;
	
	/* sort first (n_outputs - 1) parts */
	for(uint32_t i = 0; i < n_outputs - 1; i++){
#if 0
		batch_t * tmp = (batch_t *) malloc(sizeof(batch_t));
		tmp->size = each_len * reclen;
		tmp->start = (int64_t *) malloc(sizeof(int64_t) * tmp->size);
		tmp->end = tmp->start + tmp->size;	
#endif	
		//batch_t *tmp = batch_new(BATCH_NEW_PARAMETER);	
		batch_t *tmp = batch_new(BATCH_NEW_PARAMETER, each_len * reclen * sizeof(simd_t));	
		batch_update(tmp, tmp->start + each_len * reclen);
		dests[i] = (x_addr) tmp;
		
		for(uint64_t j = 0; j < each_len; j++){
			each_k[j] = *(idx + keypos);
			each_ptr[j] = (int64_t)idx;
			idx = idx + reclen;
		}
		
		x2_sort(each_k, each_ptr, each_len);
		
		/* store records to dests[i] according to sorted each_ptr[] */
		store = tmp->start;
		for(uint64_t j = 0; j < each_len; j++){
			for(uint64_t k = 0; k < reclen; k++){
				store[k] = *((int64_t *)(each_ptr[j]) + k); 
			}
			store = store + reclen;
		}
	}

	/* sort last partition */
#if 0
	batch_t * tmp = (batch_t *) malloc(sizeof(batch_t));
	tmp->size = last_len * reclen;
	tmp->start = (int64_t *) malloc(sizeof(int64_t) * tmp->size);
	tmp->end = tmp->start + tmp->size;	
#endif	
	//batch_t *tmp = batch_new(BATCH_NEW_PARAMETER);	
	batch_t *tmp = batch_new(BATCH_NEW_PARAMETER, last_len * reclen * sizeof(simd_t));	
	batch_update(tmp, tmp->start + last_len * reclen);
	dests[n_outputs - 1] = (x_addr) tmp;

	for(uint64_t j = 0; j < last_len; j++){
		last_k[j] = *(idx + keypos);
		last_ptr[j] = (int64_t)idx;
		idx = idx + reclen;
	}

	x2_sort(last_k, last_ptr, last_len);

	/* store records to dests[i] according to sorted last_ptr[] */
	store = tmp->start;
	for(uint64_t j = 0; j < last_len; j++){
		for(uint64_t k = 0; k < reclen; k++){
			store[k] = *((int64_t *)(last_ptr[j]) + k); 
		}
		store = store + reclen;
	}
/*	
	free(each_k);	
	free(each_ptr);	
	free(last_k);	
	free(last_ptr);	
*/	

/*
	if(buf_size <= BUFF_SIZE_1M){
		return_bundle_slow_1M(each_k);	
		return_bundle_slow_1M(each_ptr);	
		return_bundle_slow_1M(last_k);	
		return_bundle_slow_1M(last_ptr);	
	}else if(buf_size <= BUFF_SIZE_128M){
		return_bundle_slow_128M(each_k);	
		return_bundle_slow_128M(each_ptr);	
		return_bundle_slow_128M(last_k);	
		return_bundle_slow_128M(last_ptr);	
	}else{
		printf("unsupported buf_size!!!!!!!\n");
		abort();
	}
*/	
	BATCH_KILL(b_each_k, 0);
	BATCH_KILL(b_each_ptr, 0);
	BATCH_KILL(b_last_k, 0);
	BATCH_KILL(b_last_ptr, 0);
	
	return 0;
}


int32_t sort_kp(x_addr *dests_k, x_addr *dests_p, uint32_t n_outputs, x_addr src_k, x_addr src_p){
	batch_t *src_in_k = (batch_t *) src_k;
	batch_t *src_in_p = (batch_t *) src_p;
	assert(src_in_k->size == src_in_p->size);
	uint64_t len = src_in_k->size; // # of keys
	
	/* first (n_outputs - 1) partition have the same len */	
	uint64_t each_len = len/n_outputs;
	/* last partition may has different len with previous partitions' len */
	uint64_t last_len = len - (n_outputs - 1)*each_len;
	
	int64_t *each_k = src_in_k->start;
	int64_t *each_ptr = src_in_p->start;
	int64_t *last_k = src_in_k->start + each_len * (n_outputs - 1);
	int64_t *last_ptr = src_in_p->start + each_len * (n_outputs - 1);

	/* sort first (n_outputs - 1) parts */
	for(uint32_t i = 0; i < n_outputs - 1; i++){
		batch_t *tmp_k = batch_new(BATCH_NEW_PARAMETER, each_len * sizeof(simd_t));	
		batch_t *tmp_ptr = batch_new(BATCH_NEW_PARAMETER, each_len * sizeof(simd_t));	
		batch_update(tmp_k, tmp_k->start + each_len);
		batch_update(tmp_ptr, tmp_ptr->start + each_len);
		dests_k[i] = (x_addr) tmp_k;
		dests_p[i] = (x_addr) tmp_ptr;
		
		x2_sort(each_k, each_ptr, tmp_k->start, tmp_ptr->start, each_len);
		
		each_k = each_k + each_len;
		each_ptr = each_ptr + each_len;
	}

	/* sort last partition */
	batch_t *tmp_k = batch_new(BATCH_NEW_PARAMETER, last_len * sizeof(simd_t));	
	batch_t *tmp_ptr = batch_new(BATCH_NEW_PARAMETER, last_len * sizeof(simd_t));	
	batch_update(tmp_k, tmp_k->start + last_len);
	batch_update(tmp_ptr, tmp_ptr->start + last_len);
	dests_k[n_outputs - 1] = (x_addr) tmp_k;
	dests_p[n_outputs - 1] = (x_addr) tmp_ptr;

	x2_sort(last_k, last_ptr, tmp_k->start, tmp_ptr->start, last_len);

	return 0;
}

/*
 * Function: splits srca and srcb into n_outputs parts, then merge each pair of their parts. 
 * Notes: x_addr stores the address of buffer descriptor(batch_t). batch_t stores a buffer's start address, size...
 * Requirement: NO
 * @dests: array of x_addr, which stores the address of a descriptor of batch_t
 * @n_outputs: # of outputs. # of elements in dests[]
 * @srca: stores the address of first input's descriptor(batch_t)
 * @srcb: stores the address of second input's descriptor(batch_t)
 * @keypos: key offset in a record. starts from 0
 * @reclen: # of elements in a record
 * */
int32_t merge(x_addr *dests, uint32_t n_outputs, x_addr srca, x_addr srcb,
			idx_t keypos, idx_t reclen){
	batch_t *src_A = (batch_t *) srca;
	batch_t *src_B = (batch_t *) srcb;
	uint64_t len_A = src_A->size/reclen; // # records in A
	uint64_t len_B = src_B->size/reclen; //# records in B
	int64_t * start_A = src_A->start;
	int64_t * start_B = src_B->start;
	int64_t * idx_A = start_A; 
	int64_t * idx_B = start_B; 
	int64_t * store;
	
	/* equally partition A and B */
	uint64_t each_len_A = len_A/n_outputs;
	uint64_t last_len_A = len_A - (n_outputs - 1)*each_len_A;
	uint64_t each_len_B = len_B/n_outputs;
	uint64_t last_len_B = len_B - (n_outputs - 1)*each_len_B;
	
	if(len_A == 0 && len_B == 0){
		/* case1: both of A and B are null, which is possible. Just allocate dests and return it.*/
		//assert(len_A && len_B);
		for(auto i = 0u; i < n_outputs; i++){
#if 0
			batch_t * tmp = (batch_t *) malloc(sizeof(batch_t));
			tmp->size = 0;
			tmp->start = NULL;	
			tmp->end = NULL;	
#endif	
			//batch_t *tmp = batch_new(BATCH_NEW_PARAMETER);	
			batch_t *tmp = batch_new(BATCH_NEW_PARAMETER, 1); //allocate smallest granularity if using mem pool	
			batch_update(tmp, tmp->start);
			dests[i] = (x_addr) tmp;
		}

	}else if(len_A == 0 && len_B != 0){
		
		/* case2: A is null, B is not null. copy data from B to dests*/
		for(auto i = 0u; i < n_outputs - 1; i++){
#if 0
			batch_t * tmp = (batch_t *) malloc(sizeof(batch_t));
			tmp->size = each_len_B * reclen;
			tmp->start = (int64_t *) malloc(sizeof(int64_t) * tmp->size);
			tmp->end = tmp->start + tmp->size;
#endif
			//batch_t *tmp = batch_new(BATCH_NEW_PARAMETER);	
			batch_t *tmp = batch_new(BATCH_NEW_PARAMETER, each_len_B * reclen * sizeof(simd_t));	
			batch_update(tmp, tmp->start + each_len_B * reclen);
			dests[i] = (x_addr) tmp;
			
			memcpy(tmp->start, idx_B, sizeof(int64_t)* tmp->size);
			idx_B = idx_B + tmp->size;	
		}
		/* last_len_B*/
#if 0
		batch_t * tmp = (batch_t *) malloc(sizeof(batch_t));
		tmp->size = last_len_B * reclen;
		tmp->start = (int64_t *) malloc(sizeof(int64_t) * tmp->size);	
		tmp->end = tmp->start + tmp->size;	
#endif
		//batch_t *tmp = batch_new(BATCH_NEW_PARAMETER);	
		batch_t *tmp = batch_new(BATCH_NEW_PARAMETER, last_len_B * reclen * sizeof(simd_t));	
		batch_update(tmp, tmp->start + last_len_B * reclen);
		dests[n_outputs - 1] = (x_addr) tmp;
		memcpy(tmp->start, idx_B, sizeof(int64_t)* tmp->size);
		
	}else if(len_A != 0 && len_B == 0){
		
		/* case3: A is not null, B is null. copy data from A to dests*/
		for(auto i = 0u; i < n_outputs - 1; i++){
#if 0
			batch_t * tmp = (batch_t *) malloc(sizeof(batch_t));
			tmp->size = each_len_A * reclen;
			tmp->start = (int64_t *) malloc(sizeof(int64_t) * tmp->size);
			tmp->end = tmp->start + tmp->size;	
#endif	
			//batch_t *tmp = batch_new(BATCH_NEW_PARAMETER);	
			batch_t *tmp = batch_new(BATCH_NEW_PARAMETER, each_len_A * reclen * sizeof(simd_t));	
			batch_update(tmp, tmp->start + each_len_A * reclen);
			
			dests[i] = (x_addr) tmp;
			memcpy(tmp->start, idx_A, sizeof(int64_t)* tmp->size);
			idx_A = idx_A + tmp->size;	
		}
		/* last_len_A*/
#if 0
		batch_t * tmp = (batch_t *) malloc(sizeof(batch_t));
		tmp->size = last_len_A * reclen;
		tmp->start = (int64_t *) malloc(sizeof(int64_t) * tmp->size);	
		tmp->end = tmp->start + tmp->size;	
#endif
		batch_t *tmp = batch_new(BATCH_NEW_PARAMETER, last_len_A * reclen * sizeof(simd_t));	
		batch_update(tmp, tmp->start + last_len_A * reclen);
		dests[n_outputs - 1] = (x_addr) tmp;
		memcpy(tmp->start, idx_A, sizeof(int64_t)* tmp->size);
	
	}else{
		
		/* case4: neith A or B is null. general case*/
		/* key and ptr */
/*
		int64_t *k_A = (int64_t *) malloc(sizeof(int64_t) * each_len_A);
		int64_t *ptr_A = (int64_t *) malloc(sizeof(int64_t) * each_len_A);
		int64_t *k_B = (int64_t *) malloc(sizeof(int64_t) * each_len_B);
		int64_t *ptr_B = (int64_t *) malloc(sizeof(int64_t) * each_len_B);
		int64_t *k_out = (int64_t *) malloc(sizeof(int64_t) * (each_len_A +each_len_B));
		int64_t *ptr_out = (int64_t *) malloc(sizeof(int64_t) * (each_len_A + each_len_B));
*/
		int64_t *k_A;
		int64_t *ptr_A;
		int64_t *k_B;
		int64_t *ptr_B;
		int64_t *k_out;
		int64_t *ptr_out;

		uint64_t each_buf_size_A = sizeof(int64_t) * each_len_A;
		uint64_t each_buf_size_B = sizeof(int64_t) * each_len_B;
		uint64_t each_buf_size_out = sizeof(int64_t) * (each_len_A + each_len_B);


		batch_t *b_k_A = batch_new(0, each_buf_size_A); 
		batch_t *b_ptr_A = batch_new(0, each_buf_size_A); 
		batch_t *b_k_B = batch_new(0, each_buf_size_B); 
		batch_t *b_ptr_B = batch_new(0, each_buf_size_B); 
		batch_t *b_k_out = batch_new(0, each_buf_size_out); 
		batch_t *b_ptr_out = batch_new(0, each_buf_size_out); 

		k_A = b_k_A->start;
		ptr_A = b_ptr_A->start;
		k_B = b_k_B->start;
		ptr_B = b_ptr_B->start;
		k_out = b_k_out->start;
		ptr_out = b_ptr_out->start;
/*
		if(each_buf_size_A <= BUFF_SIZE_1M){
			k_A = (int64_t *) get_bundle_slow_1M();	
			ptr_A = (int64_t *) get_bundle_slow_1M();	
		}else if(each_buf_size_A <= BUFF_SIZE_128M){
			k_A = (int64_t *) get_bundle_slow_128M();	
			ptr_A = (int64_t *) get_bundle_slow_128M();	
		}else{
			printf("unsupported buf_size!!!!!!!\n");
			abort();
		}

		if(each_buf_size_B <= BUFF_SIZE_1M){
			k_B = (int64_t *) get_bundle_slow_1M();	
			ptr_B = (int64_t *) get_bundle_slow_1M();	
		}else if(each_buf_size_A <= BUFF_SIZE_128M){
			k_B = (int64_t *) get_bundle_slow_128M();	
			ptr_B = (int64_t *) get_bundle_slow_128M();	
		}else{
			printf("unsupported buf_size!!!!!!!\n");
			abort();
		}

		if(each_buf_size_out <= BUFF_SIZE_1M){
			k_out = (int64_t *) get_bundle_slow_1M();	
			ptr_out = (int64_t *) get_bundle_slow_1M();	
		}else if(each_buf_size_out <= BUFF_SIZE_128M){
			k_out = (int64_t *) get_bundle_slow_128M();	
			ptr_out = (int64_t *) get_bundle_slow_128M();	
		}else{
			printf("unsupported buf_size!!!!!!!\n");
			abort();
		}
*/		
		/* merge A' and B's first (n_outputs - 1) partitions */
		//if both are 8 aligned call aliedn version, otherwise call unaligned	
		for(auto i = 0u; i < n_outputs - 1; i++){
#if 0
			batch_t * tmp = (batch_t *) malloc(sizeof(batch_t));
			tmp->size = (each_len_A + each_len_B) * reclen;
			tmp->start = (int64_t *) malloc(sizeof(int64_t) * tmp->size);
			tmp->end = tmp->start + tmp->size;	
#endif
			batch_t *tmp = batch_new(BATCH_NEW_PARAMETER, (each_len_A + each_len_B) * reclen * sizeof(simd_t));	
			batch_update(tmp, tmp->start + (each_len_A + each_len_B) * reclen);
			dests[i] = (x_addr) tmp;

			for(uint64_t i = 0; i < each_len_A; i++){
				k_A[i] = *(idx_A + keypos);
				ptr_A[i] = (int64_t)idx_A;
				idx_A = idx_A + reclen;
			}

			for(uint64_t i = 0; i < each_len_B; i++){
				k_B[i] = *(idx_B + keypos);
				ptr_B[i] = (int64_t)idx_B;
				idx_B = idx_B + reclen;
			}

			assert(each_len_A && each_len_B);
			if(each_len_A%8 == 0 && each_len_B%8 == 0){
				x2_merge_8_aligned(k_A, ptr_A, k_B, ptr_B, k_out, ptr_out, each_len_A, each_len_B);				
			}else{
				x2_merge_8_unaligned(k_A, ptr_A, k_B, ptr_B, k_out, ptr_out, each_len_A, each_len_B);				
			}

			/* store records according to ptr_out */
			store = tmp->start;
			for(uint64_t j = 0; j < each_len_A + each_len_B; j++){
				for(uint64_t k = 0; k < reclen; k++){
					store[k] = *((int64_t *)(ptr_out[j]) + k); 
				}
				store = store + reclen;
			}
		}	

		/* merge A's last partition with B's last partition */
		assert(last_len_A && last_len_B);
		//if(last_len_A && last_len_B){
/*
		int64_t *last_k_A = (int64_t *) malloc(sizeof(int64_t) * last_len_A);
		int64_t *last_ptr_A = (int64_t *) malloc(sizeof(int64_t) * last_len_A);
		int64_t *last_k_B = (int64_t *) malloc(sizeof(int64_t) * last_len_B);
		int64_t *last_ptr_B = (int64_t *) malloc(sizeof(int64_t) * last_len_B);
		int64_t *last_k_out = (int64_t *) malloc(sizeof(int64_t) * (last_len_A + last_len_B));
		int64_t *last_ptr_out = (int64_t *) malloc(sizeof(int64_t) * (last_len_A + last_len_B));
*/
		int64_t *last_k_A;
		int64_t *last_ptr_A;
		int64_t *last_k_B;
		int64_t *last_ptr_B;
		int64_t *last_k_out;
		int64_t *last_ptr_out;
		
		uint64_t last_buf_size_A = sizeof(int64_t) * last_len_A;
		uint64_t last_buf_size_B = sizeof(int64_t) * last_len_B;
		uint64_t last_buf_size_out = sizeof(int64_t) * (last_len_A + last_len_B);

		batch_t *b_last_k_A = batch_new(0, last_buf_size_A); 
		batch_t *b_last_ptr_A = batch_new(0, last_buf_size_A); 
		batch_t *b_last_k_B = batch_new(0, last_buf_size_B); 
		batch_t *b_last_ptr_B = batch_new(0, last_buf_size_B); 
		batch_t *b_last_k_out = batch_new(0, last_buf_size_out); 
		batch_t *b_last_ptr_out = batch_new(0, last_buf_size_out); 

		last_k_A = b_last_k_A->start;
		last_ptr_A = b_last_ptr_A->start;
		last_k_B = b_last_k_B->start;
		last_ptr_B = b_last_ptr_B->start;
		last_k_out = b_last_k_out->start;
		last_ptr_out = b_last_ptr_out->start;
/*
		if(last_buf_size_A <= BUFF_SIZE_1M){
			last_k_A = (int64_t *) get_bundle_slow_1M();	
			last_ptr_A = (int64_t *) get_bundle_slow_1M();	
		}else if(last_buf_size_A <= BUFF_SIZE_128M){
			last_k_A = (int64_t *) get_bundle_slow_128M();	
			last_ptr_A = (int64_t *) get_bundle_slow_128M();	
		}else{
			printf("unsupported buf_size!!!!!!!\n");
			abort();
		}

		if(last_buf_size_B <= BUFF_SIZE_1M){
			last_k_B = (int64_t *) get_bundle_slow_1M();	
			last_ptr_B = (int64_t *) get_bundle_slow_1M();	
		}else if(last_buf_size_A <= BUFF_SIZE_128M){
			last_k_B = (int64_t *) get_bundle_slow_128M();	
			last_ptr_B = (int64_t *) get_bundle_slow_128M();	
		}else{
			printf("unsupported buf_size!!!!!!!\n");
			abort();
		}

		if(last_buf_size_out <= BUFF_SIZE_1M){
			last_k_out = (int64_t *) get_bundle_slow_1M();	
			last_ptr_out = (int64_t *) get_bundle_slow_1M();	
		}else if(last_buf_size_out <= BUFF_SIZE_128M){
			last_k_out = (int64_t *) get_bundle_slow_128M();	
			last_ptr_out = (int64_t *) get_bundle_slow_128M();	
		}else{
			printf("unsupported buf_size!!!!!!!\n");
			abort();
		}
*/

#if 0
		batch_t * tmp = (batch_t *) malloc(sizeof(batch_t));
		tmp->size = (last_len_A + last_len_B) * reclen;
		tmp->start = (int64_t *) malloc(sizeof(int64_t) * tmp->size);
		tmp->end = tmp->start + tmp->size;	
#endif
		batch_t *tmp = batch_new(BATCH_NEW_PARAMETER, (last_len_A + last_len_B) * reclen * sizeof(simd_t));	
		batch_update(tmp, tmp->start + (last_len_A + last_len_B) * reclen);
		dests[n_outputs - 1] = (x_addr) tmp;

		for(uint64_t i = 0; i < last_len_A; i++){
			last_k_A[i] = *(idx_A + keypos);
			last_ptr_A[i] = (int64_t)idx_A;
			idx_A = idx_A + reclen;
		}

		for(uint64_t i = 0; i < last_len_B; i++){
			last_k_B[i] = *(idx_B + keypos);
			last_ptr_B[i] = (int64_t)idx_B;
			idx_B = idx_B + reclen;
		}

		if(last_len_A%8 == 0 && last_len_B%8 == 0){
			x2_merge_8_aligned(last_k_A, last_ptr_A, last_k_B, last_ptr_B, last_k_out, last_ptr_out, last_len_A, last_len_B);				
		}else{
			x2_merge_8_unaligned(last_k_A, last_ptr_A, last_k_B, last_ptr_B, last_k_out, last_ptr_out, last_len_A, last_len_B);				
		}

		/* store records according to ptr_out */
		store = tmp->start;
		for(uint64_t j = 0; j < last_len_A + last_len_B; j++){
			for(uint64_t k = 0; k < reclen; k++){
				store[k] = *((int64_t *)(last_ptr_out[j]) + k); 
			}
			store = store + reclen;
		}
/*
		free(last_k_A);
		free(last_ptr_A);
		free(last_k_B);
		free(last_ptr_B);
		free(last_k_out);
		free(last_ptr_out);
		
		free(k_A);
		free(ptr_A);
		free(k_B);
		free(ptr_B);
		free(k_out);
		free(ptr_out);
*/
		BATCH_KILL(b_last_k_A, 0);
		BATCH_KILL(b_last_ptr_A, 0);
		BATCH_KILL(b_last_k_B, 0);
		BATCH_KILL(b_last_ptr_B, 0);
		BATCH_KILL(b_last_k_out, 0);
		BATCH_KILL(b_last_ptr_out, 0);
		
		BATCH_KILL(b_k_A, 0);
		BATCH_KILL(b_ptr_A, 0);
		BATCH_KILL(b_k_B, 0);
		BATCH_KILL(b_ptr_B, 0);
		BATCH_KILL(b_k_out, 0);
		BATCH_KILL(b_ptr_out, 0);
/*
		if(each_buf_size_A <= BUFF_SIZE_1M){
			return_bundle_slow_1M(k_A);	
			return_bundle_slow_1M(ptr_A);	
		}else if(each_buf_size_A <= BUFF_SIZE_128M){
			return_bundle_slow_128M(k_A);	
			return_bundle_slow_128M(ptr_A);	
		}else{
			printf("unsupported buf_size!!!!!!!\n");
			abort();
		}

		if(each_buf_size_B <= BUFF_SIZE_1M){
			return_bundle_slow_1M(k_B);	
			return_bundle_slow_1M(ptr_B);	
		}else if(each_buf_size_A <= BUFF_SIZE_128M){
			return_bundle_slow_128M(k_B);	
			return_bundle_slow_128M(ptr_B);	
		}else{
			printf("unsupported buf_size!!!!!!!\n");
			abort();
		}

		if(each_buf_size_out <= BUFF_SIZE_1M){
			return_bundle_slow_1M(k_out);	
			return_bundle_slow_1M(ptr_out);	
		}else if(each_buf_size_out <= BUFF_SIZE_128M){
			return_bundle_slow_128M(k_out);	
			return_bundle_slow_128M(ptr_out);	
		}else{
			printf("unsupported buf_size!!!!!!!\n");
			abort();
		}
		
		if(last_buf_size_A <= BUFF_SIZE_1M){
			return_bundle_slow_1M(last_k_A);	
			return_bundle_slow_1M(last_ptr_A);	
		}else if(last_buf_size_A <= BUFF_SIZE_128M){
			return_bundle_slow_128M(last_k_A);	
			return_bundle_slow_128M(last_ptr_A);	
		}else{
			printf("unsupported buf_size!!!!!!!\n");
			abort();
		}

		if(last_buf_size_B <= BUFF_SIZE_1M){
			return_bundle_slow_1M(last_k_B);	
			return_bundle_slow_1M(last_ptr_B);	
		}else if(last_buf_size_A <= BUFF_SIZE_128M){
			return_bundle_slow_128M(last_k_B);	
			return_bundle_slow_128M(last_ptr_B);	
		}else{
			printf("unsupported buf_size!!!!!!!\n");
			abort();
		}

		if(last_buf_size_out <= BUFF_SIZE_1M){
			return_bundle_slow_1M(last_k_out);	
			return_bundle_slow_1M(last_ptr_out);	
		}else if(last_buf_size_out <= BUFF_SIZE_128M){
			return_bundle_slow_128M(last_k_out);	
			return_bundle_slow_128M(last_ptr_out);	
		}else{
			printf("unsupported buf_size!!!!!!!\n");
			abort();
		}
*/
		/*XXX Shall we release srca and srcb ??? */

	}
	/*XXX what to return? */
	return 0;
}

int32_t merge_kp(x_addr *dests_k, x_addr *dests_p, uint32_t n_outputs, x_addr srcA_k, x_addr srcA_p, x_addr srcB_k, x_addr srcB_p){ 

	batch_t *src_A_k = (batch_t *) srcA_k;
	batch_t *src_A_p = (batch_t *) srcA_p;
	batch_t *src_B_k = (batch_t *) srcB_k;
	batch_t *src_B_p = (batch_t *) srcB_p;
	
	assert(src_A_k->size == src_A_p->size);	
	assert(src_B_k->size == src_B_p->size);	
	
	uint64_t len_A = src_A_k->size; // # keys in A
	uint64_t len_B = src_B_k->size; // # keys in B
	
	int64_t * start_A_k = src_A_k->start;
	int64_t * start_A_p = src_A_p->start;
	int64_t * start_B_k = src_B_k->start;
	int64_t * start_B_p = src_B_p->start;
	
	/* equally partition A and B */
	uint64_t each_len_A = len_A/n_outputs;
	uint64_t last_len_A = len_A - (n_outputs - 1)*each_len_A;
	uint64_t each_len_B = len_B/n_outputs;
	uint64_t last_len_B = len_B - (n_outputs - 1)*each_len_B;
	
	if(len_A == 0 && len_B == 0){
		/* case1: both of A and B are null, which is possible. Just allocate dests and return it.*/
		for(auto i = 0u; i < n_outputs; i++){
			batch_t *tmp_k = batch_new(BATCH_NEW_PARAMETER, 1); //allocate smallest granularity if using mem pool	
			batch_t *tmp_p = batch_new(BATCH_NEW_PARAMETER, 1); //allocate smallest granularity if using mem pool	
			
			batch_update(tmp_k, tmp_k->start);
			batch_update(tmp_p, tmp_p->start);
			
			dests_k[i] = (x_addr) tmp_k;
			dests_p[i] = (x_addr) tmp_p;
		}

	}else if(len_A == 0 && len_B != 0){
		
		/* case2: A is null, B is not null. copy data from B to dests*/
		for(auto i = 0u; i < n_outputs - 1; i++){
			batch_t *tmp_k = batch_new(BATCH_NEW_PARAMETER, each_len_B * sizeof(simd_t));	
			batch_t *tmp_p = batch_new(BATCH_NEW_PARAMETER, each_len_B * sizeof(simd_t));	
			
			batch_update(tmp_k, tmp_k->start + each_len_B);
			batch_update(tmp_p, tmp_p->start + each_len_B);
			
			dests_k[i] = (x_addr) tmp_k;
			dests_p[i] = (x_addr) tmp_p;
			
			memcpy(tmp_k->start, start_B_k, sizeof(simd_t)* tmp_k->size);
			memcpy(tmp_p->start, start_B_p, sizeof(simd_t)* tmp_p->size);
			
			start_B_k = start_B_k + each_len_B;
			start_B_p = start_B_p + each_len_B;
		}
		
		/* last_len_B*/
		batch_t *tmp_k = batch_new(BATCH_NEW_PARAMETER, last_len_B * sizeof(simd_t));	
		batch_t *tmp_p = batch_new(BATCH_NEW_PARAMETER, last_len_B * sizeof(simd_t));	
		
		batch_update(tmp_k, tmp_k->start + last_len_B);
		batch_update(tmp_p, tmp_p->start + last_len_B);
		
		dests_k[n_outputs - 1] = (x_addr) tmp_k;
		dests_p[n_outputs - 1] = (x_addr) tmp_p;
		
		memcpy(tmp_k->start, start_B_k, sizeof(int64_t)* tmp_k->size);
		memcpy(tmp_p->start, start_B_p, sizeof(int64_t)* tmp_p->size);
		
	}else if(len_A != 0 && len_B == 0){
		
		/* case3: A is not null, B is null. copy data from A to dests*/
		for(auto i = 0u; i < n_outputs - 1; i++){
			batch_t *tmp_k = batch_new(BATCH_NEW_PARAMETER, each_len_A * sizeof(simd_t));	
			batch_t *tmp_p = batch_new(BATCH_NEW_PARAMETER, each_len_A * sizeof(simd_t));	
			
			batch_update(tmp_k, tmp_k->start + each_len_A);
			batch_update(tmp_p, tmp_p->start + each_len_A);
			
			dests_k[i] = (x_addr) tmp_k;
			dests_p[i] = (x_addr) tmp_p;
			
			memcpy(tmp_k->start, start_A_k, sizeof(simd_t)* tmp_k->size);
			memcpy(tmp_p->start, start_A_p, sizeof(simd_t)* tmp_p->size);
			
			start_A_k = start_A_k + each_len_A;
			start_A_p = start_A_p + each_len_A;
		}
		
		/* last_len_B*/
		batch_t *tmp_k = batch_new(BATCH_NEW_PARAMETER, last_len_A * sizeof(simd_t));	
		batch_t *tmp_p = batch_new(BATCH_NEW_PARAMETER, last_len_A * sizeof(simd_t));	
		
		batch_update(tmp_k, tmp_k->start + last_len_A);
		batch_update(tmp_p, tmp_p->start + last_len_A);
		
		dests_k[n_outputs - 1] = (x_addr) tmp_k;
		dests_p[n_outputs - 1] = (x_addr) tmp_p;
		
		memcpy(tmp_k->start, start_A_k, sizeof(int64_t)* tmp_k->size);
		memcpy(tmp_p->start, start_A_p, sizeof(int64_t)* tmp_p->size);
	
	}else{
		
		/* case4: neith A or B is null. general case*/
		/* key and ptr */
		int64_t *k_A = src_A_k->start;
		int64_t *ptr_A = src_A_p->start;
		int64_t *k_B = src_B_k->start;
		int64_t *ptr_B = src_B_p->start;
		int64_t *k_out;
		int64_t *ptr_out;

		/* merge A' and B's first (n_outputs - 1) partitions */
		//if both are 8 aligned call aliedn version, otherwise call unaligned	
		for(auto i = 0u; i < n_outputs - 1; i++){
			batch_t *tmp_k = batch_new(BATCH_NEW_PARAMETER, (each_len_A + each_len_B) * sizeof(simd_t));	
			batch_t *tmp_p = batch_new(BATCH_NEW_PARAMETER, (each_len_A + each_len_B) * sizeof(simd_t));	
			
			batch_update(tmp_k, tmp_k->start + (each_len_A + each_len_B));
			batch_update(tmp_p, tmp_p->start + (each_len_A + each_len_B));
			
			dests_k[i] = (x_addr) tmp_k;
			dests_p[i] = (x_addr) tmp_p;

			k_out = tmp_k->start;
			ptr_out = tmp_p->start;

			assert(each_len_A && each_len_B);
			if(each_len_A%8 == 0 && each_len_B%8 == 0){
				x2_merge_8_aligned(k_A, ptr_A, k_B, ptr_B, k_out, ptr_out, each_len_A, each_len_B);				
			}else{
				x2_merge_8_unaligned(k_A, ptr_A, k_B, ptr_B, k_out, ptr_out, each_len_A, each_len_B);				
			}

			k_A = k_A + each_len_A;
			ptr_A = ptr_A + each_len_A;
			k_B = k_B + each_len_B;
			ptr_B = ptr_B + each_len_B;
		}	

		/* merge A's last partition with B's last partition */
		assert(last_len_A && last_len_B);
		
		batch_t *tmp_k = batch_new(BATCH_NEW_PARAMETER, (last_len_A + last_len_B) * sizeof(simd_t));	
		batch_t *tmp_p = batch_new(BATCH_NEW_PARAMETER, (last_len_A + last_len_B) * sizeof(simd_t));	
		
		batch_update(tmp_k, tmp_k->start + (last_len_A + last_len_B));
		batch_update(tmp_p, tmp_p->start + (last_len_A + last_len_B));
		
		dests_k[n_outputs - 1] = (x_addr) tmp_k;
		dests_p[n_outputs - 1] = (x_addr) tmp_p;
		
		k_out = tmp_k->start;
		ptr_out = tmp_p->start;

		if(last_len_A%8 == 0 && last_len_B%8 == 0){
			x2_merge_8_aligned(k_A, ptr_A, k_B, ptr_B, k_out, ptr_out, last_len_A, last_len_B);				
		}else{
			x2_merge_8_unaligned(k_A, ptr_A, k_B, ptr_B, k_out, ptr_out, last_len_A, last_len_B);				
		}
	}
	/*XXX what to return? */
	return 0;
}


/* @vpos: alrady sorted pos */
int32_t sort_stable(x_addr *dests, uint32_t n_outputs, x_addr src, idx_t keypos, idx_t vpos, idx_t reclen){
	batch_t *src_in = (batch_t *) src;
	uint64_t len = src_in->size/reclen; // # of records
	
	/* first (n_outputs - 1) partition have the same len */	
	uint64_t each_len = len/n_outputs;
	/* last partition may has different len with previous partitions' len */
	uint64_t last_len = len - (n_outputs - 1)*each_len;
	
	int64_t* idx = src_in->start;

	/* sort first (n_outputs - 1) parts*/
	for(auto i = 0u; i < n_outputs - 1; i++){
#if 0
		batch_t * tmp = (batch_t *) malloc(sizeof(batch_t));
		tmp->size = each_len * reclen;
		tmp->start = (int64_t *) malloc(sizeof(int64_t) * tmp->size);
		tmp->end = tmp->start + tmp->size;	
#endif	
		batch_t *tmp = batch_new(BATCH_NEW_PARAMETER, each_len * reclen * sizeof(simd_t));	
		batch_update(tmp, tmp->start + each_len * reclen);
		
		dests[i] = (x_addr) tmp;
		memcpy(tmp->start, idx, sizeof(int64_t)*tmp->size);
		idx = idx + tmp->size;

		switch(keypos){
			case 0: /* assume sorted by key 1 */
				switch(vpos){
					case 1:
						qsort(tmp->start, each_len / reclen,
								sizeof(int64_t) * reclen, sort0_sorted1);
						break;
					case 2:
						qsort(tmp->start, each_len / reclen,
								sizeof(int64_t) * reclen, sort0_sorted2);
						break;
					case 3:
						qsort(tmp->start, each_len / reclen,
								sizeof(int64_t) * reclen, sort0_sorted3);
						break;
					default:
						//return -X_ERR_POS;
						printf("wrong vpos");
						abort();
				}
				break;
			case 1:
				switch(vpos){
					case 0:
						qsort(tmp->start, each_len / reclen,
								sizeof(int64_t) * reclen, sort1_sorted0);
						break;
					case 2:
						qsort(tmp->start, each_len / reclen,
								sizeof(int64_t) * reclen, sort1_sorted2);
						break;
					case 3:
						qsort(tmp->start, each_len / reclen,
								sizeof(int64_t) * reclen, sort1_sorted3);
						break;
					default:
						//return -X_ERR_POS;
						printf("wrong vpos");
						abort();
				}
				break;
			case 2:	/* assume sorted by key 1 */
				switch(vpos){
					case 0:
						qsort(tmp->start, each_len / reclen,
								sizeof(int64_t) * reclen, sort2_sorted0);
						break;
					case 1:
						qsort(tmp->start, each_len / reclen,
								sizeof(int64_t) * reclen, sort2_sorted1);
						break;
					case 3:
						qsort(tmp->start, each_len / reclen,
								sizeof(int64_t) * reclen, sort2_sorted3);
						break;
					default:
						//return -X_ERR_POS;
						printf("wrong vpos");
						abort();
				}
				break;
			case 3:	/* assume sorted by key 1 */
				switch(vpos){
					case 0:
						qsort(tmp->start, each_len / reclen,
								sizeof(int64_t) * reclen, sort3_sorted0);
						break;
					case 1:
						qsort(tmp->start, each_len / reclen,
								sizeof(int64_t) * reclen, sort3_sorted1);
						break;
					case 2:
						qsort(tmp->start, each_len / reclen,
								sizeof(int64_t) * reclen, sort3_sorted2);
						break;
					default:
						//return -X_ERR_POS;
						printf("wrong vpos");
						abort();
				}
				break;
			default:
				//return -X_ERR_POS;
				/* wrong vpos*/
				printf("wrong keypos");
				abort();
		}
	}
	
	
	/* sort last part */
#if 0
	batch_t * tmp = (batch_t *) malloc(sizeof(batch_t));
	tmp->size = last_len * reclen;
	tmp->start = (int64_t *) malloc(sizeof(int64_t) * tmp->size);
	tmp->end = tmp->start + tmp->size;	
#endif
	batch_t *tmp = batch_new(BATCH_NEW_PARAMETER, last_len * reclen * sizeof(simd_t));	
	batch_update(tmp, tmp->start + last_len * reclen);
	
	dests[n_outputs - 1] = (x_addr) tmp;
	memcpy(tmp->start, idx, sizeof(int64_t)*tmp->size);
	idx = idx + tmp->size;

	switch(keypos){
		case 0: /* assume sorted by key 1 */
			switch(vpos){
				case 1:
					qsort(tmp->start, last_len / reclen,
							sizeof(int64_t) * reclen, sort0_sorted1);
					break;
				case 2:
					qsort(tmp->start, last_len / reclen,
							sizeof(int64_t) * reclen, sort0_sorted2);
					break;
				case 3:
					qsort(tmp->start, last_len / reclen,
							sizeof(int64_t) * reclen, sort0_sorted3);
					break;
				default:
					//return -X_ERR_POS;
					printf("wrong vpos");
					abort();
			}
			break;
		case 1:
			switch(vpos){
				case 0:
					qsort(tmp->start, last_len / reclen,
							sizeof(int64_t) * reclen, sort1_sorted0);
					break;
				case 2:
					qsort(tmp->start, last_len / reclen,
							sizeof(int64_t) * reclen, sort1_sorted2);
					break;
				case 3:
					qsort(tmp->start, last_len / reclen,
							sizeof(int64_t) * reclen, sort1_sorted3);
					break;
				default:
					//return -X_ERR_POS;
					printf("wrong vpos");
					abort();
			}
			break;
		case 2:	/* assume sorted by key 1 */
			switch(vpos){
				case 0:
					qsort(tmp->start, last_len / reclen,
							sizeof(int64_t) * reclen, sort2_sorted0);
					break;
				case 1:
					qsort(tmp->start, last_len / reclen,
							sizeof(int64_t) * reclen, sort2_sorted1);
					break;
				case 3:
					qsort(tmp->start, last_len / reclen,
							sizeof(int64_t) * reclen, sort2_sorted3);
					break;
				default:
					//return -X_ERR_POS;
					printf("wrong vpos");
					abort();
			}
			break;
		case 3:	/* assume sorted by key 1 */
			switch(vpos){
				case 0:
					qsort(tmp->start, last_len / reclen,
							sizeof(int64_t) * reclen, sort3_sorted0);
					break;
				case 1:
					qsort(tmp->start, last_len / reclen,
							sizeof(int64_t) * reclen, sort3_sorted1);
					break;
				case 2:
					qsort(tmp->start, last_len / reclen,
							sizeof(int64_t) * reclen, sort3_sorted2);
					break;
				default:
					//return -X_ERR_POS;
					printf("wrong vpos");
					abort();
			}
			break;
		default:
			//return -X_ERR_POS;
			/* wrong vpos*/
			printf("wrong keypos");
			abort();
	}
	return 0;
}

/*
 * Spec: recursively search key in data(records) between offset l and r
 * Return:
 * 	1) if key exists: return the offset
 * 	2) if key doesn't exist: 
 * 		a) if key > all:  return r (the right end of the range) 
 *		b) if key < all:  return r (r should be smaller than l, e.g. if l is 0, then r should be a negative vale: - reclen) 
 * 		c) else: return r (r should points to the value who is the largest one that smaller than key)
 * */
/*
 *1) inital_l <= r <= initial_r
 *2) r < inital_l
 *    -- key < all keys 
 *3) r = inital_r is okay because:
 *   data[r] = key: r is the possion
 *   data[r] < key: key > all keys... r is the position
*/
int binary_search(simd_t *data, int l, int r, simd_t key, idx_t keypos, idx_t reclen){
	if(r >= l){
		int mid = l + (((r - l)/reclen)/2)*reclen;
		if(data[mid + keypos] == key){
			return mid;
		}
		if(data[mid + keypos] > key){
			return binary_search(data, l, mid - reclen, key, keypos, reclen);
		}
		return binary_search(data, mid + reclen, r, key, keypos, reclen);
	}
	return r;
}


/* Spec: split @src_a and @src_b to N partitions, so we can use N threads to merge these small partitions
 * @src_a: first batch's address
 * @offset_a[]: stores the start offset of each partition of src_a
 * @len_a[]: stores the len of each partition of src_a 
 * @src_b: the other batch's address 
 * @offset_b[]: stores the start offset of each partition of src_b,(offset of simd_t, not offset of record)
 * @len_b[]: stores the len of each partition of src_b 
 * @n_partitions: # of partitions, or # of threads to merge src_a and src_b
 */

/* Actually, we can equally partition src_a
 * then find the keys at the begining of each partition
 * then find the positions of these keys in src_b
 */
void split_batch(x_addr src_a, uint32_t* offset_a, uint32_t* len_a, 
	         x_addr src_b, uint32_t* offset_b, uint32_t * len_b, 
		 uint32_t n_parts, idx_t keypos, idx_t reclen){
	batch_t *src_A = (batch_t *) src_a;	
	batch_t *src_B = (batch_t *) src_b;
	
	simd_t* start_A = src_A->start;
	simd_t* start_B = src_B->start;

	uint32_t len_A = src_A->size/reclen; // #records in A
	uint32_t len_B = src_B->size/reclen; // #records in B
	//printf("len_A is %d, len_B is %d\n", len_A, len_B);
	
	/* 1. set src_a's offset_a[] and len_a[]*/
	uint32_t each_len_A = len_A / n_parts;
	uint32_t last_len_A = len_A - (n_parts - 1) * each_len_A;
	//printf("each_len_A is %d, last_len_A is %d\n", each_len_A, last_len_A);
	
	uint32_t i;
	for(i = 0; i < n_parts - 1; i++){
		offset_a[i] = i * each_len_A * reclen;
		len_a[i] = each_len_A * reclen;
	}
	
	offset_a[i] = i * each_len_A * reclen;
	len_a[i] = last_len_A * reclen;

	/* 2. set src_b's offset_b[] and len_b[] */
	int pos = 0;
	simd_t key;
	offset_b[0] = 0;
 	int pos_l = 0;
	int pos_r = len_B * reclen;

	for(i = 1; i < n_parts; i++){
		key = *(start_A + offset_a[i] + keypos); 	
		pos = binary_search(start_B, pos_l, pos_r, key, keypos, reclen);
		//printf("pos_l is %d, pos_r is %d, pos is %d\n", pos_l, pos_r, pos);
		if(pos < pos_l){
			len_b[i-1] = 0;
			offset_b[i] = pos_l;
		}else{
			len_b[i-1] = pos - pos_l;
			pos_l = pos;
			offset_b[i] = pos_l;
		}
	}
	//printf("pos_l is %d, pos_r is %d, pos is %d\n", pos_l, pos_r, pos);
	if(pos < pos_l){
		//all values in B are lager than all avalues in A
		// pos will be a minus value
		len_b[n_parts-1] = pos_r - pos_l;
	}else{
		len_b[n_parts-1] = pos_r - pos;
	}
}


/*
 * Spec: merge records starting from offset_a in src_a and records starting from offset_b in src_b to merge_dest starting from merge_offset 
 * @src_a: address of batch a
 * @offset_a: offset of starting point in src_a
 * @len_a: len of simd_t to be merged in src_a (simd_t granularity, not record)
 * @offset_b: offset of starting point in src_b
 * @len_b: len of simd_t to be merged in src_b (simd_t granularity, not record)
 * @merge_offset: should be pre-calculated
 * @keypos: key offset in a record
 * @reclen: # simd_t in a record
 */
void merge_one_part(x_addr src_a, uint32_t offset_a, uint32_t len_a,
		    x_addr src_b, uint32_t offset_b, uint32_t len_b,
		    x_addr merge_dest,  uint32_t merge_offset,
		    idx_t keypos, idx_t reclen){

	//printf("---offset_a is %d, len_a is %d, offset_b is %d, len_b is %d\n", offset_a, len_a, offset_b, len_b);	
	batch_t *src_A = (batch_t *) src_a;
	batch_t *src_B = (batch_t *) src_b;
	
	int64_t * start_A = src_A->start + offset_a;
	int64_t * start_B = src_B->start + offset_b;
	
	uint32_t len_A = len_a/reclen; // # records in A
	uint32_t len_B = len_b/reclen; //# records in B
	
	batch_t *dest = (batch_t *) merge_dest;
	int64_t *start_dest = dest->start + merge_offset;
		
	if(len_A == 0 && len_B == 0){
		/*do nothing*/
		return;
	}else if(len_A == 0 && len_B != 0){
		/* copy src_b to merge_dest*/
		memcpy(start_dest, start_B, sizeof(simd_t)* len_b);
	}else if(len_A != 0 && len_B == 0){
		/* copy src_a merge_dest*/
		memcpy(start_dest, start_A, sizeof(simd_t)* len_a);
	}else{
		/* merge */
/*
		int64_t *k_A = (int64_t *) malloc(sizeof(int64_t) * len_A);
		int64_t *ptr_A = (int64_t *) malloc(sizeof(int64_t) * len_A);
		int64_t *k_B = (int64_t *) malloc(sizeof(int64_t) * len_B);
		int64_t *ptr_B = (int64_t *) malloc(sizeof(int64_t) * len_B);
		int64_t *k_out = (int64_t *) malloc(sizeof(int64_t) * (len_A + len_B));
		int64_t *ptr_out = (int64_t *) malloc(sizeof(int64_t) * (len_A + len_B));
*/
		int64_t *k_A;
		int64_t *ptr_A;
		int64_t *k_B;
		int64_t *ptr_B;
		int64_t *k_out;
		int64_t *ptr_out;

		uint64_t each_buf_size_A = sizeof(int64_t) * len_A;
		uint64_t each_buf_size_B = sizeof(int64_t) * len_B;
		uint64_t each_buf_size_out = sizeof(int64_t) * (len_A + len_B);
		
		batch_t *b_k_A = batch_new(0, each_buf_size_A); 
		batch_t *b_ptr_A = batch_new(0, each_buf_size_A); 
		batch_t *b_k_B = batch_new(0, each_buf_size_B); 
		batch_t *b_ptr_B = batch_new(0, each_buf_size_B); 
		batch_t *b_k_out = batch_new(0, each_buf_size_out); 
		batch_t *b_ptr_out = batch_new(0, each_buf_size_out); 

		k_A = b_k_A->start;
		ptr_A = b_ptr_A->start;
		k_B = b_k_B->start;
		ptr_B = b_ptr_B->start;
		k_out = b_k_out->start;
		ptr_out = b_ptr_out->start;

/*
		if(each_buf_size_A <= BUFF_SIZE_1M){
			k_A = (int64_t *) get_bundle_slow_1M();	
			ptr_A = (int64_t *) get_bundle_slow_1M();	
		}else if(each_buf_size_A <= BUFF_SIZE_128M){
			k_A = (int64_t *) get_bundle_slow_128M();	
			ptr_A = (int64_t *) get_bundle_slow_128M();	
		}else{
			printf("unsupported buf_size!!!!!!!\n");
			abort();
		}

		if(each_buf_size_B <= BUFF_SIZE_1M){
			k_B = (int64_t *) get_bundle_slow_1M();	
			ptr_B = (int64_t *) get_bundle_slow_1M();	
		}else if(each_buf_size_A <= BUFF_SIZE_128M){
			k_B = (int64_t *) get_bundle_slow_128M();	
			ptr_B = (int64_t *) get_bundle_slow_128M();	
		}else{
			printf("unsupported buf_size!!!!!!!\n");
			abort();
		}

		if(each_buf_size_out <= BUFF_SIZE_1M){
			k_out = (int64_t *) get_bundle_slow_1M();	
			ptr_out = (int64_t *) get_bundle_slow_1M();	
		}else if(each_buf_size_out <= BUFF_SIZE_128M){
			k_out = (int64_t *) get_bundle_slow_128M();	
			ptr_out = (int64_t *) get_bundle_slow_128M();	
		}else{
			printf("unsupported buf_size!!!!!!!\n");
			abort();
		}
*/


		int64_t * idx_A = start_A; 
		int64_t * idx_B = start_B; 
		
		for(uint32_t i = 0; i < len_A; i++){
			k_A[i] = *(idx_A + keypos);
			ptr_A[i] = (int64_t)idx_A;
			idx_A = idx_A + reclen;
		}

		for(uint32_t i = 0; i < len_B; i++){
			k_B[i] = *(idx_B + keypos);
			ptr_B[i] = (int64_t)idx_B;
			idx_B = idx_B + reclen;
		}
		
		if(len_A%8 == 0 && len_B%8 == 0){
			x2_merge_8_aligned(k_A, ptr_A, k_B, ptr_B, k_out, ptr_out, len_A, len_B);				
		}else{
			x2_merge_8_unaligned(k_A, ptr_A, k_B, ptr_B, k_out, ptr_out, len_A, len_B);				
		}

		int64_t * store = start_dest; 
		for(uint32_t j = 0; j < len_A + len_B; j++){
			for(uint64_t k = 0; k < reclen; k++){
				store[k] = *((int64_t *)(ptr_out[j]) + k); 
			}
			store = store + reclen;
		}
/*		
		free(k_A);
		free(ptr_A);
		free(k_B);
		free(ptr_B);
		free(k_out);
		free(ptr_out);
*/	
		BATCH_KILL(b_k_A, 0);
		BATCH_KILL(b_ptr_A, 0);
		BATCH_KILL(b_k_B, 0);
		BATCH_KILL(b_ptr_B, 0);
		BATCH_KILL(b_k_out, 0);
		BATCH_KILL(b_ptr_out, 0);
/*
		if(each_buf_size_A <= BUFF_SIZE_1M){
			return_bundle_slow_1M(k_A);	
			return_bundle_slow_1M(ptr_A);	
		}else if(each_buf_size_A <= BUFF_SIZE_128M){
			return_bundle_slow_128M(k_A);	
			return_bundle_slow_128M(ptr_A);	
		}else{
			printf("unsupported buf_size!!!!!!!\n");
			abort();
		}

		if(each_buf_size_B <= BUFF_SIZE_1M){
			return_bundle_slow_1M(k_B);	
			return_bundle_slow_1M(ptr_B);	
		}else if(each_buf_size_A <= BUFF_SIZE_128M){
			return_bundle_slow_128M(k_B);	
			return_bundle_slow_128M(ptr_B);	
		}else{
			printf("unsupported buf_size!!!!!!!\n");
			abort();
		}

		if(each_buf_size_out <= BUFF_SIZE_1M){
			return_bundle_slow_1M(k_out);	
			return_bundle_slow_1M(ptr_out);	
		}else if(each_buf_size_out <= BUFF_SIZE_128M){
			return_bundle_slow_128M(k_out);	
			return_bundle_slow_128M(ptr_out);	
		}else{
			printf("unsupported buf_size!!!!!!!\n");
			abort();
		}
*/
	}
}


/*
 * Spec: calcualte the offset of each partition in merge_batch
 */
void calculate_offset(uint32_t* len_a, uint32_t* len_b, uint32_t* offset, uint32_t n_parts, idx_t reclen){
	offset[0] = 0;
	for(uint32_t i = 1; i < n_parts; i++){
		offset[i] = offset[i-1] + (len_a[i-1] + len_b[i-1]);
	}
}

#endif
