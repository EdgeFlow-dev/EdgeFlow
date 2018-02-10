#include <err.h>
#include <assert.h>
//#include <pthread.h>
#include <string.h>

#include <stdio.h>
#include <stdlib.h>

/* To the UUID (found the TA's h-file(s)) */
//#include <hello_world_ta.h>
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "common-types.h"
#include "xplane-internal.h"
#include "xplane_lib.h"
#include "xplane/xplane_segment.h"
#include "xplane/xplane_sort.h"
#include "xplane/xplane_sumcount.h"
#include "xplane/xplane_join.h"
#include "xplane/xplane_median.h"
#include "xplane/xplane_misc.h"
#include "xplane/xplane_uarray.h"
#include "xplane/xplane_interface.h"

#include "log.h"

#define MAX_NUM_THREADS 8

uint32_t execution_time = 0;


//TEEC_Context ctx;
//TEEC_UUID uuid = STA_SELF_TEST_UUID;

uint32_t err_origin;

struct thread_arg {
	x_addr **dests;
	x_addr src;
	uint32_t n_outputs;
	uint32_t cmd;
	uint32_t t_idx;
};

/* size = # of elemetns (int32_t) */
x_addr xfunc_dest(uint32_t cmd, uint32_t *nsbuf, uint32_t size){
	int32_t res;
	x_arg args[NUM_ARGS];

	uint64_t out[2] = {0,};

	xzl_assert(cmd < X_CMD_MAX);

	args[1].memref.buffer = out;
	args[1].memref.size = sizeof(out);

	if(nsbuf){
		args[0].value.a = BUF_START;
		args[2].memref.buffer = (void *) nsbuf;
		args[2].memref.size	= sizeof(simd_t) * size;
		res = invoke_command(cmd, args);
#if 0
		if(sizeof(int32_t) * size > 1 << 20){
			args[0].value.a = BUF_START;	/* state start invoke batch open*/
			args[2].memref.buffer = nsbuf;
			args[2].memref.size = 1 << 20;

			res = invoke_command(cmd, args);

			size -= ((1 << 20) / sizeof(int32_t));
			nsbuf += ((1 << 20) / sizeof(int32_t));
			args[2].memref.buffer = nsbuf;

			while(sizeof(int32_t) * size > 1 << 20){
				args[0].value.a = BUF_CONTINUE;	/* state continue */
				args[2].memref.size = 1 << 20;

				res = invoke_command(cmd, args);

				size -= ((1 << 20) / sizeof(int32_t));
				nsbuf += ((1 << 20) / sizeof(int32_t));
				args[2].memref.buffer = nsbuf;
			}
			if(size > 0){
				args[0].value.a = BUF_END;	/* state end invoke batch_close */
				args[2].memref.size = sizeof(int32_t) * size;

				res = invoke_command(cmd, args);
			}
		} else{
			args[0].value.a = BUF_NORMAL;	/* state normal invoke both batch_new and close*/
			args[2].memref.buffer = nsbuf;
			args[2].memref.size = sizeof(int32_t) * size;

			res = invoke_command(cmd, args);
		}
		printf("completed... \n");
#endif
	} else {
		res = invoke_command(cmd, args);
	}

	if(res < 0)
		errx(1, "Invoke segment error");

	return *(x_addr *) args[1].memref.buffer;
}

#if 0
int32_t xfunc_segment(x_addr **dests, xscalar_t **seg_bases, void *args, uint32_t args_size){
	TEEC_Result res;
	TEEC_Operation op;
	TEEC_Session sess;

	x_addr dest_buf[100] = {0,};	 // hp: set the size of dest buffer to get return array
	xscalar_t bases_buf[100] = {0,};

	open_session(&sess);

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_INOUT, \
									 TEEC_MEMREF_TEMP_INOUT, TEEC_MEMREF_TEMP_OUTPUT /* for dest */);

#if 0
	/* pass cmd to TA */
	op.params[0].value.a = cmd;
	op.params[0].value.b = 0;	// tid
#endif
	/* get dest references from TA */
	op.params[1].tmpref.buffer = dest_buf;
	op.params[1].tmpref.size = sizeof(dest_buf);

	/* pass args to TA */
	op.params[2].tmpref.buffer = args;
	op.params[2].tmpref.size = args_size;
#if 1
	/* pass src reference to TA */
	op.params[3].tmpref.buffer = bases_buf;
	op.params[3].tmpref.size = sizeof(bases_buf);
#endif


	res = TEEC_InvokeCommand(&sess, X_CMD_SEGMENT, /* xplane_invoke_command */ &op, &err_origin);
	if(res != TEEC_SUCCESS)
		errx(1, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x", res, err_origin);

	/* dest_buf[0] = # of segments */
//	*dest = (x_addr *) malloc (sizeof(x_addr) * dest_buf[0]);
	*dests = (x_addr *) malloc (op.params[1].tmpref.size);
	*seg_bases = (xscalar_t *) malloc (op.params[3].tmpref.size);

	/* |		seg 0		|		|		seg n		| ... */
	/* | xaddr0  |   xaddr1 | ..... | xaddr0  |  xaddr1 | ... */
	memcpy(*dests, dest_buf, op.params[1].tmpref.size);
	memcpy(*seg_bases, bases_buf, op.params[3].tmpref.size);

	close_session(&sess);

	return op.params[0].value.a;
}
#endif

int32_t invoke_xfunc_segment(x_addr **dests, xscalar_t **seg_bases, void *params, uint32_t params_size){
	int32_t res;
	x_arg args[NUM_ARGS];

	x_addr dest_buf[100] = {0,};	 // hp: set the size of dest buffer to get return array
	xscalar_t bases_buf[100] = {0,};

	DD("xfunc segment : %u", params_size);

	/* get dest references from TA */
	args[1].memref.buffer 	= dest_buf;
	args[1].memref.size 	= sizeof(dest_buf);
	DD("dest_buf size : %ld", sizeof(dest_buf));

	/* pass args to TA */
	args[2].memref.buffer	= params;
	args[2].memref.size 	= params_size;
	DD("params_size : %u", params_size);

	/* pass src reference to TA */
	args[3].memref.buffer 	= bases_buf;
	args[3].memref.size 	= sizeof(bases_buf);
	DD("bases_buf size : %ld", sizeof(bases_buf));

	res = invoke_command(X_CMD_SEGMENT, args);
	if(res != TEE_SUCCESS)
		printf("xfunc_segment failed\n");
//	*dests = (x_addr *) malloc (op.params[1].tmpref.size);
//	*seg_bases = (xscalar_t *) malloc (op.params[3].tmpref.size);
	*dests = (x_addr *) malloc (args[1].memref.size);
	*seg_bases = (xscalar_t *) malloc (args[3].memref.size);

	/* |		seg 0		|		|		seg n		| ... */
	/* | xaddr0  |   xaddr1 | ..... | xaddr0  |  xaddr1 | ... */
//	memcpy(*dests, dest_buf, op.params[1].tmpref.size);
//	memcpy(*seg_bases, bases_buf, op.params[3].tmpref.size);
	memcpy(*dests, dest_buf, args[1].memref.size);
	memcpy(*seg_bases, bases_buf, args[3].memref.size);

	DD("xfunc segment end");
	return args[0].value.a;
}
#if 0
/*
@dests[out]		: outputs addrs from secure world
@n_outputs[in]	: in (# of outputs)
@cmd			: xfunc invoke command
@params			: cmd specific params
@params_size	: size of params */
int32_t invoke_xfunc(void *outbuf, size_t outbuf_size, uint32_t cmd, void *params, size_t params_size){
//int32_t invoke_xfunc(x_addr *dests, uint32_t n_outputs, uint32_t cmd, void *params, uint32_t params_size){
	TEEC_Result res;
	TEEC_Operation op;
	TEEC_Session sess;

	xzl_assert(cmd < X_CMD_MAX);

	open_session(&sess);

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_INOUT, \
									 TEEC_MEMREF_TEMP_INOUT, TEEC_NONE);
	/* cmd and n_outputs */

	/* buffer for returning dest references */
	op.params[1].tmpref.buffer = outbuf;
	op.params[1].tmpref.size = outbuf_size;

	/* buffer for pass src reference */
	op.params[2].tmpref.buffer = params;
	op.params[2].tmpref.size = params_size;

	res = TEEC_InvokeCommand(&sess, cmd, /* xplane_invoke_command */ &op, &err_origin);
	if(res != TEEC_SUCCESS)
		errx(1, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x", res, err_origin);

//	printf("tmpref.buffer : %lx\n", *(x_addr *)op.params[1].tmpref.buffer);
//	printf("dest : %lx\n", dests[0]);

//	printf(_k2clr_red "exec time : %d ms\n" _k2clr_none, op.params[0].value.b);
	DD("exec time : %d ms", op.params[0].value.b); // xzl

	if(cmd == X_CMD_JOIN)
		execution_time += op.params[0].value.b;
	close_session(&sess);

	return op.params[0].value.a; /* error code */
}
#endif

int32_t invoke_xfunc(void *outbuf, size_t outbuf_size, uint32_t cmd, void *params, size_t params_size){
	int32_t res;
	x_arg args[NUM_ARGS];

	xzl_assert(cmd < X_CMD_MAX);

	/* buffer for returning dest references */
	args[1].memref.buffer 	= outbuf;
	args[1].memref.size 	= outbuf_size;

	/* buffer for pass src reference */
	args[2].memref.buffer	= params;
	args[2].memref.size 	= params_size;

	res = invoke_command(cmd, args);

	if(res < 0 )
		errx(1, "invoke_command failed");

//	printf(_k2clr_red "exec time : %d ms\n" _k2clr_none, op.params[0].value.b);
	// DD("exec time : %d ms", args[0].value.b);

	return args[0].value.a;		/* error code */
}

x_addr create_uarray_file(FILE* fp, uint32_t size, idx_t reclen){
	uint32_t *nsbuf;
//	simd_t *nsbuf;
	uint32_t cnt = 0;
	x_addr ret;

	nsbuf = (uint32_t *) malloc (sizeof(uint32_t) * size * reclen);
	printf("read file ... \n");

	while(cnt < size){
#ifdef __x86_64
		if (fscanf(fp, "%ld\n", nsbuf + cnt++) == EOF)
#else
		if (fscanf(fp, "%d\n", nsbuf + cnt++) == EOF)
#endif
			break;
	}
	fclose(fp);

	ret = xfunc_dest(X_CMD_UA_NSBUF, nsbuf, size);

	free(nsbuf);

	return ret;
}


x_addr create_uarray(int flags){
	return xfunc_dest(X_CMD_UA_CREATE, NULL, 0u);
}

x_addr create_uarray_rand(){
	return xfunc_dest(X_CMD_UA_RAND, NULL, 0u);
}

x_addr create_uarray_nsbuf(uint32_t *nsbuf, uint32_t size){
	x_addr ret = xfunc_dest(X_CMD_UA_NSBUF, nsbuf, size);
	I("[core %d] returns. xaddr %lx", sched_getcpu(), ret);
	return ret;
}

int32_t retire_uarray(x_addr r){
	x_params params;
	x_addr unused_dests;
	int32_t ret;

	I("[core %d tid %lu] starts. xaddr %lx",
			sched_getcpu(), syscall(SYS_gettid),
			(unsigned long)r);

	params.srcA = r;

	ret = invoke_xfunc(&unused_dests, 0, X_CMD_UA_RETIRE, (void *)&params, sizeof(params));

	I("[core %d tid %lu] returns. xaddr %lx",
				sched_getcpu(), syscall(SYS_gettid),
				(unsigned long)r);

	return ret;
}

/* caller should allocate memory for dest */
int32_t uarray_to_nsbuf(simd_t *dest, x_addr src, uint32_t *len){
	x_params params;
	size_t dest_size = sizeof(simd_t) * *len;
	int32_t ret;

	I("[core %d tid %lu] starts. &dest %lx src %lx",
			sched_getcpu(), syscall(SYS_gettid),
			(unsigned long)dest, (unsigned long)src);
	params.srcA = src;
	params.n_outputs = *len;	/* use this field as len */

	ret = invoke_xfunc(dest, dest_size, X_CMD_UA_TO_NS, (void *)&params, sizeof(params));
	*len = params.n_outputs;

	I("[core %d] returns", sched_getcpu());
	return ret;
}


int32_t segment(x_addr **dests, xscalar_t **seg_bases, uint32_t *n_segs,
		uint32_t n_outputs,
		x_addr src, uint32_t base, uint32_t subrange,
		idx_t tspos, idx_t reclen){

	seg_arg segarg;
	int32_t ret;

	I("[core %d] starts", sched_getcpu());
	segarg.src	= src;
	segarg.n_outputs = n_outputs;
	segarg.base = base;
	segarg.subrange = subrange;
	segarg.tspos = tspos;
	segarg.reclen = reclen;

	I("seg_arg size : %lu", sizeof(seg_arg));

	ret = invoke_xfunc_segment(dests, seg_bases, (void *)&segarg, sizeof(seg_arg));

	*n_segs = segarg.n_segs;

	I("[core %d] returns", sched_getcpu());
	return ret;
}

#ifdef ARM_NEON
int32_t sort(x_addr *dests, uint32_t n_outputs, x_addr src,
		idx_t keypos, idx_t reclen)
{
	x_params params;
	size_t dests_size = sizeof(x_addr) * n_outputs;
	int32_t ret;

	I("[core %d] starts. src %lx", sched_getcpu(), src);
	params.func = x_sort_t;
	params.srcA = src;
	params.n_outputs = n_outputs;
	params.keypos = keypos;
	params.reclen = reclen;

	ret = invoke_xfunc(dests, dests_size, X_CMD_SORT, (void *)&params, sizeof(params));

	I("[core %d] returns. src %lx dests[0] %lx", sched_getcpu(), src, dests[0]);
//	I("returns");
	return ret;
}

int32_t sort_stable(x_addr *dests, uint32_t n_outputs, x_addr src,
		idx_t keypos, idx_t vpos, idx_t reclen)
{
	x_params params;
	size_t dests_size = sizeof(x_addr) * n_outputs;
	int32_t ret;

	I("[core %d] starts. src %lx", sched_getcpu(), src);
	params.func = qsort_t;
	params.srcA = src;
	params.n_outputs = n_outputs;
	params.keypos = keypos;
	params.vpos = vpos;
	params.reclen = reclen;

	ret = invoke_xfunc(dests, dests_size, X_CMD_SORT, (void *)&params, sizeof(params));

	I("[core %d] returns. src %lx dests[0] %lx", sched_getcpu(), src, dests[0]);
//	I("returns");
	return ret;
}

int32_t merge(x_addr *dests, uint32_t n_outputs, x_addr srcA, x_addr srcB,
		idx_t keypos, idx_t reclen){
	x_params params;
	size_t dests_size = sizeof(x_addr) * n_outputs;
	int32_t ret;

	xzl_assert(n_outputs == 1); // otherwise TBD

	I("[core %d] starts", sched_getcpu());
	params.srcA 		= srcA;
	params.srcB 		= srcB;
	params.n_outputs 	= n_outputs;
	params.keypos 		= keypos;
	params.reclen 		= reclen;

	ret = invoke_xfunc(dests, dests_size, X_CMD_MERGE, (void *)&params, sizeof(params));

	I("[core %d] returns", sched_getcpu());
	return ret;
}
#endif

/* TODO */
int32_t filter_band(x_addr *dests, uint32_t n_outputs, x_addr src,
		simd_t lower, simd_t higher, idx_t vpos, idx_t reclen){
	x_params params;
	size_t dests_size = sizeof(x_addr) * n_outputs;
	int32_t ret;

	I("[core %d] starts", sched_getcpu());
	params.srcA 		= src;
	params.n_outputs 	= n_outputs;
	params.vpos 		= vpos;
	params.reclen		= reclen;

	params.lower 		= lower;
	params.higher 		= higher;

	ret = invoke_xfunc(dests, dests_size, X_CMD_FILTER, (void *)&params, sizeof(params));

	I("[core %d] returns", sched_getcpu());
	return ret;
}


int32_t pseudo_source(x_addr *dest,
		x_addr src, uint32_t src_offset, uint32_t len, /* both in #records */
		int32_t ts_start, int32_t ts_delta,
		idx_t tspos, idx_t reclen
		){
	source_params params;
	size_t dests_size = sizeof(x_addr) * 1; /* one output */
	int32_t ret;

	I("[core %d] starts", sched_getcpu());
	params.cmd 			= X_CMD_UA_PSEUDO;
	params.src 			= src;
	params.src_offset	= src_offset;
	params.count		= len;
	params.reclen		= reclen;
	params.tspos 		= tspos;
	params.ts_start 	= ts_start;
	params.ts_delta 	= ts_delta;

//	printf("[pseudo_source] src : 0x%lx\n", params.src);
	I("[pseudo_source] src : 0x%lx", params.src);

	ret = invoke_xfunc(dest, dests_size, X_CMD_UA_PSEUDO, (void *)&params, sizeof(params));

	I("[core %d] returns", sched_getcpu());
	return ret;
}


/* --- sumcount/avg per key --- */
int32_t sum_perkey(x_addr *dests, uint32_t n_outputs, x_addr src,
		idx_t keypos, idx_t vpos, idx_t reclen){

	x_params params;
	size_t dests_size = sizeof(x_addr) * n_outputs;
	int32_t ret;

	I("[core %d] starts. n_outputs %d", sched_getcpu(), n_outputs);
	params.func 		= sum_perkey_t;
	params.n_outputs	= n_outputs;
	params.srcA 		= src;
	params.keypos 		= keypos;
	params.vpos 		= vpos;
//	params.countpos 	= 0;
	params.reclen 		= reclen;

	ret = invoke_xfunc(dests, dests_size, X_CMD_SUMCOUNT, (void *)&params, sizeof(params));

	I("[core %d] returns", sched_getcpu());
	return ret;
}



int32_t sumcount1_perkey(x_addr *dests, uint32_t n_outputs,
		x_addr src, idx_t keypos, idx_t vpos, idx_t countpos, idx_t reclen){

	x_params params;
	size_t dests_size = sizeof(x_addr) * n_outputs;
	int32_t ret;

	I("[core %d] starts. n_outputs %d", sched_getcpu(), n_outputs);
	params.func 		= sumcount1_perkey_t;
	params.n_outputs	= n_outputs;
	params.srcA 		= src;
	params.keypos 		= keypos;
	params.vpos 		= vpos;
	params.countpos 	= countpos;
	params.reclen 		= reclen;

	ret = invoke_xfunc(dests, dests_size, X_CMD_SUMCOUNT, (void *)&params, sizeof(params));

	I("[core %d] returns", sched_getcpu());
	return ret;
}

int32_t sumcount_perkey(x_addr *dests, uint32_t n_outputs,
		x_addr src,	idx_t keypos, idx_t vpos, idx_t countpos, idx_t reclen){

	x_params params;
	size_t dests_size = sizeof(x_addr) * n_outputs;
	int32_t ret;

	I("[core %d] starts. n_outputs %d", sched_getcpu(), n_outputs);
	params.func 		= sumcount_perkey_t;
	params.n_outputs	= n_outputs;
	params.srcA 		= src;
	params.keypos 		= keypos;
	params.vpos 		= vpos;
	params.countpos 	= countpos;
	params.reclen 		= reclen;

	ret = invoke_xfunc(dests, dests_size, X_CMD_SUMCOUNT, (void *)&params, sizeof(params));

	I("[core %d] returns", sched_getcpu());
	return ret;
}
int32_t avg_perkey(x_addr *dests, uint32_t n_outputs,
		x_addr src,	idx_t keypos, idx_t vpos, idx_t countpos, idx_t reclen){

	x_params params;
	size_t dests_size = sizeof(x_addr) * n_outputs;
	int32_t ret;

	I("[core %d] starts. n_outputs %d", sched_getcpu(), n_outputs);
	params.func		 	= avg_perkey_t;
	params.n_outputs	= n_outputs;
	params.srcA 		= src;
	params.keypos 		= keypos;
	params.vpos 		= vpos;
	params.countpos 	= countpos;
	params.reclen 		= reclen;

	ret = invoke_xfunc(dests, dests_size, X_CMD_SUMCOUNT, (void *)&params, sizeof(params));

	I("[core %d] returns", sched_getcpu());
	return ret;
}

/* --- sumcount/avg all. result: one record  --- */
int32_t sumcount1_all(x_addr *dest, x_addr src,
		idx_t keypos, idx_t vpos, idx_t countpos, idx_t reclen){

	x_params params;
	size_t dests_size = sizeof(x_addr) * 1;
	int32_t ret;

	I("[core %d] starts", sched_getcpu());
	params.func 		= sumcount1_all_t;
	params.n_outputs	= 1;
	params.srcA 		= src;
	params.keypos	 	= keypos;
	params.vpos 		= vpos;
	params.countpos 	= countpos;
	params.reclen 		= reclen;

	ret = invoke_xfunc(dest, dests_size, X_CMD_SUMCOUNT, (void *)&params, sizeof(params));

	I("[core %d] returns", sched_getcpu());
	return ret;
}
int32_t sumcount_all(x_addr *dest, x_addr src,
		idx_t keypos, idx_t vpos, idx_t countpos, idx_t reclen){
	x_params params;
	size_t dests_size = sizeof(x_addr) * 1;
	int32_t ret;

	I("[core %d] starts", sched_getcpu());
	params.func 		= sumcount_all_t;
	params.n_outputs	= 1;
	params.srcA 		= src;
	params.keypos 		= keypos;
	params.vpos 		= vpos;
	params.countpos 	= countpos;
	params.reclen 		= reclen;
	ret = invoke_xfunc(dest, dests_size, X_CMD_SUMCOUNT, (void *)&params, sizeof(params));

	I("[core %d] returns", sched_getcpu());
	return ret;
}
int32_t avg_all(x_addr *dest, x_addr src,
		idx_t keypos, idx_t vpos, idx_t countpos, idx_t reclen){

	x_params params;
	size_t dests_size = sizeof(x_addr) * 1;
	int32_t ret;

	I("[core %d] starts", sched_getcpu());
	params.func 		= avg_all_t;
	params.n_outputs	= 1;
	params.srcA 		= src;
	params.keypos 		= keypos;
	params.vpos 		= vpos;
	params.countpos		= countpos;
	params.reclen 		= reclen;
	ret = invoke_xfunc(dest, dests_size, X_CMD_SUMCOUNT, (void *)&params, sizeof(params));

	I("[core %d] returns", sched_getcpu());
	return ret;
}


int32_t join_bykey(x_addr *dest, x_addr srcA, x_addr srcB, idx_t keypos, idx_t reclen){
	x_params params;
	size_t dests_size = sizeof(x_addr) * 1;
	int32_t ret;

	I("[core %d] starts", sched_getcpu());
	params.func 		= join_bykey_t;
	params.n_outputs	= 1;
	params.srcA 		= srcA;
	params.srcB			= srcB;
	params.keypos 		= keypos;
	params.reclen 		= reclen;
	ret = invoke_xfunc(dest, dests_size, X_CMD_JOIN, (void *)&params, sizeof(params));

	I("[core %d] returns", sched_getcpu());
	return ret;
}

int32_t join_bykey_kp(x_addr *dest_k, x_addr *dest_p,
						x_addr srcA_k, x_addr srcA_p,
						x_addr srcB_k, x_addr srcB_p) {
	batch_t *bsrcA_k = (batch_t *) srcA_k;
	batch_t *bsrcA_p = (batch_t *) srcA_p;

	batch_t *bsrcB_k = (batch_t *) srcB_k;
	batch_t *bsrcB_p = (batch_t *) srcB_p;

	batch_t *bdest_k = batch_new(1 /* unsued */, (bsrcA_k->size + bsrcB_k->size) * sizeof(simd_t));
	batch_t *bdest_p = batch_new(1 /* unsued */, (bsrcA_p->size + bsrcB_p->size) * sizeof(simd_t));

	simd_t *ddest_k = bdest_k->start;
	simd_t *ddest_p = bdest_p->start;

	simd_t matched_key;
	int64_t idxA = 0, idxB = 0, idx_tmp;

	while(idxA < bsrcA_k->size && idxB < bsrcB_k->size) {
		if (bsrcA_k->start[idxA] < bsrcB_k->start[idxB]) {
			++idxA;
		} else if (bsrcA_k->start[idxA] > bsrcB_k->start[idxB]) {
			++idxB;
		} else {	/* keyA == key B (matched) */
			matched_key = bsrcA_k->start[idxA];
//			I("idxA : %ld, idxB : %ld, matched_key : %ld\n", idxA, idxB, matched_key);

			/* copy srcA_k and srcA_p matched with srcB_k */
			idx_tmp = idxA;
			do {
				++idx_tmp;
			} while (bsrcA_k->start[idx_tmp] == matched_key && idx_tmp < bsrcA_k->size);

			memcpy(ddest_k, &(bsrcA_k->start[idxA]), sizeof(simd_t) * (idx_tmp - idxA));
			memcpy(ddest_p, &(bsrcA_p->start[idxA]), sizeof(simd_t) * (idx_tmp - idxA));
			ddest_k += (idx_tmp - idxA);
			ddest_p += (idx_tmp - idxA);
			idxA = idx_tmp;

			/* copy srcB_k and srcB_p matched with srcA_k */
			idx_tmp = idxB;
			do {
				++idx_tmp;
			} while (bsrcB_k->start[idx_tmp] == matched_key && idx_tmp < bsrcB_k->size);

			memcpy(ddest_k, &(bsrcB_k->start[idxB]), sizeof(simd_t) * (idx_tmp - idxB));
			memcpy(ddest_p, &(bsrcB_p->start[idxB]), sizeof(simd_t) * (idx_tmp - idxB));
			ddest_k += (idx_tmp - idxB);
			ddest_p += (idx_tmp - idxB);
			idxB = idx_tmp;
		}
	}

	batch_update(bdest_k, ddest_k);
	batch_update(bdest_p, ddest_p);

	*dest_k = (x_addr) bdest_k;
	*dest_p = (x_addr) bdest_p;

	return 0;
}

 /* -- join by filter -- */
 int32_t join_byfilter(x_addr *dest, x_addr src, x_addr threshold, idx_t vpos, idx_t reclen){
	x_params params;
	size_t dests_size = sizeof(x_addr) * 1;
	int32_t ret;

	I("[core %d] starts", sched_getcpu());
	params.func 		= join_byfilter_t;
	params.n_outputs	= 1;
	params.srcA 		= src;
	params.srcB			= threshold;
	params.vpos 		= vpos;
	params.reclen 		= reclen;
	ret = invoke_xfunc(dest, dests_size, X_CMD_JOIN, (void *)&params, sizeof(params));

	I("[core %d] returns", sched_getcpu());
	return ret;
 }


/* -- med per key TBD -- */
int32_t med_bykey(x_addr *dests, uint32_t n_outputs, x_addr src,
					idx_t keypos, idx_t vpos, idx_t reclen){
	x_params params;
	size_t dests_size = sizeof(x_addr) * n_outputs;
	int32_t ret;

	I("[core %d] starts", sched_getcpu());
	params.func 		= med_bykey_t;
	params.n_outputs	= n_outputs;
	params.srcA 		= src;
	params.keypos		= keypos;
	params.vpos 		= vpos;
	params.reclen 		= reclen;
	ret = invoke_xfunc(dests, dests_size, X_CMD_MEDIAN, (void *)&params, sizeof(params));

	I("[core %d] returns", sched_getcpu());
	return ret;
}

int32_t topk_bykey(x_addr *dests, uint32_t n_outputs, x_addr src,
					idx_t keypos, idx_t vpos, idx_t reclen,
					float k, bool reverse){
	x_params params;
	size_t dests_size = sizeof(x_addr) * n_outputs;
	int32_t ret;

	I("[core %d] starts", sched_getcpu());
	params.func 		= topk_bykey_t;
	params.n_outputs	= n_outputs;
	params.srcA 		= src;
	params.keypos		= keypos;
	params.vpos 		= vpos;
	params.reclen 		= reclen;
	params.k			= k;
	params.reverse		= reverse;
	ret = invoke_xfunc(dests, dests_size, X_CMD_MEDIAN, (void *)&params, sizeof(params));

	I("[core %d] returns", sched_getcpu());
	return ret;
}

int32_t kpercentile_bykey(x_addr *dests, uint32_t n_outputs, x_addr src,
					idx_t keypos, idx_t vpos, idx_t reclen,
					float k){
	x_params params;
	size_t dests_size = sizeof(x_addr) * n_outputs;
	int32_t ret;

	I("[core %d] starts", sched_getcpu());
	params.func 		= kpercentile_bykey_t;
	params.n_outputs	= n_outputs;
	params.srcA 		= src;
	params.keypos		= keypos;
	params.vpos 		= vpos;
	params.reclen 		= reclen;
	params.k			= k;
	ret = invoke_xfunc(dests, dests_size, X_CMD_MEDIAN, (void *)&params, sizeof(params));

	I("[core %d] returns", sched_getcpu());
	return ret;
}

int32_t kpercentile_all(x_addr *dest, x_addr src,
					idx_t keypos, idx_t vpos, idx_t reclen,
					float k){
	x_params params;
	size_t dests_size = sizeof(x_addr) * 1;
	int32_t ret;

	I("[core %d] starts", sched_getcpu());
	params.func 		= kpercentile_all_t;
	params.n_outputs	= 1;
	params.srcA 		= src;
	params.keypos		= keypos;
	params.vpos 		= vpos;
	params.reclen 		= reclen;
	params.k			= k;
	ret = invoke_xfunc(dest, dests_size, X_CMD_MEDIAN, (void *)&params, sizeof(params));

	I("[core %d] returns", sched_getcpu());
	return ret;
}


/* -- med all TBD -- */
int32_t med_all(x_addr *dest, x_addr src,
					idx_t keypos, idx_t vpos, idx_t reclen){
	x_params params;
	size_t dests_size = sizeof(x_addr) * 1;
	int32_t ret;

	I("[core %d] starts", sched_getcpu());
	params.func 		= med_all_t;
	params.n_outputs	= 1;
	params.srcA 		= src;
	params.keypos		= keypos;
	params.vpos 		= vpos;
	params.reclen 		= reclen;
	ret = invoke_xfunc(dest, dests_size, X_CMD_MEDIAN, (void *)&params, sizeof(params));

	I("[core %d] returns", sched_getcpu());
	return ret;
}

int32_t med_all_s(xscalar_t *dest, x_addr src,
					idx_t keypos, idx_t vpos, idx_t reclen){
	x_params params;
	size_t dests_size = sizeof(x_addr) * 1;
	int32_t ret;

	I("[core %d] starts", sched_getcpu());
	params.func 		= med_all_s_t;
	params.n_outputs	= 1;
	params.srcA 		= src;
	params.keypos		= keypos;
	params.vpos 		= vpos;
	params.reclen 		= reclen;
	ret = invoke_xfunc(dest, dests_size, X_CMD_MEDIAN, (void *)&params, sizeof(params));

	I("[core %d] returns", sched_getcpu());
	return ret;
}



/* copy content of @src to @dest. @dest must not be produced. */
/* hp: assume all contents of src is copied into dest */
int32_t concatenate(x_addr dest, x_addr src /* uint32_t src_offset */){
	x_params params;
	x_addr unused_dests;
	int32_t ret;

	I("[core %d] starts", sched_getcpu());
	params.srcA 	= src;	/* source x_addr */
	params.srcB 	= dest; /* dest x_addr */
	ret = invoke_xfunc(&unused_dests, 0, X_CMD_CONCATENATE, (void *)&params, sizeof(params));

	I("[core %d] returns", sched_getcpu());
	return ret;
}

int32_t uarray_size(x_addr src){
	x_params params;
	x_addr unused_dests;
	int32_t ret;

	DD("[core %d] starts", sched_getcpu());
	params.func	= uarray_size_t;
	params.srcA = src;

	ret = invoke_xfunc(&unused_dests, 0, X_CMD_UA_DEBUG, (void *)&params, sizeof(params));

	DD("[core %d] returns", sched_getcpu());
	return ret;
}

int32_t unique_key(x_addr *dest, x_addr src, idx_t keypos, idx_t reclen){
	x_params params;
	size_t dests_size = sizeof(x_addr) * 1;
	int32_t ret;

	I("[core %d] starts", sched_getcpu());
	params.func 		= unique_key_t;
	params.n_outputs	= 1;
	params.srcA 		= src;
	params.keypos 		= keypos;
	params.reclen 		= reclen;
	ret = invoke_xfunc(dest, dests_size, X_CMD_MISC, (void *)&params, sizeof(params));

	I("[core %d] returns", sched_getcpu());
	return ret;
}

void xplane_init(struct pool_desc *p) {
	/* todo: arch specific init */
    /* NB: if !USE_MEM_POOL, this func will emulate pool
     * by keep tracking of allocation w/o doing the actual alloc */
	init_mem_pool_slow(p);
}

void dump_xaddr_info(x_addr x) {
	batch_t *b = (batch_t *)x;
	printf("x_addr %08lx size %ld buf_size %u\n",
	(unsigned long)x, b->size, b->buf_size);
}

#if defined(KNL) || defined(ARCH_AVX2)
#define UNUSED_FLAG 1
x_addr create_batch_merge(int flags, x_addr srca, x_addr srcb){
	batch_t * src_a = (batch_t *) srca;
	batch_t * src_b = (batch_t *) srcb;
	uint32_t merge_size = (src_a->size + src_b->size) * sizeof(simd_t);
	
	batch_t * merge = batch_new(UNUSED_FLAG, merge_size);
	batch_update(merge, merge->start + (merge_size/sizeof(simd_t)));
	return (x_addr)merge;
}
//create expanded batch according to p batch
x_addr create_batch_expand(int flags, x_addr src_p, idx_t reclen){
	if(src_p){	
		batch_t * srcp = (batch_t *) src_p;
		batch_t * expanded_r_batch = batch_new(UNUSED_FLAG, srcp->size * sizeof(simd_t) * reclen);
		batch_update(expanded_r_batch, expanded_r_batch->start + (srcp->size * reclen));
		return (x_addr) expanded_r_batch;
	}else{
		return 0;
	}

}

//uint32_t get_batch_size(x_addr * src){
uint32_t get_batch_size(x_addr src){
	batch_t *src_b = (batch_t *) src;
	if(src_b){
		return src_b->size;
	}else{
		return 0;
	}
}

void free_batch(x_addr src){
	batch_t * src_free = (batch_t *) src;
	BATCH_KILL(src_free, UNUSED_FLAG);
	free(src_free);
}
#endif
