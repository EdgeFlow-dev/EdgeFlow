#include <err.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <pthread.h>

#include "log.h"// xzl
#include "ubench.h"

extern x_addr global_src;

static void test_sort(x_addr src){
	x_addr *sorted_srcA, *sorted_srcB;
	x_addr *dest;
	simd_t *ns_dest;

	uint32_t n_outputs = 1;
	uint32_t len = 60;
	uint32_t i, j, k;

	const idx_t keypos = 1, reclen = 3;

	int32_t ret = 0;

	sorted_srcA = (x_addr *) malloc (sizeof(x_addr) * n_outputs);
	sorted_srcB = (x_addr *) malloc (sizeof(x_addr) * n_outputs);
	ns_dest = (simd_t *) malloc (sizeof(simd_t) * len);

	dest = (x_addr *) malloc (sizeof(x_addr) * n_outputs);

	x_addr merge_destA;
	x_addr merge_destB;
	merge_destA = create_uarray();
//	*sorted_srcB = create_uarray();
	for(int s = 0; s < 100; s ++){
		/** sorted src A **/
		ret = sort(sorted_srcA, n_outputs, src,
				keypos, reclen);
		xzl_bug_on(ret < 0);
		xzl_bug_on(!sorted_srcA);

		/** sorted src B **/
//		*sorted_srcB = create_uarray();

		/** retire src **/

		for(i = 0; i < n_outputs; i++){
			ret = join_bykey(&dest[i], sorted_srcA[i], merge_destA,
					keypos, reclen);

			xzl_bug_on(ret < 0);
			xzl_bug_on(!dest[i]);

			ret = retire_uarray(dest[i]);
			xzl_bug_on(ret < 0);

			ret = merge(&merge_destB, 1, sorted_srcA[i], merge_destA,
					keypos, reclen);

			ret = retire_uarray(merge_destA);
			xzl_bug_on(ret < 0);

			ret = retire_uarray(sorted_srcA[i]);
			xzl_bug_on(ret < 0);

			merge_destA = merge_destB;
		}
	}
	ret = retire_uarray(merge_destA);

	ret = retire_uarray(src);
		xzl_bug_on(ret < 0);

	free(dest);
	free(sorted_srcA);
	free(sorted_srcB);
	free(ns_dest);
}

int main(int argc, char *argv[])
{
	uint32_t *nsbuf;
	x_addr *src;
	uint32_t i;
	const idx_t reclen = 3;	/* record length for global source */

	/**************** generate global src ********************/
	nsbuf = (uint32_t *) malloc (sizeof(uint32_t) * NITEMS * reclen);
	load_file(nsbuf);
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
	//	src = (x_addr *) malloc (sizeof(x_addr) * ncores);

	for(i = 0 ; i < 400; i++){
		create_segment(&src, tput * 1024, ncores);
		//		create_src(src, 16 * 1024, ncores);
		mt_x_run(test_sort, ncores, src);

		show_sbuff_resource();
	}

	retire_uarray(global_src);
	free(src);

	show_sbuff_resource();
#endif
}

