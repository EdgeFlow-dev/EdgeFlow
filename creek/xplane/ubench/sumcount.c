#include <err.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <pthread.h>

#include "log.h"// xzl
#include "ubench.h"

extern x_addr global_src;
extern uint32_t execution_time;

static void test_sumcount(x_addr src){
	x_addr *dests;
	simd_t *ns_dest;

	int sumcount_nout = 1;
	uint32_t len = 60;
	uint32_t i;

	const idx_t keypos = 1, vpos = 2, reclen = 3;
	const idx_t countpos = 0;

	int32_t ret = 0;

	ns_dest = (simd_t *) malloc (sizeof(simd_t) * len);

	dests = (x_addr *) malloc (sizeof(x_addr) * sumcount_nout);
	ns_dest = (simd_t *) malloc (sizeof(simd_t) * len);

	ret = sumcount1_all(dests, src,
			keypos, vpos, countpos, reclen);

#if 0
	ret = sum_perkey(dests,
			sumcount_nout, sorted_src[i],
			keypos, vpos, reclen);
#endif
#if 0
	ret = sumcount1_perkey(dests,
			sumcount_nout, sorted_src[i],
			keypos, vpos, countpos, reclen);
#endif
	xzl_bug_on(ret < 0);
	xzl_bug_on(!dests);

	uarray_to_nsbuf(ns_dest, dests[0], &len);
	for(i = 0; i < len; i += 3){
#ifdef __x86_64
		printf("%6ld %6ld %6ld\n", ns_dest[i], ns_dest[i+1], ns_dest[i+2]);
#else		
		printf("%6d %6d %6d\n", ns_dest[i], ns_dest[i+1], ns_dest[i+2]);
#endif
	}
	for(i = 0; i < sumcount_nout; i++){
		ret = retire_uarray(dests[i]);
		xzl_bug_on(ret < 0);
	}

	printf("\n");

	ret = retire_uarray(src);
	xzl_bug_on(ret < 0);


	free(dests);
	free(ns_dest);
}


__attribute__((used))
static void test_sumcount_with_sort(x_addr src){
	x_addr *sorted_src;
	x_addr *dests;
	simd_t *ns_dest;

	uint32_t n_outputs = 1;
	uint32_t len = 60;
	uint32_t i, j;

	const idx_t keypos = 1, vpos = 2, reclen = 3;

	int32_t ret = 0;

	sorted_src = (x_addr *) malloc (sizeof(x_addr) * n_outputs);
	ns_dest = (simd_t *) malloc (sizeof(simd_t) * len);

	ret = sort(sorted_src, n_outputs, src,
			keypos, reclen);

	xzl_bug_on(ret < 0);
	xzl_bug_on(!sorted_src);

	ret = retire_uarray(src);
	xzl_bug_on(ret < 0);

	int sumcount_nout = 4;
	dests = (x_addr *) malloc (sizeof(x_addr) * sumcount_nout);
	ns_dest = (simd_t *) malloc (sizeof(simd_t) * len);

	for(i = 0 ; i < n_outputs; i++){
		uarray_to_nsbuf(ns_dest, sorted_src[i], &len);
		for(j = 0; j < len; j += 3){
//			printf("%6d %6d %6d\n", ns_dest[j], ns_dest[j+1], ns_dest[j+2]);
		}
	printf("\n");

#if 1
	ret = sum_perkey(dests,
				sumcount_nout, sorted_src[i],
				keypos, vpos, reclen);
#else
	const idx_t countpos = 0;
	ret = sumcount1_perkey(dests,
			sumcount_nout, sorted_src[i],
			keypos, vpos, countpos, reclen);
#endif
		xzl_bug_on(ret < 0);
		xzl_bug_on(!dests);

		ret = retire_uarray(sorted_src[i]);
		xzl_bug_on(ret < 0);

		uarray_to_nsbuf(ns_dest, dests[0], &len);
		for(j = 0; j < len; j += 3){
#ifdef __x86_64
			printf("%6ld %6ld %6ld\n", ns_dest[j], ns_dest[j+1], ns_dest[j+2]);
#else
			printf("%6d %6d %6d\n", ns_dest[j], ns_dest[j+1], ns_dest[j+2]);
#endif
		}
		for(j = 0; j < sumcount_nout; j++){
			ret = retire_uarray(dests[j]);
			xzl_bug_on(ret < 0);
		}

		printf("\n");
	}

	free(dests);
	free(sorted_src);
	free(ns_dest);
}

int main(int argc, char *argv[])
{
	simd_t *nsbuf;
	x_addr *src;
	uint32_t i;
	const idx_t reclen = 3;	/* record length for global source */

	/**************** generate global src ********************/
	nsbuf = (simd_t *) malloc (sizeof(simd_t) * NITEMS * reclen);
	if(load_file(nsbuf) < 0){
		free(nsbuf);
		return -1;
	}
	global_src = create_uarray_nsbuf(nsbuf, NITEMS * reclen);
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
//		create_segment(&src, tput * 1024, ncores);

		src = (x_addr *) malloc (sizeof(x_addr) * ncores);
		create_src(src, tput * 1024, ncores);
		mt_x_run(test_sumcount, ncores, src);

		show_sbuff_resource();
	}

	retire_uarray(global_src);
	free(src);

	printf("Execution Time : %d\n", execution_time);
	printf("avg : %f\n", (float)execution_time / ITER);
	show_sbuff_resource();
#endif
}

