#include <err.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <pthread.h>

#include "log.h"// xzl
#include "ubench.h"
#include "xplane_lib.h"
#include "mm/batch.h"

void test_binary_search(){
	int reclen = 3;
	idx_t keyops = 0;
 	int record_num = 7;	
	simd_t * data = (simd_t *) malloc(sizeof(simd_t) * record_num * reclen);

	printf("<offset, value>:\n");
	for(int i = 0; i < record_num; i++){
		for(int j = 0; j < reclen; j++){
			data[i*reclen + j] = 2 + i*2; 
			printf("<%d, %ld> ", i*reclen+j, data[i*reclen+j]);
		}
		printf("; ");
	}
	
	printf("\n1. find an existing value: 4\n");
	int exit_offset = binary_search(data, 0, (record_num -1)* reclen, 4, keyops, reclen);
	printf("positin is: %d\n", exit_offset);
	
	
	printf("\n2.find an non-existing value: 7\n");
	int non_exit_offset = binary_search(data, 0, (record_num -1)* reclen, 7, keyops, reclen);
	printf("positin is: %d\n", non_exit_offset);


	printf("\n3.find a value smaller than all: 1\n");
	int smaller_all_offset = binary_search(data, 0, (record_num -1)* reclen, 1, keyops, reclen);
	printf("positin is: %d\n", smaller_all_offset);

	printf("\n3.find a value larger than all: 100\n");
	int larger_all_offset = binary_search(data, 0, (record_num -1)* reclen, 100, keyops, reclen);
	printf("positin is: %d\n", larger_all_offset);
}


void test_split_batch(){
	printf("test_split_batch(): \n");	
	int reclen = 3;
	idx_t keypos = 0;		
	uint32_t n_parts = 3;
	
	int record_num_A = 10;
	int record_num_B = 9;
	
	struct batch * src_batch_A = (struct batch *) malloc(sizeof(struct batch));
	batch_t * src_batch_B = (batch_t *) malloc(sizeof(batch_t));

	src_batch_A->size = record_num_A * reclen;
	src_batch_A->start = (int64_t *) malloc(sizeof(int64_t) * src_batch_A->size);
	
	src_batch_B->size = record_num_B * reclen;
	src_batch_B->start = (int64_t *) malloc(sizeof(int64_t) * src_batch_B->size);

	printf("Batch A: record <k, offset>, <v, offset>, <ts, offset>\n");
	for(int i = 0; i < record_num_A; i++){	
		for(int j = 0; j < reclen; j++){
			src_batch_A->start[i*reclen +j] = i; 
			printf("<%ld, %d> ",src_batch_A->start[i*reclen+j], i*reclen+j);
		}
		printf("\n");
	}
	printf("\n");
	
	printf("Batch B: record <k, offset>, <v, offset>, <ts, offset>\n");
	for(int i = 0; i < record_num_B; i++){	
		for(int j = 0; j < reclen; j++){
			src_batch_B->start[i*reclen +j] = i*2; 
			printf("<%ld, %d> ", src_batch_B->start[i*reclen+j], i*reclen+j);
		}
		printf("\n");
	}
	printf("\n");
	
	x_addr src_A = (x_addr) (src_batch_A);
	x_addr src_B = (x_addr) (src_batch_B);
	
	uint32_t* offset_a = (uint32_t *) malloc(sizeof(uint32_t) * n_parts);
	uint32_t* offset_b = (uint32_t *) malloc(sizeof(uint32_t) * n_parts);
	uint32_t* len_a = (uint32_t *) malloc(sizeof(uint32_t) * n_parts);
	uint32_t* len_b = (uint32_t *) malloc(sizeof(uint32_t) * n_parts);
	
	split_batch(src_A, offset_a, len_a, src_B, offset_b, len_b, n_parts, keypos, reclen);
	for(int i = 0; i < n_parts; i++){
		printf("offset_a[%d] is: %d, len_a[%d] is: %d, offset_b[%d] is: %d, len_b[%d] is: %d\n", i, offset_a[i], i, len_a[i], i, offset_b[i], i, len_b[i]);
	}
}


void test_merge_one_part(){	
	printf("test_merge_one_part(): \n");	
	int reclen = 3;
	idx_t keypos = 0;		
	uint32_t n_parts = 3;
	
	int record_num_A = 10;
	int record_num_B = 9;
	
	batch_t * src_batch_A = (batch_t *) malloc(sizeof(batch_t));
	batch_t * src_batch_B = (batch_t *) malloc(sizeof(batch_t));

	src_batch_A->size = record_num_A * reclen;
	src_batch_A->start = (int64_t *) malloc(sizeof(int64_t) * src_batch_A->size);
	
	src_batch_B->size = record_num_B * reclen;
	src_batch_B->start = (int64_t *) malloc(sizeof(int64_t) * src_batch_B->size);

	printf("Batch A: record <k, offset>, <v, offset>, <ts, offset>\n");
	for(int i = 0; i < record_num_A; i++){	
		for(int j = 0; j < reclen; j++){
			src_batch_A->start[i*reclen +j] = i; 
			printf("<%ld, %d> ",src_batch_A->start[i*reclen+j], i*reclen+j);
		}
		printf("\n");
	}
	printf("\n");
	
	printf("Batch B: record <k, offset>, <v, offset>, <ts, offset>\n");
	for(int i = 0; i < record_num_B; i++){	
		for(int j = 0; j < reclen; j++){
			src_batch_B->start[i*reclen +j] = i*2; 
			printf("<%ld, %d> ", src_batch_B->start[i*reclen+j], i*reclen+j);
		}
		printf("\n");
	}
	printf("\n");
	
	x_addr src_A = (x_addr) (src_batch_A);
	x_addr src_B = (x_addr) (src_batch_B);
	
	uint32_t* offset_a = (uint32_t *) malloc(sizeof(uint32_t) * n_parts);
	uint32_t* offset_b = (uint32_t *) malloc(sizeof(uint32_t) * n_parts);
	uint32_t* len_a = (uint32_t *) malloc(sizeof(uint32_t) * n_parts);
	uint32_t* len_b = (uint32_t *) malloc(sizeof(uint32_t) * n_parts);

	/* 1. split A and B to n parts */
	split_batch(src_A, offset_a, len_a, src_B, offset_b, len_b, n_parts, keypos, reclen);
	for(int i = 0; i < n_parts; i++){
		printf("offset_a[%d] is: %d, len_a[%d] is: %d, offset_b[%d] is: %d, len_b[%d] is: %d\n", i, offset_a[i], i, len_a[i], i, offset_b[i], i, len_b[i]);
	}

	/* 2. allocate batch for merge result*/
	batch_t * merge_batch = (batch_t *) malloc(sizeof(batch_t));
	merge_batch->start = (int64_t *) malloc(sizeof(int64_t) * (src_batch_A->size + src_batch_B->size));
	uint32_t* merge_offset = (uint32_t *) malloc(sizeof(uint32_t) * n_parts);

	/* 3. calculate merge_offset[] */
	calculate_offset(len_a, len_b, merge_offset, n_parts, reclen);
	for(int i = 0; i < n_parts; i++){
		printf("merge_offset[%d] is %d\n", i, merge_offset[i]);
	}

	/* 4. merge all partitions */
	x_addr merge_dest = (x_addr) (merge_batch);
	for(int i = 0; i < n_parts; i++){
		printf("part: %d\n", i);
		merge_one_part(src_A, offset_a[i], len_a[i], src_B, offset_b[i], len_b[i], merge_dest, merge_offset[i], keypos, reclen);
	}
	
	/* 5. print merge results*/
	printf("Merged results: record <k, offset>, <v, offset>, <ts, offset>\n");
	for(int i = 0; i < record_num_A + record_num_B; i++){	
		for(int j = 0; j < reclen; j++){
			printf("<%ld, %d> ", merge_batch->start[i*reclen+j], i*reclen+j);
		}
		printf("\n");
	}
	printf("\n");
}

int main(){
	//test_binary_search();
	//test_split_batch();
	test_merge_one_part();	
	return 0;
}
