/* hym: functions for parallel aggregation */

#include "xplane_lib.h"
#include <cassert>
#include <cstring>
#include "mm/batch.h"

/* the invariant of taking k batch as input */

//int32_t find_k_boundary_k(uint32_t * offsets, x_addr src_k, uint32_t n_parts){
int32_t find_k_boundary_k(uint32_t * offsets, uint32_t * lens, x_addr src_k, uint32_t n_parts){
	if(!src_k){
		//kbatch is null
		return 0;
	}
	batch_t *batch_k = (batch_t *) src_k;
	assert(n_parts);
	uint32_t len_k = batch_k->size;
	uint32_t part_gap = len_k / n_parts;

	simd_t cmp_k; 
	uint32_t idx;
	uint32_t start_pos;
	offsets[0] = 0;
	start_pos = offsets[0] + part_gap;
	
	for(uint32_t i = 1; i < n_parts; i++){
		cmp_k = batch_k->start[start_pos];
		idx = start_pos; 	
		while(idx < len_k){
			//find first k != cmp_k
			if(cmp_k != batch_k->start[idx]){
				break;
			}
			idx++;
		}
		offsets[i] = idx;
		
		if(idx < start_pos + part_gap && idx < len_k){
			start_pos = start_pos + part_gap;
		}else{
			start_pos = idx;
		}
	}
	
	for(uint32_t i = 0; i < n_parts - 1; i++){
		lens[i] = offsets[i+1] - offsets[i];
	}
	lens[n_parts - 1] = len_k - offsets[n_parts - 1];
	
	return 1;
}

/*
//IN: offsets[], src_k, n_parts
//OUT: len[]
// len[] should be allocated by caller
int32_t calculate_len(uint32_t *len, uint32_t * offsets, x_addr src_k, uint32_t n_parts){
	if(!src_k){
		//kbatch is NULL
		return 0;
	}

	batch_t *batch_k = (batch_t *) src_k;
	uint32_t len_k = batch_k->size;		
	
	for(uint32_t i = 0; i < n_parts - 1; i++){
		len[i] = offsets[i+1] - offsets[i];
	}
	
	len[n_parts - 1] = len_k - offsets[n_parts - 1];
	return 1;
}
*/

