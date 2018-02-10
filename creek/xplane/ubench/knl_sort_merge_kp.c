/* 
 * hym 12/15/2017
 * test sort_kp() and merge_kp() defined at Sort-Merge/AVX-512-Sort-Merge 
 */
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include "mm/batch.h"
#include "xplane_lib.h"
#include "ubench.h"

void test_sort_kp(){
	printf("test_sort_kp(): \n");
	int key_num = 100;
	batch_t * src_batch_k = (batch_t *) malloc(sizeof(batch_t));
	batch_t * src_batch_p = (batch_t *) malloc(sizeof(batch_t));

	src_batch_k->size = key_num;
	src_batch_p->size = key_num;

	src_batch_k->start = (simd_t *) malloc(sizeof(simd_t) * src_batch_k->size);
	src_batch_p->start = (simd_t *) malloc(sizeof(simd_t) * src_batch_p->size);
	
	for(int i = 0; i < key_num; i++){	
		src_batch_k->start[i] = 200 - i; 
		src_batch_p->start[i] = 200 - i; 
	}
	
	x_addr src_k = (x_addr) (src_batch_k);
	x_addr src_p = (x_addr) (src_batch_p);
	
	x_addr *dests_k;
	x_addr *dests_p;
	uint64_t n_outputs = 3;
	
	dests_k = (x_addr *) malloc(sizeof(x_addr) * n_outputs);
	dests_p = (x_addr *) malloc(sizeof(x_addr) * n_outputs);
	
	sort_kp(dests_k, dests_p, n_outputs, src_k, src_p);

	batch_t *dst_k;
	batch_t *dst_p;
	
	for(int i = 0; i < n_outputs; i++){
		printf("sorted out <k,p> %d:\n", i);
		dst_k = (batch_t *) dests_k[i];
		dst_p = (batch_t *) dests_p[i];
		
		for(uint64_t j = 0; j < dst_k->size; j++){
			printf("<%ld, %ld>\n", dst_k->start[j], dst_p->start[j]);
		}
		printf("\n");
	}
}



void test_merge_kp(){
	printf("test_merge_kp(): \n");	
	
	int key_num_A = 10;
	int key_num_B = 50;
	
	batch_t * src_batch_A_k = (batch_t *) malloc(sizeof(batch_t));
	batch_t * src_batch_A_p = (batch_t *) malloc(sizeof(batch_t));
	batch_t * src_batch_B_k = (batch_t *) malloc(sizeof(batch_t));
	batch_t * src_batch_B_p = (batch_t *) malloc(sizeof(batch_t));

	src_batch_A_k->size = key_num_A;
	src_batch_A_p->size = key_num_A;
	src_batch_B_k->size = key_num_B;
	src_batch_B_p->size = key_num_B;
	
	src_batch_A_k->start = (simd_t *) malloc(sizeof(simd_t) * src_batch_A_k->size);
	src_batch_A_p->start = (simd_t *) malloc(sizeof(simd_t) * src_batch_A_p->size);
	src_batch_B_k->start = (simd_t *) malloc(sizeof(simd_t) * src_batch_B_k->size);
	src_batch_B_p->start = (simd_t *) malloc(sizeof(simd_t) * src_batch_B_p->size);
	
	for(int i = 0; i < key_num_A; i++){	
		src_batch_A_k->start[i] = i; 
		src_batch_A_p->start[i] = i; 
	}
	
	for(int i = 0; i < key_num_B; i++){	
		src_batch_B_k->start[i] = i; 
		src_batch_B_p->start[i] = i; 
	}
	
	x_addr src_A_k = (x_addr) (src_batch_A_k);
	x_addr src_A_p = (x_addr) (src_batch_A_p);
	x_addr src_B_k = (x_addr) (src_batch_B_k);
	x_addr src_B_p = (x_addr) (src_batch_B_p);
	
	x_addr *dests_k;
	x_addr *dests_p;
	
	uint64_t n_outputs = 3;
	dests_k = (x_addr *) malloc(sizeof(x_addr) * n_outputs);
	dests_p = (x_addr *) malloc(sizeof(x_addr) * n_outputs);

	merge_kp(dests_k, dests_p, n_outputs, src_A_k, src_A_p, src_B_k, src_B_p);
		
	batch_t *dst_k;
	batch_t *dst_p;
	
	for(int i = 0; i < n_outputs; i++){
		dst_k = (batch_t *) dests_k[i];
		dst_p = (batch_t *) dests_p[i];

		printf("merged out <k,p> %d: size is %ld\n", i, dst_k->size);
		for(uint64_t j = 0; j < dst_k->size; j++){
			printf("<%ld, %ld>\n", dst_k->start[j], dst_p->start[j]);
		}
	}
}

int main(){
	//xplane_init();
	xplane_init(NULL);
	test_merge_kp();
	printf("--------------------------------\n");
	test_sort_kp();
	return 0;
}
