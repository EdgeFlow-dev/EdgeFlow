#include <err.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <pthread.h>

#include "log.h"
#include "ubench.h"
#include "xplane_lib.h"
#include "mm/batch.h"


//uint32_t binary_search_k(simd_t *data, int l, int r, simd_t key)
void test_binary_search_k(){
	printf("test_binary_search_k(): \n");	
 	int key_num = 7;	
	simd_t * data = (simd_t *) malloc(sizeof(simd_t) * key_num);

	printf("<key, offset>:\n");
	for(int i = 0; i < key_num; i++){
			data[i] = 2 + i*2; 
			printf("<%ld, %d>\n", data[i], i);
	}
	
	printf("\n1. find an existing value: 4\n");
	int exit_offset = binary_search_k(data, 0, key_num - 1, 4);
	printf("positin is: %d\n", exit_offset);
	
	
	printf("\n2.find an non-existing value: 7\n");
	int non_exit_offset = binary_search_k(data, 0, key_num - 1, 7);
	printf("positin is: %d\n", non_exit_offset);


	printf("\n3.find a value smaller than all: 1\n");
	int smaller_all_offset = binary_search_k(data, 0, key_num - 1, 1);
	printf("positin is(should be -1): %d\n", smaller_all_offset);

	printf("\n4.find a value larger than all: 100\n");
	int larger_all_offset = binary_search_k(data, 0, key_num - 1, 100);
	printf("positin is(should be the last one of the buff): %d\n", larger_all_offset);
}

void test_split_batch_kp(){
	printf("test_split_batch_kp(): \n");	
	uint32_t n_parts = 3;
	
	int keys_num_A = 10;
	int keys_num_B = 9;
	
	batch_t * src_batch_AK = (batch_t *) malloc(sizeof(batch_t));
	batch_t * src_batch_BK = (batch_t *) malloc(sizeof(batch_t));

	src_batch_AK->size = keys_num_A;
	src_batch_AK->start = (simd_t *) malloc(sizeof(simd_t) * src_batch_AK->size);
	
	src_batch_BK->size = keys_num_B;
	src_batch_BK->start = (simd_t *) malloc(sizeof(simd_t) * src_batch_BK->size);

	printf("Batch AK: <k, offset>\n");
	for(int i = 0; i < keys_num_A; i++){	
		src_batch_AK->start[i] = i; 
		printf("<%ld, %d> ",src_batch_AK->start[i], i);
	}
	printf("\n");
	
	printf("Batch BK: <k, offset>\n");
	for(int i = 0; i < keys_num_B; i++){	
		src_batch_BK->start[i] = i*2; 
		printf("<%ld, %d> ", src_batch_BK->start[i], i);
	}
	printf("\n");
	
	x_addr src_AK = (x_addr) (src_batch_AK);
	x_addr src_BK = (x_addr) (src_batch_BK);
	
	uint32_t* offset_a = (uint32_t *) malloc(sizeof(uint32_t) * n_parts);
	uint32_t* offset_b = (uint32_t *) malloc(sizeof(uint32_t) * n_parts);
	uint32_t* len_a = (uint32_t *) malloc(sizeof(uint32_t) * n_parts);
	uint32_t* len_b = (uint32_t *) malloc(sizeof(uint32_t) * n_parts);
	
	split_batch_k(src_AK, offset_a, len_a, src_BK, offset_b, len_b, n_parts);
	
	for(int i = 0; i < n_parts; i++){
		printf("offset_a[%d] is: %d, len_a[%d] is: %d, offset_b[%d] is: %d, len_b[%d] is: %d\n", i, offset_a[i], i, len_a[i], i, offset_b[i], i, len_b[i]);
	}
}


void test_merge_one_part_kp(){	
	printf("test_merge_one_part_kp(): \n");	
	uint32_t n_parts = 3;
	
	int kp_num_A = 10;
	int kp_num_B = 9;
	
	batch_t * src_batch_AK = (batch_t *) malloc(sizeof(batch_t));
	batch_t * src_batch_AP = (batch_t *) malloc(sizeof(batch_t));
	batch_t * src_batch_BK = (batch_t *) malloc(sizeof(batch_t));
	batch_t * src_batch_BP = (batch_t *) malloc(sizeof(batch_t));

	src_batch_AK->size = kp_num_A;
	src_batch_AP->size = kp_num_A;
	src_batch_AK->start = (simd_t *) malloc(sizeof(simd_t) * src_batch_AK->size);
	src_batch_AP->start = (simd_t *) malloc(sizeof(simd_t) * src_batch_AP->size);
	
	src_batch_BK->size = kp_num_B;
	src_batch_BP->size = kp_num_B;
	src_batch_BK->start = (simd_t *) malloc(sizeof(simd_t) * src_batch_BK->size);
	src_batch_BP->start = (simd_t *) malloc(sizeof(simd_t) * src_batch_BP->size);

	printf("A's : <k, p, offset>\n");
	for(int i = 0; i < kp_num_A; i++){	
		src_batch_AK->start[i] = i; 
		src_batch_AP->start[i] = i; 
		printf("<%ld, %ld, %d> ", src_batch_AK->start[i], src_batch_AP->start[i], i);
	}
	printf("\n");
	
	printf("B's : <k, p, offset>\n");
	for(int i = 0; i < kp_num_B; i++){	
		src_batch_BK->start[i] = i * 2; 
		src_batch_BP->start[i] = i * 2; 
		printf("<%ld, %ld, %d> ", src_batch_BK->start[i], src_batch_BP->start[i], i);
	}
	printf("\n");
	
	x_addr src_AK = (x_addr) (src_batch_AK);
	x_addr src_AP = (x_addr) (src_batch_AP);
	x_addr src_BK = (x_addr) (src_batch_BK);
	x_addr src_BP = (x_addr) (src_batch_BP);

	uint32_t* offset_a = (uint32_t *) malloc(sizeof(uint32_t) * n_parts);
	uint32_t* offset_b = (uint32_t *) malloc(sizeof(uint32_t) * n_parts);
	uint32_t* len_a = (uint32_t *) malloc(sizeof(uint32_t) * n_parts);
	uint32_t* len_b = (uint32_t *) malloc(sizeof(uint32_t) * n_parts);

	/* 1. split A and B to n parts */
	split_batch_k(src_AK, offset_a, len_a, src_BK, offset_b, len_b, n_parts);
	
	for(int i = 0; i < n_parts; i++){
		printf("offset_a[%d] is: %d, len_a[%d] is: %d, offset_b[%d] is: %d, len_b[%d] is: %d\n", i, offset_a[i], i, len_a[i], i, offset_b[i], i, len_b[i]);
	}

	/* 2. allocate batch for merge result*/
	batch_t * merge_batch_k = (batch_t *) malloc(sizeof(batch_t));
	batch_t * merge_batch_p = (batch_t *) malloc(sizeof(batch_t));
	
	merge_batch_k->start = (simd_t *) malloc(sizeof(simd_t) * (src_batch_AK->size + src_batch_BK->size));
	merge_batch_p->start = (simd_t *) malloc(sizeof(simd_t) * (src_batch_AP->size + src_batch_BP->size));
	
	uint32_t* merge_offset = (uint32_t *) malloc(sizeof(uint32_t) * n_parts);

	/* 3. calculate merge_offset[] */
	calculate_offset_kp(len_a, len_b, merge_offset, n_parts);
	
	for(int i = 0; i < n_parts; i++){
		printf("merge_offset[%d] is %d\n", i, merge_offset[i]);
	}

	/* 4. merge all partitions */
	x_addr merge_dest_K = (x_addr) (merge_batch_k);
	x_addr merge_dest_P = (x_addr) (merge_batch_p);
	
	for(int i = 0; i < n_parts; i++){
		printf("part: %d\n", i);
		merge_one_part_kp(src_AK, src_AP, offset_a[i], len_a[i],
				  src_BK, src_BP, offset_b[i], len_b[i],
				  merge_dest_K, merge_dest_P, merge_offset[i]);	
	}
	
	/* 5. print merge results*/
	printf("Merged results: <k, p>\n");
	for(int i = 0; i < kp_num_A + kp_num_B; i++){	
		printf("<%ld, %ld> ", merge_batch_k->start[i], merge_batch_p->start[i]);
	}
	printf("\n");
}


int main(){
	//test_binary_search_k();
	//test_split_batch_kp();
	test_merge_one_part_kp();	
	
	return 0;
}
