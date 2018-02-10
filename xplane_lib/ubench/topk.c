#include <err.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <pthread.h>

#include "log.h"// xzl
#include "ubench.h"

extern x_addr global_src;

static void test_topk(x_addr src){
	x_addr *sorted_src;
	x_addr *dests;
	simd_t *ns_dest;

	uint32_t n_outputs = 4;
	uint32_t len = 768;
	uint32_t i, j;

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

	/*****************************************************/
	int topk_nout = 1;
	dests = (x_addr *) malloc (sizeof(x_addr) * topk_nout);
	ns_dest = (simd_t *) malloc (sizeof(simd_t) * len);

	uarray_to_nsbuf(ns_dest, sorted_src[0], &len);
	for(j = 0; j < len; j += 3){
		printf("%6d %6d %6d\n", ns_dest[j], ns_dest[j+1], ns_dest[j+2]);
	}
	printf("\n\n");

	for(i = 0 ; i < n_outputs; i++){
#if 1
		ret = kpercentile_all(dests, sorted_src[i],
				keypos, vpos, reclen,
				0.50);
#endif
#if 0
		ret = kpercentile_bykey(dests,
				1, sorted_src[i],
				keypos, vpos, reclen,
				0.50);
#endif
#if 0
		ret = topk_bykey(dests,
				1, sorted_src[i],
				keypos, vpos, reclen,
				0.40, false);
#endif
		xzl_bug_on(ret < 0);
		xzl_bug_on(!dests);

		ret = retire_uarray(sorted_src[i]);
		xzl_bug_on(ret < 0);

		uarray_to_nsbuf(ns_dest, dests[0], &len);
		for(j = 0; j < 3; j += 3){
			printf("%6d %6d %6d\n", ns_dest[j], ns_dest[j+1], ns_dest[j+2]);
		}
		for(j = 0; j < topk_nout; j++){
			printf("[j:%d] addr : %lx, size : %d\n",
		          j, dests[j], uarray_size(dests[j]));

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
//	src = (x_addr *) malloc (sizeof(x_addr) * ncores);

	for(i = 0 ; i < 1; i++){
		create_segment(&src, tput * 1024, ncores);
//		create_src(src, 16 * 1024, ncores);
		mt_x_run(test_topk, ncores, src);

		show_sbuff_resource();
	}

	retire_uarray(global_src);
	free(src);

	show_sbuff_resource();
#endif
}

