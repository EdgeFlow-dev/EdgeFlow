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

static void test_join(x_addr src){
	x_addr *sorted_srcA, *sorted_srcB;
	x_addr *dest;
	simd_t *ns_dest;

	uint32_t n_outputs = 1;
	uint32_t len = 60;
	uint32_t i, j;

	const idx_t keypos = 1, reclen = 3;

	int32_t ret = 0;

	sorted_srcA = (x_addr *) malloc (sizeof(x_addr) * n_outputs);
	sorted_srcB = (x_addr *) malloc (sizeof(x_addr) * n_outputs);
	ns_dest = (simd_t *) malloc (sizeof(simd_t) * len);

	/** sorted src A **/
	ret = sort(sorted_srcA, n_outputs, src,
			keypos, reclen);
	xzl_bug_on(ret < 0);
	xzl_bug_on(!sorted_srcA);

	/** sorted src B **/
	ret = sort(sorted_srcB, n_outputs, src,
			keypos, reclen);
	xzl_bug_on(ret < 0);
	xzl_bug_on(!sorted_srcB);

	/** retire src **/
	ret = retire_uarray(src);
	xzl_bug_on(ret < 0);


	dest = (x_addr *) malloc (sizeof(x_addr) * n_outputs);

	for(i = 0; i < n_outputs; i++){
#if 0
		ret = join_filter(&dest[i], sorted_srcA[i], sorted_srcB[i],
				keypos, reclen);
#endif
#if 1
		ret = join_bykey(&dest[i], sorted_srcA[i], sorted_srcB[i],
				keypos, reclen);
#endif

		xzl_bug_on(ret < 0);
		xzl_bug_on(!dest[i]);

		ret = retire_uarray(sorted_srcA[i]);
		xzl_bug_on(ret < 0);

		ret = retire_uarray(sorted_srcB[i]);
		xzl_bug_on(ret < 0);

		uarray_to_nsbuf(ns_dest, dest[i], &len);
			for(j = 0; j < len; j += 3){
#ifdef __x86_64
				printf("%6ld %6ld %6ld\n", ns_dest[j], ns_dest[j+1], ns_dest[j+2]);
#else	
				printf("%6d %6d %6d\n", ns_dest[j], ns_dest[j+1], ns_dest[j+2]);
#endif
			}
			printf("\n");

		ret = retire_uarray(dest[i]);
		xzl_bug_on(ret < 0);
	}


	free(dest);
	free(sorted_srcA);
	free(sorted_srcB);
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

	int tput = 1;
	int ncores = 8;
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
//		create_segment(&src, tput * 1024, ncores);

		src = (x_addr *) malloc (sizeof(x_addr) * ncores);
		create_src(src, tput * 1024, ncores);
		mt_x_run(test_join, ncores, src);

		show_sbuff_resource();
	}

	retire_uarray(global_src);
	free(src);

	printf("Execution Time : %d\n", execution_time);
	printf("avg : %f\n", (float)execution_time / ITER);
	show_sbuff_resource();
#endif
}

