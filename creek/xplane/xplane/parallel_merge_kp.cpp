/* hym: functions for parallel merge in aggregation */

#include "xplane_lib.h"
#include <cassert>
#include <cstring>

#ifdef KNL
#include "x2_sort_merge.h"
#elif defined(ARCH_AVX2)
//TODO add avx2's header file
extern uint64_t avx_merge(int64_t * const inpA, int64_t * const inpAp,
                   int64_t * const inpB, int64_t * const inpBp,
                   int64_t * const out, int64_t * const outp,
                   const uint64_t lenA, const uint64_t lenB);
#else
#error  "undefined"
#endif

//extern "C"{ //<--- don't use this. we already have it in batch.h
#include "mm/batch.h"
//}

/* 
 * Spec: seach key in data(only includes keys) between offset l and r
 * Return: see the Spec of binary_search() above
 **/
int binary_search_k(simd_t *data, int l, int r, simd_t key){
	if(r >= l){
		int mid = l + (r - l)/2;
		if(data[mid] == key){
			return mid;
		}
		if(data[mid] > key){
			return binary_search_k(data, l, mid - 1, key);
		}
		return binary_search_k(data, mid + 1, r, key);
	}
	return r;
}

/* 
 * Spec: plit @src_ak and @src_bk to N partitions, so we can use N threads to merge these small partitions 
 * @src_ak: batch that contain keys only
 * @offset_a: start offset of each partition of src_ak
 * @len_a: len of each partition of src_ak
 * @src_bk: the other batch that contain keys only
 * @offset_b: start offset of each partition of src_bk
 * @len_b: len of each partition of src_bk
 * @n_parts: # of partitions
 * 
 * Note: 
 *   len of src_ak and src_bk should be store in "size" in struct batch
 *   offset_a, len_a, offset_b, and len_b should be preallocated by caller
 *
 * Diff split_batch() and split_batch_k():
 *   split_batch(): src_a and strc are batches that container records
 *   split_batch_k(): src_ak and src_bk are batches that only contain keys
 * */
void split_batch_k(x_addr src_ak, uint32_t* offset_a, uint32_t* len_a,  
                   x_addr src_bk, uint32_t* offset_b, uint32_t* len_b, 
		   uint32_t n_parts){
	
	batch_t *src_AK = (batch_t *) src_ak;	
	batch_t *src_BK = (batch_t *) src_bk;
	
	simd_t* start_AK = src_AK->start;
	simd_t* start_BK = src_BK->start;

	uint32_t len_AK = src_AK->size; // # keys in A
	uint32_t len_BK = src_BK->size; // # keys in B
	//printf("len_A is %d, len_B is %d\n", len_A, len_B);
	
	/* 1. set src_a's offset_a[] and len_a[]*/
	uint32_t each_len_AK = len_AK / n_parts;
	uint32_t last_len_AK = len_AK - (n_parts - 1) * each_len_AK;
	//printf("each_len_A is %d, last_len_A is %d\n", each_len_A, last_len_A);
	
	uint32_t i;
	for(i = 0; i < n_parts - 1; i++){
		offset_a[i] = i * each_len_AK;
		len_a[i] = each_len_AK;
	}
	offset_a[i] = i * each_len_AK;
	len_a[i] = last_len_AK;

	/* 2. set src_b's offset_b[] and len_b[] */
	int pos = 0;
	simd_t key;
	offset_b[0] = 0;
 	int pos_l = 0;
	int pos_r = len_BK;

	for(i = 1; i < n_parts; i++){
		key = *(start_AK + offset_a[i]); 	
		pos = binary_search_k(start_BK, pos_l, pos_r, key);
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
 * Spec: merge keys/ptrs starting from offset_a in src_ak/src_ap and keys/ptrs starting from offset_b in src_bk/src_bp to dest_mergek/dest_mergep starting from merge_offset 
 * @src_ak: 1st batch of keys
 * @src_ap: 1st batch of ptrs
 * @offset_a: offset of starting point in src_ak/src_ap
 * @len_a: # of keys/ptrs of current partition in src_ak/src_ap to be merged
 * @src_ak: 2nd batch of keys
 * @src_ap: 2nd batch of ptrs
 * @offset_b: offset of starting point in src_bk/src_bp
 * @len_b: # of keys/ptrs of current partition in src_bk/src_bp to be merged
 * @merge_offset: start point of merge in dest_mergek/dest_mergep (should be pre-calculated)
 *
 * Note:
 *   dest_mergek/dest_mergep should be preallocated by caller
 *
 * Diff merge_one_part() and merge_one_part_kp():
 *   merge_one_part(): src_a and str_b are batches that container records
 *   merge_one_part_kp(): src_ak/src_ap and src_bk/src_bp are batches that only contain keys/ptrs
 */
void merge_one_part_kp(x_addr src_ak, x_addr src_ap, uint32_t offset_a, uint32_t len_a,
                       x_addr src_bk, x_addr src_bp, uint32_t offset_b, uint32_t len_b,
		       x_addr dest_mergek, x_addr dest_mergep, uint32_t offset_merge){
	
	assert(dest_mergek && dest_mergep);	
	
	//printf("---offset_a is %d, len_a is %d, offset_b is %d, len_b is %d\n", offset_a, len_a, offset_b, len_b);	
	batch_t *src_AK = (batch_t *) src_ak;
	batch_t *src_AP = (batch_t *) src_ap;
	batch_t *src_BK = (batch_t *) src_bk;
	batch_t *src_BP = (batch_t *) src_bp;
	
	simd_t * start_AK = src_AK->start + offset_a;
	simd_t * start_AP = src_AP->start + offset_a;
	simd_t * start_BK = src_BK->start + offset_b;
	simd_t * start_BP = src_BP->start + offset_b;
	
	uint32_t len_A = len_a; // # keys/ptrs in A
	uint32_t len_B = len_b; // # keys/ptrs in B
	
	batch_t *dest_K = (batch_t *) dest_mergek;
	batch_t *dest_P = (batch_t *) dest_mergep;
	
	simd_t *start_dest_K = dest_K->start + offset_merge;
	simd_t *start_dest_P = dest_P->start + offset_merge;
		
	if(len_A == 0 && len_B == 0){
		/*do nothing*/
		return;
	}else if(len_A == 0 && len_B != 0){
		/* copy src_b to merge_dest*/
		memcpy(start_dest_K, start_BK, sizeof(simd_t)* len_B);
		memcpy(start_dest_P, start_BP, sizeof(simd_t)* len_B);
	}else if(len_A != 0 && len_B == 0){
		/* copy src_a to merge_dest*/
		memcpy(start_dest_K, start_AK, sizeof(simd_t)* len_A);
		memcpy(start_dest_P, start_AP, sizeof(simd_t)* len_A);
	}else{
#ifdef KNL
		/* merge */
		if(len_A%8 == 0 && len_B%8 == 0){
			//x2_merge_8_aligned(k_A, ptr_A, k_B, ptr_B, k_out, ptr_out, len_A, len_B);				
			x2_merge_8_aligned(start_AK, start_AP, start_BK, start_BP, start_dest_K, start_dest_P, len_A, len_B);				
		}else{
			//x2_merge_8_unaligned(k_A, ptr_A, k_B, ptr_B, k_out, ptr_out, len_A, len_B);				
			x2_merge_8_unaligned(start_AK, start_AP, start_BK, start_BP, start_dest_K, start_dest_P, len_A, len_B);				
		}
#elif defined(ARCH_AVX2)
		avx_merge(start_AK, start_AP, start_BK, start_BP, start_dest_K, start_dest_P, len_A, len_B);
#else
#error "undefined"
#endif
	}

}


void calculate_offset_kp(uint32_t* len_a, uint32_t* len_b, uint32_t* offset, uint32_t n_parts){
	offset[0] = 0;
	for(uint32_t i = 1; i < n_parts; i++){
		offset[i] = offset[i-1] + (len_a[i-1] + len_b[i-1]);
	}
}

