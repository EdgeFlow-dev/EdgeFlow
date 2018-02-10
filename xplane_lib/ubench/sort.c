#include <err.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <pthread.h>

#include "log.h"// xzl
#include "ubench.h"

//#define QSORT

#if defined X1
#define RECLEN 1
#elif defined X3
#define RECLEN 3
#endif

extern x_addr global_src;
extern execution_time;

static void test_sort(x_addr src){
	x_addr *tmp_src;
	x_addr *sorted_src;
	simd_t *ns_dest;

	uint32_t n_outputs = 1;
	uint32_t len = 60;
	uint32_t i, j;
#if defined X1
	const idx_t keypos = 0;	/* for x1 */
#elif defined X3
	const idx_t keypos = 1; /* for x3 */
#endif

	int32_t ret = 0;

	printf("len : %d, keypos : %d\n", RECLEN, keypos);

	sorted_src = (x_addr *) malloc (sizeof(x_addr) * n_outputs);
	tmp_src = (x_addr *) malloc (sizeof(x_addr) * n_outputs);
	ns_dest = (simd_t *) malloc (sizeof(simd_t) * len);

	I("src : 0x%lx", src);
	uarray_to_nsbuf(ns_dest, src, &len);
	for(j = 0; j < len; j += 3){
		printf("%6d %6d %6d\n", ns_dest[j], ns_dest[j+1], ns_dest[j+2]);
	}
	printf("\n");

#ifdef QSORT
	ret = x_qsort(sorted_src, n_outputs, src,
			keypos, 0, RECLEN);
#else
	ret = sort(tmp_src, n_outputs, src,
			keypos, RECLEN);
	xzl_bug_on(ret < 0);
	xzl_bug_on(!tmp_src);

	I("tmp_src : 0x%lx", *tmp_src);
	for(i = 0 ; i < n_outputs; i++){
		printf("[i:%d] addr : %lx, size : %d\n", i, sorted_src[i], uarray_size(sorted_src[i]));
		uarray_to_nsbuf(ns_dest, tmp_src[i], &len);
		for(j = 0; j < len; j += 3){
			printf("%6d %6d %6d\n", ns_dest[j], ns_dest[j+1], ns_dest[j+2]);
		}
		printf("\n");
	}

	ret = x_qsort(sorted_src, n_outputs, *tmp_src,
			2, keypos, RECLEN);

	ret = retire_uarray(*tmp_src);
	xzl_bug_on(ret < 0);
#endif


	xzl_bug_on(ret < 0);
	xzl_bug_on(!sorted_src);
	I("sorted src : 0x%x", sorted_src);

	for(i = 0 ; i < n_outputs; i++){
		printf("[i:%d] addr : %lx, size : %d\n", i, sorted_src[i], uarray_size(sorted_src[i]));
		uarray_to_nsbuf(ns_dest, sorted_src[i], &len);
		for(j = 0; j < len; j += 3){
			printf("%6d %6d %6d\n", ns_dest[j], ns_dest[j+1], ns_dest[j+2]);
		}
		printf("\n");

		ret = retire_uarray(sorted_src[i]);
		xzl_bug_on(ret < 0);
	}
	printf("\n");

	ret = retire_uarray(src);
	xzl_bug_on(ret < 0);

	free(sorted_src);
	free(ns_dest);
}

int main(int argc, char *argv[])
{
	uint32_t *nsbuf;
	x_addr *src;
	uint32_t i;

	/**************** generate global src ********************/
	nsbuf = (uint32_t *) malloc (sizeof(uint32_t) * NITEMS * RECLEN);

#if defined X1
	load_file_x1(nsbuf);
#elif defined X3
	load_file(nsbuf);
#endif

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


//	create_segment(&src, 32 * 1024, ncores);
//	for(i = 0; i < 8; i++){
//		printf("%lx\n", src[i]);
//	}
#if 1


	for(i = 0 ; i < ITER; i++){
#if 0
//		create_segment(&src, tput * 1024, ncores);
#else
		src = (x_addr *) malloc (sizeof(x_addr) * ncores);
		create_src(src, tput, ncores);
#endif

		mt_x_run(test_sort, ncores, src);

		show_sbuff_resource();
	}
	retire_uarray(global_src);
	free(src);

	printf("Execution Time : %d\n", execution_time);
	printf("avg : %f\n", (float)execution_time / ITER);
	show_sbuff_resource();
#endif

	free(nsbuf);
}

