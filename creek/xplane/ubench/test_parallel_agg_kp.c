//hym: to test functions for parallel aggregation
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
#include <assert.h>
//int32_t find_k_boundary_k(uint32_t * offsets, uint32_t * lens, x_addr src_k, uint32_t n_parts)

void test_find_k_boundary_k(){
	printf("test_find_k_boundary_k: \n");	
	uint32_t key_num = 20;
	uint32_t n_parts = 4;
	uint32_t reclen = 3;

	batch_t * batch_k = batch_new(0, sizeof(simd_t) * key_num);
	batch_t * batch_p = batch_new(0, sizeof(simd_t) * key_num);
	batch_t * batch_r = batch_new(0, sizeof(simd_t) * key_num * reclen); 

	batch_update(batch_k, batch_k->start + key_num);
	uint32_t * offsets = (uint32_t *) malloc(sizeof(int32_t) * n_parts);
	uint32_t * lens = (uint32_t *) malloc(sizeof(int32_t) * n_parts);
	
	assert(batch_k != NULL);
	assert(offsets != NULL);
	assert(lens != NULL);

	//init k_batch, which should be sorted
	simd_t gap = 3;
	printf("k batch:<index, k>\n");

	for(uint32_t i = 0; i < key_num; i++){
		for(uint32_t j = 0; j < reclen; j++) {
			batch_r->start[i * reclen + j] = i / gap;
		}
		printf("<%ld %ld %ld>\n", batch_r->start[i * reclen],
				batch_r->start[i * reclen + 1], batch_r->start[i * reclen + 2]);
	}
	batch_update(batch_r, batch_r->start + (key_num * reclen));

	for(uint32_t i = 0; i < key_num; i++){
		batch_k->start[i] = i / gap;
		batch_p->start[i] = (simd_t) (batch_r->start + i * reclen);
		printf("<%d,%ld,%ld>\n", i, batch_k->start[i], batch_p->start[i]);	
	}
	printf("\n");

	batch_update(batch_p, batch_p->start + key_num);

	int32_t ret = find_k_boundary_k(offsets, lens, (x_addr) batch_k, n_parts);

	xzl_bug_on(ret < 0);

	printf("n_parts: offsets[i], lens[i]\n");
	for(uint32_t i = 0; i < n_parts; i++){
		printf("offsets[%d] %d, lens[%d] %d\n", i, offsets[i], i, lens[i]);
	}
	printf("\n");


	x_addr dest;
	idx_t vpos = 1;
	float topP = 0.5;
	batch_t *batch_dest;

	for (uint32_t k = 0; k < n_parts; k++) {
#if 0	/* test kpercentile by key */
		kpercentile_bykey_one_part_k(&dest, (x_addr)batch_k, (x_addr)batch_p,
				offsets[k], lens[k], vpos, reclen, topP);
#else	/* test topk by key */
		topk_bykey_one_part_k(&dest, (x_addr)batch_k, (x_addr)batch_p,
				offsets[k], lens[k], vpos, reclen, topP);
#endif
		batch_dest = (batch_t *) dest;
	
		printf("returned batch sized : %ld\n", batch_dest->size);
		for (uint32_t l = 0; l < batch_dest->size / 2; l++) {
			printf("<%ld, %ld>\n", batch_dest->start[l * 2], batch_dest->start[l * 2 + 1]);
		}
	}
}

int main(){
	xplane_init(NULL);
	test_find_k_boundary_k();
}
