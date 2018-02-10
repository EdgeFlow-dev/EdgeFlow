#include <err.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <pthread.h>

#include "log.h"// xzl
#include "ubench.h"

extern x_addr global_src;

static void test_merge(x_addr src){
	x_addr *sorted_src;
	simd_t *ns_dest;
	struct timespec t_start, t_stop;

	uint32_t n_outputs = 4;
	uint32_t len = 60;
	uint32_t i, j;
	const idx_t keypos = 1, reclen = 3;

	int32_t ret = 0;

	sorted_src = (x_addr *) malloc (sizeof(x_addr) * n_outputs);
	ns_dest = (simd_t *) malloc (sizeof(simd_t) * len);

	clock_gettime(CLOCK_MONOTONIC, &t_start);
	ret = sort(sorted_src, n_outputs, src,
			keypos, reclen);
	clock_gettime(CLOCK_MONOTONIC, &t_stop);

	xzl_bug_on(ret < 0);
	xzl_bug_on(!sorted_src);
	EE("sort time %.5f ms", get_exec_time_ms(t_start, t_stop));

	x_addr da, db, dc;

#if 1
	clock_gettime(CLOCK_MONOTONIC, &t_start);
	ret = merge(&da, 1, sorted_src[0], sorted_src[1], keypos, reclen);
	xzl_bug_on(ret < 0);

	ret = retire_uarray(sorted_src[0]);
	ret = retire_uarray(sorted_src[1]);

	ret = merge(&db, 1, sorted_src[2], sorted_src[3], keypos, reclen);
	xzl_bug_on(ret < 0);

	ret = retire_uarray(sorted_src[2]);
	ret = retire_uarray(sorted_src[3]);


	ret = merge(&dc, 1, da, db, keypos, reclen);
	xzl_bug_on(ret < 0);
	ret = retire_uarray(da);
	ret = retire_uarray(db);

	ret = retire_uarray(dc);
	xzl_bug_on(ret < 0);

	clock_gettime(CLOCK_MONOTONIC, &t_stop);

#endif

//	ret = retire_uarray(sorted_src[0]);
//	xzl_bug_on(ret < 0);


	ret = retire_uarray(src);
	xzl_bug_on(ret < 0);

	EE("merge time %.5f ms", get_exec_time_ms(t_start, t_stop));

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
	global_src = create_uarray_nsbuf(nsbuf, NITEMS /* rec num */ * ELES_PER_ITEM /* reclen */);
	/*********************************************************/

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
	src = (x_addr *) malloc (sizeof(x_addr) * ncores);

	for(i = 0 ; i < 20; i++){
//		create_segment(&src, tput * 1024, ncores);
//		create_src(src, 16 * 1024, ncores);
		create_src(src, tput, ncores);
		mt_x_run(test_merge, ncores, src);

		show_sbuff_resource();
	}

	retire_uarray(global_src);
	free(src);

	show_sbuff_resource();
#endif
}

