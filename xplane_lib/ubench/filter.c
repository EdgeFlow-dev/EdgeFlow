#include <err.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <pthread.h>

#include "log.h"// xzl
#include "ubench.h"

extern x_addr global_src;
extern execution_time;

static void test_filter_with_sort(x_addr src){
	x_addr *sorted_src;
	x_addr *dests;
	simd_t *ns_dest;

	uint32_t n_outputs = 1;
	uint32_t len = 240;
	uint32_t i, j, s;

	const idx_t tspos = 0, keypos = 1, vpos = 2, reclen = 3;
	const idx_t countpos = 0;

	int32_t ret = 0;

	sorted_src = (x_addr *) malloc (sizeof(x_addr) * n_outputs);
	ns_dest = (simd_t *) malloc (sizeof(simd_t) * len);

	ret = sort(sorted_src, n_outputs, src,
			keypos, reclen);

	xzl_bug_on(ret < 0);
	xzl_bug_on(!sorted_src);

	ret = retire_uarray(src);
	xzl_bug_on(ret < 0);

	int filter_nout = 1;
	int32_t lower = 24, higher = 50;	/* assume value range [0, 49] */
	dests = (x_addr *) malloc (sizeof(x_addr) * filter_nout);
	ns_dest = (simd_t *) malloc (sizeof(simd_t) * len);

	for(i = 0 ; i < n_outputs; i++){
		printf("filter\n");
		ret = filter_band(dests, filter_nout,
				sorted_src[i], lower, higher,	vpos, reclen);

		xzl_bug_on(ret < 0);
		xzl_bug_on(!dests);

		ret = retire_uarray(sorted_src[i]);
		xzl_bug_on(ret < 0);


		for(j = 0; j < filter_nout; j++){
			printf("[j:%d] addr : %lx, size : %d\n",
					j, dests[j], uarray_size(dests[j]));
			uarray_to_nsbuf(ns_dest, dests[0], &len);
			for(s = 0; s < len; s += 3){
				printf("%6d %6d %6d\n", ns_dest[s], ns_dest[s+1], ns_dest[s+2]);
			}
			ret = retire_uarray(dests[j]);
			xzl_bug_on(ret < 0);
		}

		printf("\n");
	}

	free(dests);
	free(sorted_src);
	free(ns_dest);
}

static void test_filter(x_addr src){
	x_addr *dests;
	simd_t *ns_dest;

	uint32_t n_outputs = 1;
	uint32_t len = 120;
	uint32_t i, j, s;

	const idx_t vpos = 2, reclen = 3;

	int32_t ret = 0;

	ns_dest = (simd_t *) malloc (sizeof(simd_t) * len);

	int filter_nout = 1;
	int32_t lower = 24, higher = 50;	/* assume value range [0, 49] */
	dests = (x_addr *) malloc (sizeof(x_addr) * filter_nout);
	ns_dest = (simd_t *) malloc (sizeof(simd_t) * len);

	printf("filter\n");
	ret = filter_band(dests, filter_nout,
			src, lower, higher,	vpos, reclen);

	xzl_bug_on(ret < 0);
	xzl_bug_on(!dests);

	for(j = 0; j < filter_nout; j++){
		printf("[j:%d] addr : %lx, size : %d\n",
				j, dests[j], uarray_size(dests[j]));
		uarray_to_nsbuf(ns_dest, dests[0], &len);
		for(s = 0; s < len; s += 3){
			printf("%6d %6d %6d\n", ns_dest[s], ns_dest[s+1], ns_dest[s+2]);
		}
		ret = retire_uarray(dests[j]);
		xzl_bug_on(ret < 0);
	}

	printf("\n");

	ret = retire_uarray(src);
	xzl_bug_on(ret < 0);

	free(dests);
	free(ns_dest);
}


#if 0

/* hp multi threads uarray_nsbuf test */
static void multiple_gls(x_addr src){
	uint32_t *nsbuf;
	x_addr gls;

	nsbuf = (uint32_t *) malloc (sizeof(uint32_t) * NITEMS * ELES_PER_ITEM);
	load_file(nsbuf);
	gls = create_uarray_nsbuf(nsbuf, 85000 * ELES_PER_ITEM);
	printf("gls : %ld", gls);

	free(nsbuf);
}

int main(int argc, char *argv[])
{
	uint32_t *nsbuf;
	x_addr *src;
	uint32_t i;

	int ncores = 1;
	int tput = 1;
	if (argc == 3){
		ncores = atoi(argv[1]);
		tput = atoi(argv[2]);
	}
	I("to run mt_x_run() ncores =%d\n", ncores);


//	create_segment(&src, 32 * 1024, ncores);
//	for(i = 0; i < 8; i++){
//		printf("%lx\n", src[i]);
//	}
	src = (x_addr *) malloc (sizeof(x_addr) * ncores);
	mt_x_run(multiple_gls, ncores, src);
	free(src);

	show_sbuff_resource();
}

#else

int main(int argc, char *argv[])
{
	uint32_t *nsbuf;
	x_addr *src;
	uint32_t i;

	/**************** generate global src ********************/
	nsbuf = (uint32_t *) malloc (sizeof(uint32_t) * NITEMS * ELES_PER_ITEM);
	load_file(nsbuf);
	global_src = create_uarray_nsbuf(nsbuf, NITEMS * ELES_PER_ITEM);
	/*********************************************************/

	show_sbuff_resource();

	int ncores = 1;
	int tput = 1;
	if (argc == 3){
		ncores = atoi(argv[1]);
		tput = atoi(argv[2]);
	}
	I("to run mt_x_run() ncores =%d\n", ncores);


//	create_segment(&src, 32 * 1024, ncores);
//	for(i = 0; i < 8; i++){
//		printf("%lx\n", src[i]);
//	}
#if 1
	for(i = 0 ; i < ITER; i++){
		create_segment(&src, tput * 1024, ncores);
//		create_src(src, 16 * 1024, ncores);
		mt_x_run(test_filter, ncores, src);

		show_sbuff_resource();
	}

	retire_uarray(global_src);
	free(src);

	printf("Execution Time : %d\n", execution_time);
	printf("avg : %f\n", (float)execution_time / ITER);

	show_sbuff_resource();
#endif
}
#endif
