#include <err.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <pthread.h>

#include "log.h"// xzl

#include "xplane-internal.h"
#include "ubench.h"

//#define QSORT

extern x_addr global_src;
extern uint32_t execution_time;

void print_x_addr_batch(x_addr batch_addr, idx_t reclen) {
	batch_t *batch = (batch_t *) batch_addr;
	uint64_t i;
	simd_t *tmp;

	for (i = 0; i < batch->size; i++) {
#if 0
/* print records */
		tmp = (simd_t *) batch->start[i];
		for(int j = 0; j < reclen; j++)
			printf("%10ld", tmp[j]);
		printf("\n");
#else 
/* print pointers */
		printf("%10ld", batch->start[i]);
		if ((i + 1) % reclen == 0)
			printf("\n");
#endif
	}
}


int main(int argc, char *argv[])
{
	simd_t *nsbuf;
	x_addr *src;
	uint32_t i;
	int32_t ret;

	/**************** generate global src ********************/
	nsbuf = (simd_t *) malloc (sizeof(simd_t) * NITEMS * RECLEN);

#if defined X1_type
	ret = load_file_x1(nsbuf);
#elif defined X3_type
	ret = load_file(nsbuf);
#endif

	if(ret < 0){
		free(nsbuf);
		return -1;
	}

	global_src = create_uarray_nsbuf(nsbuf, NITEMS * RECLEN);
	/*********************************************************/
	I("global_src : 0x%lx", global_src);


	show_sbuff_resource();

	int ncores = 8;
	int tput = 1;
	if (argc > 2){
		ncores = atoi(argv[1]);
		tput = atoi(argv[2]);
	}
	I("to run mt_x_run() ncores =%d\n", ncores);


//	for(i = 0 ; i < ITER; i++){
	for(i = 0 ; i < 1; i++){
#if 0
//		create_segment(&src, tput * 1024, ncores);
#else
		src = (x_addr *) malloc (sizeof(x_addr) * ncores);
		create_src(src, tput * 1024, ncores);
		
		x_addr *dests_p;
		simd_t *seg_bases;
		uint32_t n_segs = 0;
		simd_t base = 40;
		simd_t subrange = 100;
		idx_t tspos = 0;
		idx_t reclen = 3;

		segment_kp(&dests_p, &seg_bases, &n_segs,
				src[0], base, subrange, tspos, reclen);
#endif
		printf("n_segs : %d\n", n_segs);
		for (i = 0; i < n_segs; i++){
			printf("-------------------------------------------------\n");
			printf("seg_bases : %ld\n", seg_bases[i]);
			printf("-------------------------------------------------\n");
//			printf("%ld\n", dests_p[i]);
			print_x_addr_batch(dests_p[i], reclen);
		}
//		mt_x_run(bw_test, ncores, src);

		show_sbuff_resource();
	}
	retire_uarray(global_src);
	free(src);

	printf("Execution Time : %d\n", execution_time);
	printf("avg : %f\n", (float)execution_time / ITER);
	show_sbuff_resource();

	free(nsbuf);
}

