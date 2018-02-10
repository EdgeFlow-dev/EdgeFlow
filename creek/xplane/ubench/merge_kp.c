#include <err.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <pthread.h>

#include <xplane/index.h>
#include "log.h"// xzl
#include "ubench.h"

extern x_addr global_src;

static void test_merge(x_addr src){
    x_addr *sorted_srck, *sorted_srcp;
	simd_t *ns_dest;
        x_addr srck, srcp;
	uint32_t n_outputs = 4;
	uint32_t len = 60;
	uint32_t j;
	const idx_t keypos = 1, reclen = 3;

	int32_t ret = 0;

        sorted_srck = (x_addr *) malloc (sizeof(x_addr) * n_outputs);
	sorted_srcp = (x_addr *) malloc (sizeof(x_addr) * n_outputs);
        
	ns_dest = (simd_t *) malloc (sizeof(simd_t) * len);

        extract_kp(&srck, &srcp, src, keypos, reclen);
	ret = sort_kp(sorted_srck, sorted_srcp, n_outputs, srck, srcp);

	xzl_bug_on(ret < 0);

/* 	for(i = 0; i < n_outputs; i++){ */
/* 		uarray_to_nsbuf(ns_dest, sorted_src[i], &len); */
/* 		for(j = 0; j < len; j += 3){ */
/* #ifdef __x86_64 */
/* 			printf("%6ld %6ld %6ld\n", ns_dest[j], ns_dest[j+1], ns_dest[j+2]); */
/* #else */
/* 			printf("%6d %6d %6d\n", ns_dest[j], ns_dest[j+1], ns_dest[j+2]); */
/* #endif */
/* 		} */
/* 		printf("\n"); */
/* 	} */

	x_addr dak, dap, dbk, dbp, dck, dcp, out;

	ret = merge_kp(&dak, &dap, 1, sorted_srck[0], sorted_srcp[0],
                    sorted_srck[1], sorted_srcp[1]);
	xzl_bug_on(ret < 0);

	ret = retire_uarray(sorted_srck[0]);
	ret = retire_uarray(sorted_srcp[0]);
	ret = retire_uarray(sorted_srck[1]);
	ret = retire_uarray(sorted_srcp[1]);

	/* uarray_to_nsbuf(ns_dest, da, &len); */
/* 	for(j = 0; j < len; j += 3){ */
/* #ifdef __x86_64 */
/* 		printf("%6ld %6ld %6ld\n", ns_dest[j], ns_dest[j+1], ns_dest[j+2]); */
/* #else */
/* 		printf("%6d %6d %6d\n", ns_dest[j], ns_dest[j+1], ns_dest[j+2]); */
/* #endif */
/* 	} */
/* 	printf("\n"); */

	ret = merge_kp(&dbk, &dbp, 1, sorted_srck[2], sorted_srcp[2],
                    sorted_srck[3], sorted_srcp[3]);
	xzl_bug_on(ret < 0);

	ret = retire_uarray(sorted_srck[2]);
	ret = retire_uarray(sorted_srcp[2]);
	ret = retire_uarray(sorted_srck[3]);
	ret = retire_uarray(sorted_srcp[3]);

	ret = merge_kp(&dck, &dcp, 1, dak, dap, dbk, dbp);
	xzl_bug_on(ret < 0);

	ret = retire_uarray(dak);
	ret = retire_uarray(dap);
	ret = retire_uarray(dbk);
	ret = retire_uarray(dbp);

        expand_p(&out, dcp, reclen);
	uarray_to_nsbuf(ns_dest, out, &len);
	for(j = 0; j < len; j += 3){
#ifdef __x86_64
		printf("%6ld %6ld %6ld\n", ns_dest[j], ns_dest[j+1], ns_dest[j+2]);
#else
		printf("%6d %6d %6d\n", ns_dest[j], ns_dest[j+1], ns_dest[j+2]);
#endif
	}
	printf("\n");

	ret = retire_uarray(dck);
	xzl_bug_on(ret < 0);
        ret = retire_uarray(dcp);
        xzl_bug_on(ret < 0);

	ret = retire_uarray(srck);
	xzl_bug_on(ret < 0);
        ret = retire_uarray(srcp);
	xzl_bug_on(ret < 0);
        ret = retire_uarray(out);
	xzl_bug_on(ret < 0);

	free(sorted_srck);
        free(sorted_srcp);
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
	int tput = 64;
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

	for(i = 0 ; i < ITER; i++){
//		create_segment(&src, tput * 1024, ncores);
//
		src = (x_addr *) malloc (sizeof(x_addr) * ncores);
		create_src(src, tput * 1024, ncores);
		mt_x_run(test_merge, ncores, src);

		show_sbuff_resource();
	}

	retire_uarray(global_src);
	free(src);

	show_sbuff_resource();
#endif
}

