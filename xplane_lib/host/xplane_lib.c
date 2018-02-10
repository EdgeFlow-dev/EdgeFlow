#include <err.h>
#include <assert.h>
#include <pthread.h>
#include <string.h>


#include <sys/syscall.h>
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>

/* To the UUID (found the TA's h-file(s)) */
#include <hello_world_ta.h>
#include <unistd.h>

#include "armtz-types.h"
#include "xplane_lib.h"
#include "log.h"

#define MAX_NUM_THREADS 8
#define CHUNK_SIZE_TO_SECURE	(64 << 20)

uint32_t execution_time = 0;


TEEC_Context ctx;
TEEC_UUID uuid = STA_SELF_TEST_UUID;

uint32_t err_origin;

struct thread_arg {
	x_addr **dests;
	x_addr src;
	uint32_t n_outputs;
	uint32_t cmd;
	uint32_t t_idx;
};

/* Initialize a context connecting us to the TEE */
void open_context(void){
	TEEC_Result res;
	res = TEEC_InitializeContext(NULL, &ctx);
	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_InitializeContext failed with cod 0x%x", res);
}

void open_session(TEEC_Session *sess){
	TEEC_Result res;
	res = TEEC_OpenSession(&ctx, sess, &uuid, /* uuid is for xplane application */
					TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);

	if (res != TEEC_SUCCESS)
		printf("xzl: something is wrong");

	assert(res == TEEC_SUCCESS);

	if(res != TEEC_SUCCESS)
		errx(1, "TEEC_Opensession failed with code 0x%x origin 0x%x",
			res, err_origin);
}

void close_session(TEEC_Session *sess){
	TEEC_CloseSession(sess);
}

void close_context(void){
	TEEC_FinalizeContext(&ctx);
}

void populate_map(void){
	xfunc_cmd_only(X_CMD_MAP);
}

void kill_map(void){
	xfunc_cmd_only(X_CMD_KILL);
}

void show_sbuff_resource(void){
	xfunc_cmd_only(X_CMD_SHOW_SBUFF);
}

int32_t xfunc_cmd_only(uint32_t cmd){
	TEEC_Result res;
	TEEC_Operation op;
	TEEC_Session sess;

	open_session(&sess);

	memset(&op, 0, sizeof(op));

	op.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE, TEEC_NONE, TEEC_NONE);

	res = TEEC_InvokeCommand(&sess, cmd, /* xplane_invoke_command */ &op, &err_origin);
	if(res != TEEC_SUCCESS)
		errx(1, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x", res, err_origin);

	close_session(&sess);

	return 1;
}

/* size = # of elemetns (int32_t) */
x_addr xfunc_dest(uint32_t cmd, uint32_t *nsbuf, uint32_t size){
	TEEC_Result res;
	TEEC_Operation op;
	TEEC_Session sess;

	uint64_t out[2] = {0,};

	xzl_assert(cmd < X_CMD_MAX);

	open_session(&sess);

	memset(&op, 0, sizeof(op));

	/* Prepare the argument */
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_INOUT, \
									 TEEC_MEMREF_TEMP_INOUT, TEEC_NONE);
	op.params[1].tmpref.buffer = out;
	op.params[1].tmpref.size = sizeof(out);

	if(nsbuf){
		if(sizeof(int32_t) * size > CHUNK_SIZE_TO_SECURE){
			op.params[0].value.a = BUF_START;	/* state start invoke batch open*/
			op.params[2].tmpref.buffer = nsbuf;
			op.params[2].tmpref.size = CHUNK_SIZE_TO_SECURE;

			res = TEEC_InvokeCommand(&sess, cmd, /* xplane_invoke_command */ &op, &err_origin);
			if(res != TEEC_SUCCESS)
				errx(1, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x", res, err_origin);

			size -= ((CHUNK_SIZE_TO_SECURE) / sizeof(int32_t));
			nsbuf += ((CHUNK_SIZE_TO_SECURE) / sizeof(int32_t));
			op.params[2].tmpref.buffer = nsbuf;

//			op.params[2].tmpref.buffer += (1 << 20);
//			printf("size : %d\n", size);
			while(sizeof(int32_t) * size >CHUNK_SIZE_TO_SECURE){
				op.params[0].value.a = BUF_CONTINUE;	/* state continue */
				op.params[2].tmpref.size =CHUNK_SIZE_TO_SECURE;


				res = TEEC_InvokeCommand(&sess, cmd, /* xplane_invoke_command */ &op, &err_origin);
				if(res != TEEC_SUCCESS)
					errx(1, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x", res, err_origin);

				size -= ((CHUNK_SIZE_TO_SECURE) / sizeof(int32_t));
//				nsbuf += (1 << 20);
//				op.params[2].tmpref.buffer = nsbuf;
				nsbuf += ((CHUNK_SIZE_TO_SECURE) / sizeof(int32_t));
				op.params[2].tmpref.buffer = nsbuf;
//				op.params[2].tmpref.buffer += (1 << 20);
//				printf("size : %d\n", size);
			}
			if(size > 0){
				op.params[0].value.a = BUF_END;	/* state end invoke batch_close */
				op.params[2].tmpref.size = sizeof(int32_t) * size;

				res = TEEC_InvokeCommand(&sess, cmd, /* xplane_invoke_command */ &op, &err_origin);
				if(res != TEEC_SUCCESS)
					errx(1, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x", res, err_origin);
			}
		} else{
			op.params[0].value.a = BUF_NORMAL;	/* state normal invoke both batch_new and close*/
			op.params[2].tmpref.buffer = nsbuf;
			op.params[2].tmpref.size = sizeof(int32_t) * size;

			res = TEEC_InvokeCommand(&sess, cmd, /* xplane_invoke_command */ &op, &err_origin);
			if(res != TEEC_SUCCESS)
				errx(1, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x", res, err_origin);
		}
		//printf("completed... \n");
	} else {

		res = TEEC_InvokeCommand(&sess, cmd, /* xplane_invoke_command */ &op, &err_origin);
		if(res != TEEC_SUCCESS)
			errx(1, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x", res, err_origin);
	}
//	printf("size of value : %d\n", sizeof(op.params[0].value.a));
//	printf("value : %lx\n", op.params[0].value.a);

	close_session(&sess);
	return *(x_addr *) op.params[1].tmpref.buffer;
}


int32_t xfunc_dest_src_src(uint32_t cmd, x_addr *dest, x_addr srcA, x_addr srcB){
	TEEC_Result res;
	TEEC_Operation op;
	TEEC_Session sess;

	uint64_t dest_buf[2] = {0,};
	uint64_t srcA_buf[2] = {0,};
	uint64_t srcB_buf[2] = {0,};

	open_session(&sess);

	srcA_buf[0] = srcA;
	srcB_buf[0] = srcB;

	memset(&op, 0, sizeof(op));

	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_INOUT, \
									 TEEC_MEMREF_TEMP_INOUT, TEEC_MEMREF_TEMP_INOUT);
	op.params[0].value.a = cmd;
	op.params[0].value.b = 0;	// tid

	op.params[1].tmpref.buffer = dest_buf;
	op.params[1].tmpref.size = sizeof(dest_buf);

	op.params[2].tmpref.buffer = srcA_buf;
	op.params[2].tmpref.size = sizeof(srcA_buf);

	op.params[3].tmpref.buffer = srcB_buf;
	op.params[3].tmpref.size = sizeof(srcB_buf);

	res = TEEC_InvokeCommand(&sess, cmd, /* xplane_invoke_command */ &op, &err_origin);
	if(res != TEEC_SUCCESS)
		errx(1, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x", res, err_origin);
//	return *(x_addr *) op.params[1].tmpref.buffer;

	*dest = *(x_addr *) op.params[1].tmpref.buffer;
//	printf(RED "exe time : %d\n" RESET, op.params[0].value.a);

	close_session(&sess);

	return op.params[0].value.a; /* execution time */
}

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
//	op.params[1].tmpref.size = sizeof(x_addr) * n_outputs;

	/* buffer for pass src reference */
	op.params[2].tmpref.buffer = params;
	op.params[2].tmpref.size = params_size;

//	printf("dest : %lx src : %lx\n", *dests, src);

	res = TEEC_InvokeCommand(&sess, cmd, /* xplane_invoke_command */ &op, &err_origin);
	if(res != TEEC_SUCCESS)
		errx(1, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x", res, err_origin);

//	printf("tmpref.buffer : %lx\n", *(x_addr *)op.params[1].tmpref.buffer);
//	printf("dest : %lx\n", dests[0]);

//	printf(_k2clr_red "exec time : %d ms\n" _k2clr_none, op.params[0].value.b);
	I("exec time : %d ms", op.params[0].value.b); // xzl

	if(cmd == X_CMD_JOIN)
		execution_time += op.params[0].value.b;
	close_session(&sess);

	return op.params[0].value.a; /* error code */
}

x_addr create_uarray_file(FILE* fp, uint32_t size){
	simd_t *nsbuf;
	uint32_t cnt = 0;
	x_addr ret;

	nsbuf = (simd_t *) malloc (sizeof(simd_t) * size * ELES_PER_ITEM);
	printf("read file ... \n");

	while(cnt < size){
		if (fscanf(fp, "%d\n", nsbuf + cnt++) == EOF)
			break;
	}
	fclose(fp);

	ret = xfunc_dest(X_CMD_UA_NSBUF, nsbuf, size);

	free(nsbuf);

	return ret;
}

x_addr create_uarray(){
	return xfunc_dest(X_CMD_UA_CREATE, NULL, 0);
}

x_addr create_uarray_rand(){
	return xfunc_dest(X_CMD_UA_RAND, NULL, 0);
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

	I("[core %d] starts", sched_getcpu());
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

	ret = xfunc_segment(dests, seg_bases, (void *)&segarg, sizeof(seg_arg));

	*n_segs = segarg.n_segs;

	I("[core %d] returns", sched_getcpu());
	return ret;
}


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
