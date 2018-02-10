#include <err.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <pthread.h>
#include <mm/batch.h>
#include "log.h"// xzl
#include <xplane/index.h>
#include "xplane-internal.h"
#include "ubench.h"

//#define QSORT

extern x_addr global_src;
extern uint32_t execution_time;

static void test_sort(x_addr src){
    x_addr *tmp_src, *tmp_srck, *tmp_srcp;
	x_addr *sorted_src;
        x_addr srck, srcp;
	simd_t *ns_dest;

	uint32_t n_outputs = 1;
	uint32_t len = 60;
	uint32_t i, j;
#if defined X1_type
	const idx_t keypos = 0;	/* for x1 */
#elif defined X3_type
	const idx_t keypos = 1; /* for x3 */
#endif

	int32_t ret = 0;

	printf("len : %d, keypos : %d\n", RECLEN, keypos);

	sorted_src = (x_addr *) malloc (sizeof(x_addr) * n_outputs);
	tmp_src = (x_addr *) malloc (sizeof(x_addr) * n_outputs);
	tmp_srck = (x_addr *) malloc (sizeof(x_addr) * n_outputs);
	tmp_srcp = (x_addr *) malloc (sizeof(x_addr) * n_outputs);
	ns_dest = (simd_t *) malloc (sizeof(simd_t) * len);

	I("src : 0x%lx", src);
	uarray_to_nsbuf(ns_dest, src, &len);
	for(j = 0; j < len; j += 3){
#ifdef __x86_64
			printf("%6ld %6ld %6ld\n", ns_dest[j], ns_dest[j+1], ns_dest[j+2]);
#else
			printf("%6d %6d %6d\n", ns_dest[j], ns_dest[j+1], ns_dest[j+2]);
#endif
	}
	printf("\n");
    extract_kp(&srck, &srcp, src, keypos, RECLEN);

	for(i = 0; i < ((batch_t *)srck)->size ; i++) {
		printf("%ld\n", ((batch_t *)srck)->start[i]);
	}

	printf("----------------------\n");

	ret = sort_kp(tmp_srck, tmp_srcp, n_outputs, srck, srcp);
	xzl_bug_on(ret < 0);
	xzl_bug_on(!tmp_srck);

	/* filter_band_kp test */
	/* ------------------------------------- */
	x_addr dests_p[3];
	x_addr ttt[3];

	ret = filter_band_kp(dests_p, 3, tmp_srck[0], tmp_srcp[0], 0, 150);

	for (i = 0; i < 3; i++) {
		deref_k(&ttt[i], dests_p[i], 1, RECLEN);
	}

	for(i = 0 ; i < 3; i++){
		printf("[i:%d] addr : %lx, size : %d\n", i, ttt[i], uarray_size(ttt[i]));
		uarray_to_nsbuf(ns_dest, ttt[i], &len);
		for(j = 0; j < len; j += 3){
#ifdef __x86_64
			printf("%6ld %6ld %6ld\n", ns_dest[j], ns_dest[j+1], ns_dest[j+2]);
#else
			printf("%6d %6d %6d\n", ns_dest[j], ns_dest[j+1], ns_dest[j+2]);
#endif
		}
		printf("\n");
	}
	/* --------------------------- */

	for (i = 0; i < n_outputs; ++i) {
		//            expand_p(&tmp_src[i], tmp_srcp[i], RECLEN);
		deref_k(&tmp_src[i], tmp_srcp[i], 1, RECLEN);
	}


	I("tmp_src : 0x%lx", *tmp_src);
	for(i = 0 ; i < n_outputs; i++){
		printf("[i:%d] addr : %lx, size : %d\n", i, tmp_src[i], uarray_size(tmp_src[i]));
		uarray_to_nsbuf(ns_dest, tmp_src[i], &len);
		for(j = 0; j < len; j += 3){
#ifdef __x86_64
			printf("%6ld %6ld %6ld\n", ns_dest[j], ns_dest[j+1], ns_dest[j+2]);
#else
			printf("%6d %6d %6d\n", ns_dest[j], ns_dest[j+1], ns_dest[j+2]);
#endif
		}
		printf("\n");
	}

        for(i = 0; i < n_outputs; i++) {
            ret = retire_uarray(tmp_src[i]);
            xzl_bug_on(ret < 0);
            ret = retire_uarray(tmp_srck[i]);
            xzl_bug_on(ret < 0);
            ret = retire_uarray(tmp_srcp[i]);
            xzl_bug_on(ret < 0);
        }
#if 0
	ret = sort_stable(sorted_src, n_outputs, *tmp_src,
			2, keypos, RECLEN);

	ret = retire_uarray(*tmp_src);
	xzl_bug_on(ret < 0);


	xzl_bug_on(ret < 0);
	xzl_bug_on(!sorted_src);
	I("sorted src : 0x%lx", sorted_src[0]);

	for(i = 0 ; i < n_outputs; i++){
		printf("[i:%d] addr : %lx, size : %d\n", i, sorted_src[i], uarray_size(sorted_src[i]));
		uarray_to_nsbuf(ns_dest, sorted_src[i], &len);
		for(j = 0; j < len; j += 3){
#ifdef __x86_64
			printf("%6ld %6ld %6ld\n", ns_dest[j], ns_dest[j+1], ns_dest[j+2]);
#else
			printf("%6d %6d %6d\n", ns_dest[j], ns_dest[j+1], ns_dest[j+2]);
#endif
		}
		printf("\n");

		ret = retire_uarray(sorted_src[i]);
		xzl_bug_on(ret < 0);
	}
	printf("\n");
#endif
        ret = retire_uarray(src);
        xzl_bug_on(ret < 0);
        ret = retire_uarray(srck);
        xzl_bug_on(ret < 0);
	ret = retire_uarray(srcp);
	xzl_bug_on(ret < 0);

	free(sorted_src);
	free(tmp_src);
        free(tmp_srck);
        free(tmp_srcp);
	free(ns_dest);
}

int main(int argc, char *argv[])
{
	//x86_test_sort();

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

	free(nsbuf);
}

