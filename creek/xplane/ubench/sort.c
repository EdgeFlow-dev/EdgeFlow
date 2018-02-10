#include <err.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <pthread.h>

#include "log.h"// xzl

#include "xplane-internal.h"
#include "ubench.h"

//#define QSORT

extern x_addr global_src;
extern uint32_t execution_time;

#if 0
void x86_test_sort(){
	printf("test_sort(): \n");
	int reclen = 3;
	int record_num = 100;
	batch_t * src_batch = (batch_t *) malloc(sizeof(batch_t));

	src_batch->size = record_num * reclen;
	src_batch->start = (simd_t *) malloc(sizeof(simd_t) * src_batch->size);
	
	for(int i = 0; i < record_num; i++){	
		for(int j = 0; j < reclen; j++){
			src_batch->start[i*reclen +j] = 200 - i; 
		}
	}
	
	x_addr src = (x_addr) (src_batch);
	x_addr *dests;
	uint64_t n_outputs = 3;
	dests = (x_addr *) malloc(sizeof(x_addr) * n_outputs);
	
	sort(dests, n_outputs, src, 0, reclen);

	batch_t *dst_ptr;
	for(int i = 0; i < n_outputs; i++){
		printf("sorted out \n");
		dst_ptr = (batch_t *) dests[i];
		printf("dst_ptr->size/reclen is %d\n", dst_ptr->size/reclen);
		for(uint64_t j = 0; j < dst_ptr->size/reclen; j++){
#ifdef __x86_64 
			printf("<%ld, %ld, %ld>,",dst_ptr->start[j*reclen], dst_ptr->start[j*reclen +1], dst_ptr->start[j*reclen +2]);	
#else
			printf("<%d, %d, %d>,",dst_ptr->start[j*reclen], dst_ptr->start[j*reclen +1], dst_ptr->start[j*reclen +2]);	
#endif
		}
		printf("\n\n");
	}
}
#endif

__attribute__((__used__))
static void bw_test(x_addr src){
	x_addr *sorted_src;
	uint32_t n_outputs = 1;
	uint32_t i;

#if defined X1_type
	const idx_t keypos = 0;	/* for x1 */
#elif defined X3_type
	const idx_t keypos = 1; /* for x3 */
#endif

	int32_t ret = 0;

	I("len : %d, keypos : %d\n", RECLEN, keypos);

	sorted_src = (x_addr *) malloc (sizeof(x_addr) * n_outputs);

	I("src : 0x%lx", src);

	ret = sort(sorted_src, n_outputs, src,
			keypos, RECLEN);
	xzl_bug_on(ret < 0);
	xzl_bug_on(!sorted_src);

	for(i = 0; i < n_outputs; i++) {
		ret = retire_uarray(sorted_src[i]);
		xzl_bug_on(ret < 0);
	}

	free(sorted_src);
}

__attribute__((__used__))
static void test_sort(x_addr src){
	x_addr *tmp_src;
	x_addr *sorted_src;
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

#if 0
	ret = sort_stable(sorted_src, n_outputs, src,
			keypos, 0, RECLEN);
#else
	ret = sort(tmp_src, n_outputs, src,
			keypos, RECLEN);
	xzl_bug_on(ret < 0);
	xzl_bug_on(!tmp_src);


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
#endif
//	ret = retire_uarray(src);
	xzl_bug_on(ret < 0);

	free(sorted_src);
	free(tmp_src);
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

//		mt_x_run(test_sort, ncores, src);
		mt_x_run(bw_test, ncores, src);

		show_sbuff_resource();
	}
	retire_uarray(global_src);
	free(src);

	printf("Execution Time : %d\n", execution_time);
	printf("avg : %f\n", (float)execution_time / ITER);
	show_sbuff_resource();

	free(nsbuf);
}

