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

extern x_addr global_src;
extern uint32_t execution_time;

static void test_sort(x_addr src){
	x_addr *sorted_srcA;
	x_addr *sorted_srcB;

	x_addr srcAk, srcAp;
	x_addr srcBk, srcBp;

	x_addr *sorted_srcAk, *sorted_srcAp;
	x_addr *sorted_srcBk, *sorted_srcBp;

	x_addr *dest_k, *dest_p;
	x_addr *dest;

	simd_t *ns_dest;

	uint32_t n_outputs = 1;
	uint32_t len = 120;
	uint32_t i, j;
#if defined X1_type
	const idx_t keypos = 0;	/* for x1 */
#elif defined X3_type
	const idx_t keypos = 1; /* for x3 */
#endif
	int32_t ret = 0;

	printf("len : %d, keypos : %d\n", RECLEN, keypos);

	sorted_srcA = (x_addr *) malloc (sizeof(x_addr) * n_outputs);
	sorted_srcAk = (x_addr *) malloc (sizeof(x_addr) * n_outputs);
	sorted_srcAp = (x_addr *) malloc (sizeof(x_addr) * n_outputs);

	sorted_srcB = (x_addr *) malloc (sizeof(x_addr) * n_outputs);
	sorted_srcBk = (x_addr *) malloc (sizeof(x_addr) * n_outputs);
	sorted_srcBp = (x_addr *) malloc (sizeof(x_addr) * n_outputs);

	dest_k = (x_addr *) malloc (sizeof(x_addr) * n_outputs);
	dest_p = (x_addr *) malloc (sizeof(x_addr) * n_outputs);

	dest = (x_addr *) malloc (sizeof(x_addr) * n_outputs);

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

	/* ############################################################## */
	/* extract src -> sort_kp -> sorted_srcAk, sorted_srcAp*/
	/* ############################################################## */
	ret = extract_kp(&srcAk, &srcAp, src, keypos, RECLEN);
	xzl_bug_on(ret < 0);

	ret = sort_kp(sorted_srcAk, sorted_srcAp, n_outputs, srcAk, srcAp);
	xzl_bug_on(ret < 0);
	xzl_bug_on(!sorted_srcAk);

	/* ############################################################## */
	/* extract src -> sort_kp -> sorted_srcBk, sorted_srcBp*/
	/* ############################################################## */
	ret = extract_kp(&srcBk, &srcBp, src, keypos, RECLEN);
	xzl_bug_on(ret < 0);

	ret = sort_kp(sorted_srcBk, sorted_srcBp, n_outputs, srcBk, srcBp);
	xzl_bug_on(ret < 0);
	xzl_bug_on(!sorted_srcBk);

	/* ############################################################## */
	/* run join_bykey_kp -> expand */
	/* ############################################################## */
	for (i = 0; i < n_outputs; ++i) {
		ret = join_bykey_kp(&dest_k[i], &dest_p[i],
				sorted_srcAk[i], sorted_srcAp[i],
				sorted_srcBk[i], sorted_srcBp[i]);
		xzl_bug_on(ret < 0);
		ret = expand_p(&dest[i], dest_p[i], RECLEN);
		xzl_bug_on(ret < 0);

		for(j = 0; j < len; j += 3){
#ifdef __x86_64
			printf("%6ld %6ld %6ld\n", ((batch_t *)dest[i])->start[j],
					((batch_t *)dest[i])->start[j + 1], ((batch_t *)dest[i])->start[j + 2]);
#else
			printf("%6d %6d %6d\n", ((batch_t *)dest[i])->start[j],
					((batch_t *)dest[i])->start[j + 1], ((batch_t *)dest[i])->start[j + 2]);
#endif
		}
		printf("\n");

		retire_uarray(srcAk);
		xzl_bug_on(ret < 0);
		retire_uarray(srcAp);
		xzl_bug_on(ret < 0);
		retire_uarray(srcBk);
		xzl_bug_on(ret < 0);
		retire_uarray(srcBp);
		xzl_bug_on(ret < 0);

		retire_uarray(dest_k[i]);
		xzl_bug_on(ret < 0);
		retire_uarray(dest_p[i]);
		xzl_bug_on(ret < 0);

		retire_uarray(sorted_srcAk[i]);
		xzl_bug_on(ret < 0);
		retire_uarray(sorted_srcAp[i]);
		xzl_bug_on(ret < 0);
		retire_uarray(sorted_srcBk[i]);
		xzl_bug_on(ret < 0);
		retire_uarray(sorted_srcBp[i]);
		xzl_bug_on(ret < 0);
	}

	for (i = 0; i < n_outputs; i++) {
		ret = retire_uarray(dest[i]);
		xzl_bug_on(ret < 0);
	}

	free(sorted_srcA); free(sorted_srcAk); free(sorted_srcAp);
	free(sorted_srcB); free(sorted_srcBk); free(sorted_srcBp);
	free(dest); free(dest_k); free(dest_p);
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
		create_src(src, tput * 1024, ncores);
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

