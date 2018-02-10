#if 0

#include <err.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <pthread.h>

/* OP-TEE TEE client API (built by optee_client) */
#include <tee_client_api.h>

/* To the the UUID (found the the TA's h-file(s)) */
#include <hello_world_ta.h>
#include <assert.h>

#include "xplane_lib.h"
#include "log.h"// xzl


#include "xplane_interface.h"

#define MAX_NUM_THREADS 8
#define NITEMS			2 * 1024 * 1024


x_addr global_src;

/* xzl */
static void dump_nsbuf(simd_t const * nsbuf, unsigned slen,
		const char * tag = "", unsigned rlen = 3)
{

	xzl_bug_on(!nsbuf);

	EE("%s", tag);
	for (unsigned i = 0; i < slen; i++) {
		printf("%08x ", *((simd_t *) nsbuf + i));
		if (i % rlen == rlen - 1)
			printf("\t");
		if ((i % (3 * rlen)) + 1 == 3 * rlen)
			printf("\n");
	}
	printf("\n");

}

__attribute__((used))
static void test_unique_key(void){
	x_addr src;
	x_addr *sorted_src;
	x_addr u_dest;
	simd_t *ns_dest;
	uint32_t n_outputs = 1;
	uint32_t len = 60;
	uint32_t i;

	const idx_t keypos = 1, tspos = 0, reclen = 3;

	int32_t ret = 0;

	printf("test unique_key \n");

	sorted_src = (x_addr *) malloc (sizeof(x_addr) * n_outputs);
	ns_dest = (simd_t *) malloc (sizeof(simd_t) * len);

	ret = pseudo_source(&src, global_src,
			0 /*src_offset*/,
			1024 /* len */ ,
			0 /*ts_start */,
			1 /* ts_delta */, tspos, reclen);

	xzl_bug_on(ret < 0);
	xzl_bug_on(!src);

	ret = sort(sorted_src, n_outputs, src,
			keypos, reclen);

	xzl_bug_on(ret < 0);
	xzl_bug_on(!sorted_src);

	ret = unique_key(&u_dest, *sorted_src, keypos, reclen);

	len = 60 * reclen;
	ns_dest = (simd_t *) malloc (sizeof(simd_t) * len);

	ret = uarray_to_nsbuf(ns_dest, u_dest, &len);

	xzl_bug_on(ret < 0);

	EE("len : %d", len);
	for(i = 0 ; i < len; i += 3){
		printf("[i:%d] %6d %6d %6d\n", i/3, ns_dest[i], ns_dest[i+1], ns_dest[i+2]);
	}

	show_sbuff_resource();
}

__attribute__((used))
static void test_sort(void){
	x_addr src;
	x_addr *sorted_src;
	simd_t *ns_dest;
	uint32_t n_outputs = 2;
	uint32_t len = 60;
	uint32_t i, j;
	const int sort_len = 8 * 1024 * 1024;

//	assert(NITEMS > sort_len);

	const idx_t keypos = 1, tspos = 0, reclen = 3;

	int32_t ret = 0;

	printf("test sort. # elements = %u K \n", sort_len / 1024);

	sorted_src = (x_addr *) malloc (sizeof(x_addr) * n_outputs);
	ns_dest = (simd_t *) malloc (sizeof(simd_t) * len);

	ret = pseudo_source(&src, global_src,
			0 /*src_offset*/,
			sort_len,
			0 /*ts_start */,
			1 /* ts_delta */, tspos, reclen);

	xzl_bug_on(ret < 0);
	xzl_bug_on(!src);

	for (int k = 0; k < 10; k++) {
		ret = sort(sorted_src, n_outputs, src,
				keypos, reclen);

		xzl_bug_on(ret < 0);
		xzl_bug_on(!sorted_src);

		for(i = 0 ; i < n_outputs; i++){
			uarray_to_nsbuf(ns_dest, sorted_src[i], &len);
			for(j = 0; j < len; j += 3){
				printf("%6d %6d %6d\n", ns_dest[j], ns_dest[j+1], ns_dest[j+2]);
			}
			printf("\n");

			ret = retire_uarray(sorted_src[i]);
			xzl_bug_on(ret < 0);
		}
		printf("\n");
	}

#if 0
	/* test concatenate */
	concatenate(sorted_src[0], sorted_src[1]);

	len = 2 * 1024 * reclen;
	ns_dest = (simd_t *) malloc (sizeof(simd_t) * len);

	ret = uarray_to_nsbuf(ns_dest, sorted_src[0], &len);

	xzl_bug_on(ret < 0);

	EE("len : %d", len);
	for(j = 1020 * reclen ; j < 1030 * 3; j += 3){
		printf("[j:%d] %6d %6d %6d\n", j/3, ns_dest[j], ns_dest[j+1], ns_dest[j+2]);
		if(j == 3069) EE("---------------");
	}
	/* end concatenate */
#endif

	show_sbuff_resource();

	free(sorted_src);
	free(ns_dest);
}

__attribute__((used))
static void test_merge(void){
	x_addr src;
	x_addr *sorted_src;
	simd_t *ns_dest;
	x_addr dest;
	uint32_t n_outputs = 2;
	uint32_t len = 60;
	uint32_t i;

	const idx_t keypos = 1, tspos = 0, reclen = 3;

	int32_t ret = 0;

	printf("test merge \n");

	sorted_src = (x_addr *) malloc (sizeof(x_addr) * n_outputs);
	ns_dest = (simd_t *) malloc (sizeof(simd_t) * len);

	ret = pseudo_source(&src, global_src,
			0 /*src_offset*/,
			1 * 1024 * 1024 /* len */ ,
			0 /*ts_start */,
			1 /* ts_delta */, tspos, reclen);

	xzl_bug_on(ret < 0);
	xzl_bug_on(!src);

	ret = sort(sorted_src, n_outputs, src,
			keypos, reclen);

	xzl_bug_on(ret < 0);
	xzl_bug_on(!sorted_src);

	len = 60 * reclen;
	ns_dest = (simd_t *) malloc (sizeof(simd_t) * len);

	for(i = 0; i < n_outputs; i += 2){
		ret = merge(&dest,
				1 /* n_outputs */,
				sorted_src[i], sorted_src[i+1],
				keypos, reclen);
		xzl_bug_on(ret < 0);
		printf("dest : %lx\n", dest);
		xzl_bug_on(!dest);

		ret = uarray_to_nsbuf(ns_dest, dest, &len);
		xzl_bug_on(ret < 0);

		dump_nsbuf(ns_dest, len);
	}

	free(sorted_src);
	free(ns_dest);
}

__attribute__((used))
static void test_filter_band(void){
	x_addr src;
	x_addr *dests;
	simd_t *ns_dest;
	uint32_t len = 64;
	uint32_t n_outputs = 2;
	uint32_t i;
	const unsigned lower = 0, higher = 10;

	printf("test filter band lower %u -- higher %u \n", lower, higher);

	/* generate src */
	pseudo_source(&src, global_src, 0/*src_offset*/, 1 * 1024  * 1024/* len */ , 0/*ts_start */, 1/* ts_delta */, 0/* tspos */, 3/* reclen */);

	dests = (x_addr *) malloc (sizeof(x_addr) * n_outputs);
	ns_dest = (simd_t *) malloc (sizeof(simd_t) * len);

	filter_band(dests, 2, src, lower /* lower */, higher /* higher */, 2 /* vpos */, 3);

	for(i = 0 ; i < n_outputs; i++) {

		uarray_to_nsbuf(ns_dest, dests[i], &len);

		EE("output %u:", i);
		dump_nsbuf(ns_dest, len);
	}

	/* test retire */
	retire_uarray(dests[0]);

	auto ret = concatenate(dests[0], dests[1]);
	xzl_bug_on_msg(ret != -X_ERR_BATCH_CLOSED, "it should fail");

	free(ns_dest);
	free(dests);
}

/* xzl */
__attribute__((used))
static void test_segment(void)
{
	x_addr *dests;
	xscalar_t *seg_bases;
	uint32_t n_segs;
	x_addr src;

	int32_t ret;

	const idx_t tspos = 2, reclen = 3;
	const unsigned nout = 1;	/* can change this */
	const unsigned base = 0, subrange = 50 * 1000 * 1000;

	ret = pseudo_source(&src, global_src,
			0 /*src_offset*/,
			1 * 1024 * 1024 /* len */ ,
			10 * 1000 * 1000 /*ts_start */,
			2000 /* ts_delta */, tspos, reclen);

	xzl_bug_on(ret < 0);
	xzl_bug_on(!src);

	ret = segment(&dests, &seg_bases, &n_segs, nout, src, base, subrange,
			tspos, reclen);

	/* verify all */
	xzl_bug_on(ret < 0);
	xzl_bug_on(!n_segs);

	xzl_bug_on(!dests);
	xzl_bug_on(!seg_bases);

	printf("total %u segs\n", n_segs);


	/* readback & dump */
	unsigned len = 2000;
	simd_t * nsbuf = (simd_t *)malloc(sizeof(simd_t) * len);
	xzl_bug_on(!nsbuf);

//	for (unsigned n = 0; n < nout; n++) {
//		W("output %d/%d", n, nout-1);
		for (unsigned i = 0; i < n_segs; i++) {
			printf("segment %u/%u seg_base = %lu\n", i, n_segs - 1, seg_bases[i]);
			xzl_bug_on(!dests[i]);
			ret = uarray_to_nsbuf(nsbuf, dests[i], &len);
			xzl_bug_on(ret < 0);
//			dump_nsbuf(nsbuf, len);
		}
//	}

	free(dests);
	free(seg_bases);
	free(nsbuf);
}

/* xzl */
__attribute__((used))
static void test_sumcount_all(void)
{
	x_addr dest;
	x_addr src;
	int32_t ret;

	const idx_t tspos = 0, keypos = 1, vpos = 2, reclen = 3;

	EE("enters");

	/* for readback ns buf */
	unsigned len = 20;
	simd_t * nsbuf = (simd_t *)malloc(sizeof(simd_t) * len);
	xzl_bug_on(!nsbuf);

	ret = pseudo_source(&src, global_src,
			0/*src_offset*/,
			1 * 1024 /* len */ ,
			0 /*ts_start */,
			1 /* ts_delta */, tspos, reclen);

	xzl_bug_on(ret < 0);
	xzl_bug_on(!src);

	/*
	 * sumcount1_all
	 */
	ret = sumcount1_all(&dest, src, keypos, vpos, tspos, reclen);
	xzl_bug_on(!dest);
	xzl_bug_on(ret < 0);

	/* readback & dump */
	ret = uarray_to_nsbuf(nsbuf, dest, &len);
	xzl_bug_on(ret < 0);

	dump_nsbuf(nsbuf, len, "sumcount1");

	/*
	 * sumcount_all
	 */
	ret = sumcount_all(&dest, src, keypos, vpos, tspos, reclen);
	xzl_bug_on(!dest);
	xzl_bug_on(ret < 0);

	/* readback & dump */
	ret = uarray_to_nsbuf(nsbuf, dest, &len);
	xzl_bug_on(ret < 0);

	dump_nsbuf(nsbuf, len, "sumcount");

	/*
	 * avg_all
	 */
	ret = avg_all(&dest, src, keypos, vpos, tspos, reclen);
	xzl_bug_on(!dest);
	xzl_bug_on(ret < 0);

	/* readback & dump */
	ret = uarray_to_nsbuf(nsbuf, dest, &len);
	xzl_bug_on(ret < 0);

	dump_nsbuf(nsbuf, len, "sumcount");

	/* final */
	free(nsbuf);

	EE("complete");
}

__attribute__((used))
static void test_joinbykey(void)
{
	x_addr dest;
	x_addr src1 = 0, src2 = 0;
//	x_addr *sorted_src1, *sorted_src2;
	x_addr sorted_src1, sorted_src2;
	int32_t ret;
	unsigned src_len = 1024;

	const idx_t tspos = 0, keypos = 1, reclen = 3;

	EE("enters");

	ret = pseudo_source(&src1, global_src,
				0/*src_offset*/,
				src_len /* len */ ,
				0 /*ts_start */,
				1 /* ts_delta */, tspos, reclen);

	xzl_bug_on(ret < 0);
	xzl_bug_on(!src1);

//	sorted_src1 = (x_addr *) malloc (sizeof(x_addr) * 1);
	ret = sort(&sorted_src1, 1 /* n_outputs */, src1 /* src */, 1 /* keypos */, 3 /* reclen */);
	xzl_bug_on(ret < 0);
	xzl_bug_on(!sorted_src1);

	ret = pseudo_source(&src2, global_src,
				0 /*src_offset*/,
				src_len /* len */ ,
				0 /*ts_start */,
				1 /* ts_delta */, tspos, reclen);
	xzl_bug_on(!src2);

	xzl_bug_on(ret < 0);
//	sorted_src2 = (x_addr *) malloc (sizeof(x_addr) * 1);
	ret = sort(&sorted_src2, 1 /* n_outputs */, src1 /* src */, 1 /* keypos */, 3 /* reclen */);
	xzl_bug_on(ret < 0);
	xzl_bug_on(!sorted_src2);

	ret = join_bykey(&dest, sorted_src1, sorted_src2, keypos, reclen);
	xzl_bug_on(ret < 0);
	xzl_bug_on(!dest);

	/* readback & dump */
	unsigned len = 1024;
	simd_t * nsbuf = (simd_t *)malloc(sizeof(simd_t) * len);
	xzl_bug_on(!nsbuf);

	ret = uarray_to_nsbuf(nsbuf, dest, &len);
	xzl_bug_on(ret < 0);
	W("join results has %u simd_t, %u recs", len, len/reclen);

	dump_nsbuf(nsbuf, len, "joinbykey");

	concatenate(dest, sorted_src1);
	free(nsbuf);
}

__attribute__((used))
static void test_joinbyfilter(void)
{
	x_addr dest;
	x_addr src1 = 0, src2 = 0;
	int32_t ret;
	unsigned src_len = 400;

	const idx_t tspos = 0, vpos = 2, reclen = 3;

	EE("enters");

	ret = pseudo_source(&src1, global_src,
				0/*src_offset*/,
				src_len /* len */ ,
				0 /*ts_start */,
				1 /* ts_delta */, tspos, reclen);

	xzl_bug_on(ret < 0);
	xzl_bug_on(!src1);

	ret = pseudo_source(&src2, global_src,
				src_len /*src_offset*/,
				1 /* len, one record only */ ,
				0 /*ts_start */,
				1 /* ts_delta */, tspos, reclen);

	xzl_bug_on(ret < 0);
	xzl_bug_on(!src2);

	ret = join_byfilter(&dest, src1, src2, vpos, reclen);
	xzl_bug_on(ret < 0);
	xzl_bug_on(!dest);

	/* readback & dump */
	unsigned len = 500;
	simd_t * nsbuf = (simd_t *)malloc(sizeof(simd_t) * len);
	xzl_bug_on(!nsbuf);

	ret = uarray_to_nsbuf(nsbuf, dest, &len);
	xzl_bug_on(ret < 0);
	W("join results has %u simd_t, %u recs", len, len/reclen);

	dump_nsbuf(nsbuf, len, "join_byfilter");
}

__attribute__((used))
static void test_readback(void)
{
	x_addr src = 0;
	int32_t ret;
	unsigned src_len = 10;

	const idx_t tspos = 0, reclen = 3;

	EE("enters");

	ret = pseudo_source(&src, global_src,
			0/*src_offset*/,
			src_len,
			0 /*ts_start */,
			1 /* ts_delta */, tspos, reclen);

	xzl_bug_on(ret < 0);
	xzl_bug_on(!src);

	/* readback & dump */
	unsigned len = 80;
	simd_t * nsbuf = (simd_t *)malloc(sizeof(simd_t) * len);
	xzl_bug_on(!nsbuf);

	ret = uarray_to_nsbuf(nsbuf, src, &len);
	xzl_bug_on(ret < 0);
	EE("len is %u", len);
	xzl_bug_on(len != src_len * reclen);

	dump_nsbuf(nsbuf, len, "readback");
}

__attribute__((used))
static void test_sumcount_perkey(void){
	x_addr src;
	x_addr *sorted_src;
	x_addr *dests;
	simd_t *ns_dest;

	uint32_t n_outputs = 1;
	uint32_t len = 60;
//	uint32_t i, j;
	int32_t ret;

	const int source_len = 1024;

	const idx_t tspos = 0, keypos = 1, vpos = 2, reclen = 3;
	const idx_t countpos = 0;

//	printf("test sumcountp perkey \n");

/********** pseudo source *****************/
	ret = pseudo_source(&src, global_src,
				0/*src_offset*/,
				source_len,
				0 /*ts_start */,
				1 /* ts_delta */, tspos, reclen);

	xzl_bug_on(ret < 0);
	xzl_bug_on(!src);

/********** sorted source *****************/
	sorted_src = (x_addr *) malloc (sizeof(x_addr) * n_outputs);
	ret = sort(sorted_src, n_outputs, src, keypos, reclen);

	xzl_bug_on(ret < 0);
	xzl_bug_on(!sorted_src);

	ret = retire_uarray(src);
	xzl_bug_on(ret < 0);

/********** sumcount_perkey  *****************/

	dests = (x_addr *) malloc (sizeof(x_addr) * 4);
	ns_dest = (simd_t *) malloc (sizeof(simd_t) * len);

	ret = kpercentile_bykey(dests,
					4, sorted_src[0],
					keypos, vpos, reclen, 0.4);
#if 0
	ret = sumcount1_perkey(dests,
					4, sorted_src[0],
					keypos, vpos, countpos, reclen);
#endif
	xzl_bug_on(ret < 0);
	xzl_bug_on(!dests);

	ret = retire_uarray(*sorted_src);
	xzl_bug_on(ret < 0);

	x_addr da, db, dc;

	ret = merge(&da, 1, dests[0], dests[1], keypos, reclen);
	xzl_bug_on(ret < 0);

	ret = retire_uarray(dests[0]);
	ret = retire_uarray(dests[1]);

	ret = merge(&db, 1, dests[2], dests[3], keypos, reclen);
	xzl_bug_on(ret < 0);

	ret = retire_uarray(dests[2]);
	ret = retire_uarray(dests[3]);

	ret = merge(&dc, 1, da, db, keypos, reclen);
	xzl_bug_on(ret < 0);

	ret = retire_uarray(da);
	ret = retire_uarray(db);

	ret = retire_uarray(dc);
	xzl_bug_on(ret < 0);


#if 0
	for(i = 0 ; i < n_outputs; i++){
		printf("dests[%d] = %lx\n", i, dests[i]);
		auto ret = uarray_to_nsbuf(ns_dest, dests[i], &len);
		xzl_bug_on(ret < 0);
		for(j = 0; j < len; j += 3){
			printf("%6d %6d %6d\n", ns_dest[j], ns_dest[j+1], ns_dest[j+2]);
		}
		printf("\n");
	}

	sumcount_perkey(dests, n_outputs, sorted_src[0],
					keypos, vpos, countpos, reclen);

	for(i = 0 ; i < n_outputs; i++){
		printf("dests[%d] = %lx\n", i, dests[i]);
		uarray_to_nsbuf(ns_dest, dests[i], &len);
		for(j = 0; j < len; j += 3){
			printf("%6d %6d %6d\n", ns_dest[j], ns_dest[j+1], ns_dest[j+2]);
		}
		printf("\n");
	}

	avg_perkey(dests, n_outputs, sorted_src[0],
					keypos, vpos, countpos, reclen);

	for(i = 0 ; i < n_outputs; i++){
		printf("dests[%d] = %lx\n", i, dests[i]);
		uarray_to_nsbuf(ns_dest, dests[i], &len);
		for(j = 0; j < len; j += 3){
			printf("%6d %6d %6d\n", ns_dest[j], ns_dest[j+1], ns_dest[j+2]);
		}
		printf("\n");
	}
#endif

	free(sorted_src);
	free(ns_dest);
	free(dests);
}

__attribute__((used))
static void test_med_bykey(void){
	x_addr src;
	x_addr *sorted_src;
	x_addr *dests;
	simd_t *ns_dest;

	uint32_t n_outputs = 1;
	uint32_t len = 60;
	uint32_t i, j;
	int32_t ret;

	const int source_len = 1024;

	const idx_t tspos = 0, keypos = 1, vpos = 2, reclen = 3;
	const idx_t countpos = 0;

	printf("test sumcountp perkey \n");

/********** pseudo source *****************/
	ret = pseudo_source(&src, global_src,
				0/*src_offset*/,
				source_len,
				0 /*ts_start */,
				1 /* ts_delta */, tspos, reclen);

	xzl_bug_on(ret < 0);
	xzl_bug_on(!src);

/********** sorted source *****************/
	sorted_src = (x_addr *) malloc (sizeof(x_addr) * n_outputs);
	ret = sort(sorted_src, n_outputs, src, keypos, reclen);

	xzl_bug_on(ret < 0);
	xzl_bug_on(!sorted_src);

	ret = retire_uarray(src);
	xzl_bug_on(ret < 0);

	dests = (x_addr *) malloc (sizeof(x_addr) * 4);
	ns_dest = (simd_t *) malloc (sizeof(simd_t) * len);

	ret = med_bykey(dests,
					4, sorted_src[0],
					keypos, vpos, reclen);

	xzl_bug_on(ret < 0);
	xzl_bug_on(!dests);

	uarray_to_nsbuf(ns_dest, sorted_src[0], &len);
	for(j = 0; j < len; j += 3){
		printf("%6d %6d %6d\n", ns_dest[j], ns_dest[j+1], ns_dest[j+2]);
	}
	printf("\n");

	ret = retire_uarray(*sorted_src);
	xzl_bug_on(ret < 0);

	for(i = 0 ; i < 4; i++){
		printf("dests[%d] = %lx\n", i, dests[i]);
		uarray_to_nsbuf(ns_dest, dests[i], &len);
		for(j = 0; j < len; j += 3){
			printf("%6d %6d %6d\n", ns_dest[j], ns_dest[j+1], ns_dest[j+2]);
		}
		printf("\n");
	}

}

struct thread_arg {
	uint8_t id;
	void (*test_func)(void);
};

void *x_run(void *arg){
	struct thread_arg *t_arg = (struct thread_arg *)arg;
	void (*test)(void) = t_arg->test_func;
	uint32_t i;

	for(i = 0;i < 1000 * 1000; i++){
		test();
		if(t_arg->id == 0) {
			show_sbuff_resource();
			; /* nothing */
		}
//		usleep(100 * 1000);
	}

	return NULL;
}

#if 0
void x_run(void (*test_func)(void)){
	uint32_t i;
	for(i = 0;i < 10000; i++){
		test_func();
		usleep(10);
	}
}
#endif

void mt_x_run(void (*pfunc)(void), size_t num_threads){
	pthread_t thr[num_threads];
	struct thread_arg arg[num_threads];
	size_t n;

	for(n = 0; n < num_threads; n++){
		arg[n].id = n;
		arg[n].test_func = pfunc;
		if(pthread_create(thr + n, NULL, x_run, arg + n) != 0)
			err(1, "create failn\n");
	}

	for(n = 0; n < num_threads; n++){
		if(pthread_join(thr[n], NULL) != 0)
			err(1, "join failed\n");
	}
	printf("all done\n");
}


int main(int argc, char *argv[])
{
	uint32_t *nsbuf;
	uint32_t cnt = 0;

	FILE *fp;

	/* randomly generated input which has 1000K items (composed of 3 elements) */
	fp = fopen("input_x3.txt","r");	/* input file for 3 elements in item */
	if (!fp){
		printf("file open failed errno : %p\n", fp);
		return 0;
	}
#if 1
	nsbuf = (uint32_t *) malloc (sizeof(uint32_t) * NITEMS * ELES_PER_ITEM);
	printf("read file ... \n");

	while(cnt < NITEMS){
//		if (fscanf(fp, "%d\n", nsbuf + cnt++) == EOF)
		if (fscanf(fp, "%d %d %d\n", nsbuf + (cnt * ELES_PER_ITEM),
					nsbuf + (cnt * ELES_PER_ITEM + 1),
					nsbuf + (cnt * ELES_PER_ITEM + 2)) == EOF)

			break;
		cnt++;
	}
	printf("nsbuf : %p, cnt : %d\n", (void *)nsbuf, cnt);
	fclose(fp);
#endif


/* init ctx and populate map */
//	open_context();
//	populate_map();
/*****************************/

	global_src = create_uarray_nsbuf(nsbuf, NITEMS * ELES_PER_ITEM /* sizeof(int32_t) */);


	show_sbuff_resource();

//	test_med_bykey();

#if 0
	int ncores = 1;
	if (argc == 2)
		ncores = atoi(argv[1]);
	printf("to run mt_x_run() ncores =%d\n", ncores);
	mt_x_run(test_sumcount_perkey, ncores);
#endif

//	test_unique_key();
	test_sumcount_perkey();
//	test_segment();

	show_sbuff_resource();

//	test_sort();
//	test_joinbykey();
//	test_filter_band();
//	test_merge();

//	test_readback();

//		test_sumcount_all();

//	test_joinbyfilter();

#if 0
	uarray_to_nsbuf(ns_dest, sorted_arr[0], &len);
	for(int i = 0; i < len; i += 3){
		printf("%d %d %d ", ns_dest[i], ns_dest[i+1], ns_dest[i+2]);
	}
	printf("\n");


	sumcount_perkey(&s_dest, 2, sorted_arr[0], 1, 2, 0, 3);
	printf("s_dest : %llx\n", s_dest);
	uarray_to_nsbuf(ns_dest, s_dest, &len);
	for(int i = 0; i < 3; i++)
		printf("%d ", ns_dest[i]);
	printf("\n");


	free(sorted_arr);


	sumcount1_all(&s_dest, sorted_arr[0], 1, 2, 0, 3);
	printf("s_dest : %llx\n", s_dest);
	uarray_to_nsbuf(ns_dest, s_dest, &len);
	for(int i = 0; i < 3; i++)
		printf("%d ", ns_dest[i]);
	printf("\n");

	sumcount_all(&s_dest, sorted_arr[0], 1, 2, 0, 3);
	printf("s_dest : %llx\n", s_dest);
	uarray_to_nsbuf(ns_dest, s_dest, &len);
	for(int i = 0; i < 3; i++)
		printf("%d ", ns_dest[i]);
	printf("\n");;


	free(sorted_arr);

	join_bykey(&s_dest, src1, src1, 1, 3);
	join_byfilter(&s_dest, src1, src1, 2, 3);

	sumcount1_perkey(&s_dest, 1 /* n_outputs */, src1, 1, 2, 0, 3);
	sumcount_perkey(&s_dest, 1 /* n_outputs */, src1, 1, 2, 0, 3);
	avg_perkey(&s_dest, 1 /* n_outputs */, src1, 1, 2, 0, 3);

	sumcount1_all(&s_dest, src1, 1, 2, 0, 3);
	sumcount_all(&s_dest, src1, 1, 2, 0, 3);
	avg_all(&s_dest, src1, 1, 2, 0, 3);

	med_bykey(&s_dest, 1, src1, 1, 2, 3);
	med_all(&s_dest, src1, 1, 2, 3);
	med_all_s(&s_dest, src1, 1, 2, 3);

	concatenate(src1, src1);
#endif


/* destroy map and finish ctx */
//	kill_map();
//	close_context();
/******************************/
}

#include <signal.h>

/*
 * abort handling.
 * by xzl
 */

static int is_xplane_init = 0; /* XXX race condition? */

static void abort_handler(int i)
{
	if (is_xplane_init) {
		EE("cleaning up....");
		kill_map();
		EE("mapped killed");
		close_context();
		EE("context closed");
		is_xplane_init = 0;
	}

	exit(1);
}

__attribute__((constructor))
void xplane_init(void)
{
	if (signal(SIGABRT, abort_handler) == SIG_ERR) {
		fprintf(stderr, "Couldn't set signal handler\n");
		exit(1);
	}

	if (signal(SIGINT, abort_handler) == SIG_ERR) {
		fprintf(stderr, "Couldn't set signal handler\n");
		exit(1);
	}

	open_context();
	populate_map();

	EE("init OK");

	is_xplane_init = 1;
}

/* will be called when main() ends or exit() called */
__attribute__((destructor))
void xplane_fini(void)
{
	if (is_xplane_init) {
		EE("cleaning up....");
		kill_map();
		EE("mapped killed");
		close_context();
		EE("context closed");
		is_xplane_init = 0;
	}
}
/*
 * end of abort handling
 */

#endif

int main(){
	return 1;
}

