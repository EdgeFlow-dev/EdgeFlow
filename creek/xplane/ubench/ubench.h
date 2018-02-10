#ifndef UBENCH_H
#define UBENCH_H

#include <pthread.h>
#include "log.h"
#include "xplane_lib.h"

#define ITER		1
#define NITEMS		1 * 1024 * 1024

//#define X1_type
#define X3_type

#if defined X3_type
#define RECLEN	3
#elif defined X1_type
#define RECLEN 	1
#endif

#define show_sbuff_resource()

x_addr global_src;

struct thread_arg {
	uint8_t id;
	void (*test_func)(x_addr src);
	x_addr src;
};

static inline void create_src(x_addr *src, int src_len, int n_src){
	uint32_t i;
	static int src_offset = 0;
	const uint32_t ts_start = 0, ts_delta = 1;
	const idx_t tspos = 0;
#if defined X1_type
	const idx_t reclen = 1;
#elif defined X3_type
	const idx_t reclen = 3;
#endif

	int32_t ret = 0;

	I("reclen : %d", reclen);
	I("global src : %lx", global_src);
	for(i = 0 ; i < n_src; i++){
		ret = pseudo_source(&src[i], global_src,
				src_offset,
				src_len,
				ts_start, ts_delta,
				tspos, reclen);

		xzl_bug_on(ret < 0);
		xzl_bug_on(!src);
	}
	src_offset += src_len;
}

static inline void create_segment(x_addr **dests, uint32_t src_len, uint8_t ncores){
	x_addr src;
//	x_addr *dests;
	xscalar_t *seg_bases;
	uint32_t n_segs;
//	uint32_t src_len = 32 * 1024;

	int32_t ret;

	const idx_t tspos = 0, reclen = 3;
	const unsigned nout = 1;	/* can change this */
	const unsigned base = 0, subrange = src_len / ncores;

	create_src(&src, src_len, 1 /* # of src */);
	I("src addr : %lx", src);

	ret = segment(dests, &seg_bases, &n_segs, nout,
					src, base, subrange, tspos, reclen);

	/* verify all */
	xzl_bug_on(ret < 0);
	xzl_bug_on(!n_segs);

	retire_uarray(src);

	xzl_bug_on(!dests);
	xzl_bug_on(!seg_bases);

	printf("total %u segs\n", n_segs);


	/* readback & dump */
	unsigned len = 20;
	simd_t * nsbuf = (simd_t *)malloc(sizeof(simd_t) * len);
	xzl_bug_on(!nsbuf);

	for (unsigned n = 0; n < nout; n++) {
		//		W("output %d/%d", n, nout-1);
		for (unsigned i = 0; i < n_segs; i++) {
			printf("segment %u/%u seg_base = %lu\n", i, n_segs - 1, seg_bases[i]);
			xzl_bug_on(!(*dests)[i]);
			printf("addr %lx size : %d \n", (*dests)[i], uarray_size((*dests)[i]));
			ret = uarray_to_nsbuf(nsbuf, (*dests)[i], &len);
			xzl_bug_on(ret < 0);
//			dump_nsbuf(nsbuf, len);
		}
	}

//	free(dests);
	free(seg_bases);
	free(nsbuf);
}



static inline void *x_run(void *arg){
	struct thread_arg *t_arg = (struct thread_arg *)arg;
	void (*test)(x_addr src) = t_arg->test_func;
	x_addr src = t_arg->src;
	uint32_t i;

	for(i = 0 ; i < 1; i++){
		test(src);
	}
	//	if(t_arg->id == 0) {
	//		show_sbuff_resource();
	//usleep(100 * 1000);
	//	}
	return NULL;
}

// num_threads = # of src
__attribute__((unused))
static void mt_x_run(void (*pfunc)(x_addr src), size_t num_threads, x_addr *src){
	pthread_t thr[num_threads];
	struct thread_arg arg[num_threads];
	size_t n;

	for(n = 0; n < num_threads; n++){
		arg[n].id = n;
		arg[n].test_func = pfunc;
		arg[n].src = src[n];
		if(pthread_create(thr + n, NULL, x_run, arg + n) != 0)
			EE("create failn");
	}

	for(n = 0; n < num_threads; n++){
		if(pthread_join(thr[n], NULL) != 0)
			EE("join failed");
	}
}

__attribute__((unused))
static int32_t load_file_x1(simd_t *nsbuf){
	FILE *fp;
	uint32_t cnt = 0;

	fp = fopen("input_x1.txt", "r");

	if(!fp){
		printf("file open failed err : %p\n", fp);
		return -1;
	}

	I("read file ...");

	while(cnt < NITEMS){
#ifdef __x86_64
		if (fscanf(fp, "%ld\n", nsbuf + cnt) == EOF)
#else
		if (fscanf(fp, "%d\n", nsbuf + cnt) == EOF)
#endif
			break;
		cnt++;
	}
	printf("nsbuf : %p, cnt : %d\n", (void *)nsbuf, cnt);
	fclose(fp);

	return 0;
}

__attribute__((used))
static int32_t load_file(simd_t *nsbuf){
	FILE *fp;
	uint32_t cnt = 0;

	fp = fopen("input_x3.txt", "r");

	if(!fp){
		printf("file open failed err : %p\n", fp);
		return -1;
	}

	I("read file ...");

	while(cnt < NITEMS){
#ifdef __x86_64
		if (fscanf(fp, "%ld %ld %ld\n", nsbuf + (cnt * RECLEN),
#else
		if (fscanf(fp, "%d %d %d\n", nsbuf + (cnt * RECLEN),
#endif
					nsbuf + (cnt * RECLEN + 1),
					nsbuf + (cnt * RECLEN + 2)) == EOF)
			break;
		cnt++;
	}
	printf("nsbuf : %p, cnt : %d\n", (void *)nsbuf, cnt);
	fclose(fp);

	return 0;
}






#if 0
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

#endif
