#ifndef XPLANE_LIB_H
#define XPLANE_LIB_H

#ifdef __cplusplus
extern "C" {
#endif

/* OP_TEE TEE client API (built by optee_client) */
#include <tee_client_api.h>
#include <stdbool.h>
#include "armtz-types.h"
//#include "types.h"  /* xzl: don't include this here. types.h is for armtz only */

#define ELES_PER_ITEM	3

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"


/********** ERROR CODE **************/
#define X_SUCCESS			0
#define X_ERR_NOSBUF        1
#define X_ERR_SMALL_INPUT   2
#define X_ERR_NO_XADDR      3
#define X_ERR_BATCH_CLOSED  4
#define X_ERR_WRONG_PARAMETER  5 /* xzl */

/**********	FILE_STATUS	**************/
enum NSBUF_STATUS {
	BUF_NORMAL,
	BUF_START,
	BUF_CONTINUE,
	BUF_END
};
/*************************************/

/************	X_CMD	**************/
enum X_CMD {
	X_CMD_MAP,
	X_CMD_KILL,

	X_CMD_UA_RAND,
	X_CMD_UA_NSBUF,
	X_CMD_UA_CREATE,
	X_CMD_UA_RETIRE,
	X_CMD_UA_PSEUDO,
	X_CMD_UA_TO_NS,
	X_CMD_UA_DEBUG,
	X_CMD_SHOW_SBUFF,

	X_CMD_SEGMENT,
	X_CMD_SORT,
	X_CMD_MERGE,
	X_CMD_JOIN,
	X_CMD_FILTER,
	X_CMD_MEDIAN,
	X_CMD_SUMCOUNT,
	X_CMD_CONCATENATE,
	X_CMD_MISC,

	X_CMD_MAX /* xzl */
};
/*************************************/

void open_context(void);
void open_session(TEEC_Session *sess);
void close_session(TEEC_Session *sess);
void close_context();
void populate_map(void);
void kill_map(void);
void show_sbuff_resource(void);

//void *op_thread(void *arg);
void invoke_multiple_xfunc(uint32_t cmd);


#if 0 /* xzl: moved to armtz-types.h */
/************************ typedefs **********************/
typedef uint32_t simd_t ; /* the element type that simd operates on */
typedef uint64_t xscalar_t; /* encrypted 32-bit value */
typedef uint8_t idx_t; /* column index, # of cols, etc. */
#endif

/************************ invoke xfunc inside of SW  **********************/
int32_t invoke_xfunc(void *outbuf, size_t outbuf_size, uint32_t cmd, void *params, size_t params_size);

int32_t xfunc_cmd_only(uint32_t cmd);
x_addr	xfunc_dest(uint32_t cmd, simd_t *nsbuf, uint32_t size);
int32_t xfunc_dest_src_src(uint32_t cmd, x_addr *dest, x_addr srcA, x_addr srcB);

int32_t xfunc_segment(x_addr **dests, xscalar_t **seg_bases, void *args, uint32_t args_size);

/**************************************************************************/

void xplane_init(void);

/**************************************************************************/

/* all return:
 * on success, >=0. may return exec time in tz (in ms);
 * on error, -1
 *
 * @**dests allocated by the callee, must be free'd by caller
 * @dest: newly created sbatch.
 *
 * caller must retire() all sbatches.
 *
 * XXX_s the encrypted scalar version
 */

/************************ uarray functions ********************************/
#define CU_NORMAL 0
#define CU_SINGLE 1 	/* one simd_t item only. won't grow */
#define CU_SMALL	2	 /* hint: the uarray might be small */

x_addr create_uarray(int flags /* see CU_XXX above */);

x_addr create_uarray_rand(); /* optional */
x_addr create_uarray_file(FILE *fp, uint32_t size);	/* optional */

/* create an uarray from a raw/ns buf.
 * @len: # of simd_t
 * return: the uarray */
x_addr create_uarray_nsbuf(simd_t *nsbuf, uint32_t len);

/* for debugging
 * copy contents of sbatch to ns
 * caller allocates @nsbuf and ensures enough space
 * @len: # of simd_t elements. IN: expected, OUT: actual
 * */
int32_t uarray_to_nsbuf(simd_t *dest, x_addr src, uint32_t *len);

int32_t retire_uarray(x_addr r);

/* get uarray size in secure world */
int32_t uarray_size(x_addr src);


/************************ transforms **************************************/

/* @dests: allocated by caller
 *
 * caller ensures @src_offset < @src bound
 * NB: @dest is a closed batch. (this is for saving sbuffs)
 */
int32_t pseudo_source(x_addr *dest,
		x_addr src, uint32_t src_offset, uint32_t len, /* both in #records */
		int32_t ts_start, int32_t ts_delta,
		idx_t tspos, idx_t reclen
		);

int32_t sort(x_addr *dests, uint32_t n_outputs, x_addr src,
							idx_t keypos, idx_t reclen);
/* @vpos: alrady sorted pos */
int32_t sort_stable(x_addr *dests, uint32_t n_outputs, x_addr src,
							idx_t keypos, idx_t vpos, idx_t reclen);
int32_t merge(x_addr *dests, uint32_t n_outputs, x_addr srcA, x_addr srcB,
							idx_t keypos, idx_t reclen);

/* -- join by key -- */
int32_t join_bykey(x_addr *dest, x_addr srcA, x_addr srcB, idx_t keypos, idx_t reclen);
/* -- join by filter -- */
int32_t join_byfilter(x_addr *dest, x_addr src, x_addr threshold, idx_t vpos, idx_t reclen);
int32_t join_byfilter_s(xscalar_t *dest, x_addr src, xscalar_t threshold, idx_t vpos, idx_t reclen);

 /* sum */
int32_t sumall(x_addr *dest, x_addr src,
						idx_t vpos, idx_t reclen);
int32_t sumall_s(xscalar_t *dest, x_addr src,
						idx_t vpos, idx_t reclen); /* optional */
int32_t sum_perkey(x_addr *dests, uint32_t n_outputs, x_addr src,
						idx_t keypos, idx_t vpos, idx_t reclen);

/* -- med per key -- */
int32_t med_bykey(x_addr *dests, uint32_t n_outputs, x_addr src,
						idx_t keypos, idx_t vpos, idx_t reclen);

/* return top k% records for each key */
int32_t topk_bykey(x_addr *dests, uint32_t n_outputs, x_addr src,
						idx_t keypos, idx_t vpos, idx_t reclen,
						float k, bool reverse);

/* return the k%-th record for each key */
int32_t kpercentile_bykey(x_addr *dests, uint32_t n_outputs, x_addr src,
						idx_t keypos, idx_t vpos, idx_t reclen,
						float k);

/* return the k%-th record */
/* src shouled be sorted by val */
int32_t kpercentile_all(x_addr *dest, x_addr src,
						idx_t keypos, idx_t vpos, idx_t reclen,
						float k);

/* -- med all (obsoleted by percentile above) -- */
//int32_t med_all(x_addr *dest, x_addr src,
//						idx_t keypos, idx_t vpos, idx_t reclen);
//int32_t med_all_s(xscalar_t *dest, x_addr src,
//						idx_t keypos, idx_t vpos, idx_t reclen);

/* --- sumcount/avg per key --- */
int32_t sumcount1_perkey(x_addr *dests, uint32_t n_outputs, x_addr src,
					idx_t keypos, idx_t vpos, idx_t countpos, idx_t reclen);
int32_t sumcount_perkey(x_addr *dests, uint32_t n_outputs, x_addr src,
					idx_t keypos, idx_t vpos, idx_t countpos, idx_t reclen);
int32_t avg_perkey(x_addr *dests, uint32_t n_outputs, x_addr src,
					idx_t keypos, idx_t vpos, idx_t countpos, idx_t reclen);
/* result batch: one record for each key (value of record undefined) */
int32_t unique_key(x_addr *dest, x_addr src, idx_t keypos, idx_t reclen);

/* --- sumcount/avg all. result: one record  --- */
int32_t sumcount1_all(x_addr *dest, x_addr src,
					idx_t keypos, idx_t vpos, idx_t countpos, idx_t reclen);
int32_t sumcount_all(x_addr *dest, x_addr src,
					idx_t keypos, idx_t vpos, idx_t countpos, idx_t reclen);
int32_t avg_all(x_addr *dest, x_addr src,
					idx_t keypos, idx_t vpos, idx_t countpos, idx_t reclen);

/* scalar version */
int32_t sumcount1_all_s(xscalar_t *dest, x_addr src, idx_t keypos, idx_t vpos, idx_t reclen);
int32_t sumcount_all_s(xscalar_t *dest, x_addr src, idx_t keypos, idx_t vpos, idx_t reclen);
int32_t avg_all_s(xscalar_t *dest, x_addr src, idx_t keypos, idx_t vpos, idx_t reclen);

/* misc */
//int32_t segment(x_addr **dest, uint32_t * n_segs, uint32_t n_outputs,
//				x_addr src, uint32_t base, uint32_t subrange);

/* caller must free @dests and @seg_bases after use
 *
 * @n_outputs: #sbatches per segment (>1 TBD)*/
int32_t segment(x_addr **dests, xscalar_t **seg_bases, uint32_t * n_segs,
				uint32_t n_outputs,
				x_addr src, uint32_t base, uint32_t subrange,
				idx_t tspos, idx_t reclen);

/* Copy content of @src to @dest.
 *
 * Caller must guarntee that @dest is NOT "closed" yet; otherwise, xplane
 * may return <0
 */
int32_t concatenate(x_addr dest, x_addr src /* uint32_t src_offset */);

int32_t filter_band(x_addr *dests, uint32_t n_outputs, x_addr src,
					simd_t lower, simd_t higher, idx_t vpos, idx_t reclen);


#ifdef __cplusplus
}		// extern C
#endif

#endif
