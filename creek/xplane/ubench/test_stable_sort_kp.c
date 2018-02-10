#include "log.h"
#include "ubench.h"
#include "xplane_lib.h"
#include "mm/batch.h"
#include <assert.h>

#define NUM_KEY		10
#define REC_NUM		30

void print_x_addr_batch(x_addr batch_addr, idx_t reclen) {
	batch_t *batch = (batch_t *) batch_addr;
	uint64_t i;

	for (i = 0; i < batch->size; i++) {
		printf ("%10ld", batch->start[i]);
		if ((i + 1) % reclen == 0) 
			printf("\n");
	}
}

void print_x_addr_batch_kp(x_addr k_addr, x_addr p_addr) {
	batch_t *batch_k = (batch_t *) k_addr;
	batch_t *batch_p = (batch_t *) p_addr;
	uint64_t i;

	for (i = 0; i < batch_k->size; i++) {
		printf ("k: %ld p: %ld\n", batch_k->start[i], batch_p->start[i]);
	}
}

void make_x3_src(x_addr *src){
	batch_t *batch_r = batch_new(0, sizeof(simd_t) * REC_NUM * 3);
	simd_t *data = batch_r->start;
	int i;

	for (i = 0; i < REC_NUM; i++) {
		*data++ = i;					/* ts */
		*data++ = rand() % NUM_KEY;	/* key */
		*data++ = i;					/* value */ 
	}
	batch_update(batch_r, data);

	*src = (x_addr)batch_r;
}

void test_stable_sort(void) {
	idx_t reclen = 3;
	idx_t keypos = 1;
	uint32_t n_outputs = 1;

	int32_t ret;
	x_addr *dests_k, *dests_p;
	x_addr src, src_k, src_p;

	dests_k = (x_addr *) malloc (sizeof(x_addr) * n_outputs);
	dests_p = (x_addr *) malloc (sizeof(x_addr) * n_outputs);

	/* make src sorted by value */
	make_x3_src(&src);

	print_x_addr_batch(src, reclen);

	ret = extract_kp(&src_k, &src_p, src, keypos, reclen);
	xzl_bug_on(ret < 0);
	print_x_addr_batch_kp(src_k, src_p);
	
	sort_stable_kp(dests_k, dests_p, n_outputs,
			src_k, src_p);
	xzl_bug_on(ret < 0);

	for(int i = 0; i < n_outputs; i++) {
		print_x_addr_batch_kp(dests_k[i], dests_p[i]);
	}
}


int main(int argc, char *argv[])
{
	test_stable_sort();
	return 0;
}


