#ifndef XPLANE_LIB_H
#define XPLANE_LIB_H

#ifdef __cplusplus
extern "C" {
#endif

/* OP_TEE TEE client API (built by optee_client) */
//#include <tee_client_api.h>
#include <stdbool.h>
#include "xplane/type.h"

//#include "armtz-types.h" // xzl: why do we include this here?
//#include "types.h"  /* xzl: don't include this here. types.h is for armtz only */

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"

extern const char * xplane_lib_arch; /* x86, avx2, etc. defined in utils.c */

/********** ERROR CODE **************/
#define X_SUCCESS			0
#define X_ERR_NOSBUF        1
#define X_ERR_SMALL_INPUT   2
#define X_ERR_NO_XADDR      3
#define X_ERR_BATCH_CLOSED  4
#define X_ERR_WRONG_PARAMETER  5 /* xzl */

#if 0 /* moved */
/**********	FILE_STATUS	**************/
enum NSBUF_STATUS {
	BUF_NORMAL,
	BUF_START,
	BUF_CONTINUE,
	BUF_END
};
#endif

/*************************************/

#if 0 /* in xfunc.h */
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
#endif

/*************************************/

//void *op_thread(void *arg);
void invoke_multiple_xfunc(uint32_t cmd);


/************************ invoke xfunc inside of SW  **********************/
int32_t invoke_xfunc(void *outbuf, size_t outbuf_size, uint32_t cmd, void *params, size_t params_size);

int32_t xfunc_cmd_only(uint32_t cmd);
x_addr	xfunc_dest(uint32_t cmd, uint32_t *nsbuf, uint32_t size);
int32_t xfunc_dest_src_src(uint32_t cmd, x_addr *dest, x_addr srcA, x_addr srcB);

int32_t invoke_xfunc_segment(x_addr **dests, xscalar_t **seg_bases, void *params, uint32_t params_size);

/**************************************************************************/

/* @p: ref to init parameters. arch specific */
//#define NUM_POOLS 6

enum POOL_FLAG {
	POOL_MALLOC = 0,
	POOL_HUGETLB = 1,
	POOL_CAN_FAIL = 2
};

#define is_hugetlb(x) (x & POOL_HUGETLB)
#define is_malloc(x) (!is_hugetlb(x))

struct pool_desc {
	char const * name;
	unsigned long grain;
	unsigned long size; /* == 0 means invalid */
	int flag;
};

/* @pools: configs for mempool.
 * see mempool.cpp for an example config
 * if == NULL, use default config */
void xplane_init(struct pool_desc * pools);

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
x_addr create_uarray_file(FILE *fp, uint32_t size, idx_t reclen);	/* optional */

/* create an uarray from a raw/ns buf.
 * @len: # of simd_t
 * return: the uarray */
x_addr create_uarray_nsbuf(uint32_t *nsbuf, uint32_t len);

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

/* sort extracted key batch and ptr batch
 * @dests_k, @dests_p: output batch of keys and that of ptrs
 * @src_k, @src_p: ditto
 */
int32_t sort_kp(x_addr *dests_k, x_addr *dests_p,
								uint32_t n_outputs,
								x_addr src_k, x_addr src_p);

/* @vpos: alrady sorted pos */
int32_t sort_stable(x_addr *dests, uint32_t n_outputs, x_addr src,
							idx_t keypos, idx_t vpos, idx_t reclen);

/* see sort_kp() above */
int32_t sort_stable_kp(x_addr *dests_k, x_addr *dests_p,
											 uint32_t n_outputs,
											 x_addr src_k, x_addr src_p);

int32_t merge(x_addr *dests, uint32_t n_outputs, x_addr srcA, x_addr srcB,
							idx_t keypos, idx_t reclen);

/* merge two extracted key batches and the corresponding ptr batches
 *
 * @dests_k, @dests_p: output batch of keys and that of ptrs
 * @srcA_k, @srcA_p: ditto
 */
int32_t merge_kp(x_addr *dests_k, x_addr *dests_p,
		uint32_t n_outputs,
		x_addr srcA_k, x_addr srcA_p,
		x_addr srcB_k, x_addr srcB_p);

/* -- join by key -- */
int32_t join_bykey(x_addr *dest,
	x_addr srcA, x_addr srcB,
	idx_t keypos, idx_t reclen);

int32_t join_bykey_kp(x_addr *dests_k, x_addr *dests_p,
											x_addr srcA_k, x_addr srcA_p,
											x_addr srcB_k, x_addr srcB_p);

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

/*
 * Taking a batch of records, produce a batch of keys and a batch of pointers
 * @src: the input batch of records
 * @dest_k: the output batch of keys
 * @dest_p: the output batch of pointers
 *
 * unsupported if size(simd_t) < pointer size
 */
int32_t extract_kp(x_addr *dest_k, x_addr *dest_p, x_addr src, idx_t keypos, idx_t reclen);


/* Taking a batch of pointers, assemble a batch of records based on the pointers.
 * @src_p: the input batch of pointers
 * @dest: output batch of records
 *
 * the caller must ensure that src batches (pointed by @src_p) are valid
 *
 * unsupported if size(simd_t) < pointer size
 */
int32_t expand_p(x_addr *dest, x_addr src_p, idx_t reclen);

/* Taking a batch of record pointers, produce a batch of keys that extracted from a specific
 * field (column) of each record.
 *
 * @src_p: the input batch of pointers
 * @dest: the output batch of keys
 *
 * @reclen, @keypos: ditto
 * return value: ditto
 *
 * unsupported if size(simd_t) < pointer size
 */
int32_t deref_k(x_addr *dest, x_addr src_p, idx_t keypos, idx_t reclen);

/* dbg */
void dump_xaddr_info(x_addr x);

#if 0 /* old impl */
/*
 * taking a batch of <records>, produce a batch of <key,index>
 * return:
 * 	err if max index larger than the range of simd_t
 */
int32_t create_index(x_addr *dest, x_addr src, idx_t keypos, idx_t reclen);

/* take a @src batch of <record> and a @idx batch of <key, index>,
 * shuffle the @src batch based on the index, and produce to @dest
 */
int32_t shuffle_index(x_addr *dest, x_addr src, x_addr idx, idx_t reclen);
#endif

/* new functions for batch_k and batch_ptr, instead of batch_record*/
int binary_search_k(simd_t *data, int l, int r, simd_t key); /* don't expose */

void split_batch_k(x_addr src_ak, uint32_t* offset_a, uint32_t* len_a,
									 x_addr src_bk, uint32_t* offset_b, uint32_t* len_b,
									 uint32_t n_parts);

void calculate_offset_kp(uint32_t* len_a, uint32_t* len_b, uint32_t* offset, uint32_t n_parts);

//hym: flags is used to specify fast/slow memory or NUMA node
x_addr create_batch_merge(int flags, x_addr src_a, x_addr src_b);
void free_batch(x_addr src);

/* for statistics */
void mempool_stat_print(void);

/* hp: arm doesn't support kp separated operations */
#if defined(KNL) || defined(ARCH_AVX2)
/* for parallel expand */
int32_t expand_p_one_part(x_addr *dest, x_addr src_p, idx_t reclen, uint32_t offset, uint32_t len);
x_addr create_batch_expand(int flags, x_addr src_p, idx_t reclen);
uint32_t get_batch_size(x_addr src);


/*
 * Spec: find (n_parts - 1) k boundaries.
 *
 * @offset[]: OUT stores the start offset of each partition. // simd_t (not Record)
 * @src_record: IN sorted record batch by k&v
 * @n_parts: IN N partitions // will use N threads to aagreate
 * @kpos: IN key offset within a Record
 * @reclen: IN # of simd_t in a Record

	return: error code or 0 on success
 */
int32_t find_k_boundary(uint32_t * offsets, x_addr src_record, uint32_t n_parts, idx_t kpos, idx_t reclen);

/* the variant of the above function that takes k batch as input */
//int32_t find_k_boundary_k(uint32_t * offsets, x_addr src_k, uint32_t n_parts);
int32_t find_k_boundary_k(uint32_t * offsets, uint32_t * lens, x_addr src_k, uint32_t n_parts);


/*
 * Spec: aggregate on one partition, return the address of aggregated record batch // one thread's work
 *
 * Input is record batch
 *
 * Note: allocate @dest inside this function
 *
 * @dest: OUT
 * @src_record: sorted record batch by k&v
 * @offset_i: the start offset within the input partition @src_record. // simd_t (not Record)
 * @len_i: the len of input partition. //# of simd_t (not # of Record)
 * @kpos: k offset within a Record (in simd_t)
 * @vpos: v offset within a Record (in simd_t) // For future use case, like dereference
 * @reclen: # of simd_t in a Record
 * @topP: percentile
 *
 * return: error code or 0 on success
 */
int32_t
kpercentile_bykey_one_part(x_addr *dest, x_addr src_record, uint32_t offset, uint32_t len_i, idx_t kpos, idx_t vpos,
													 idx_t reclen, float topP);

/* the invariant of taking kbatch and pbatch as input. output is still *record batch* (not k p batches)
 *
 * description: pick one corresponding record (k percentile) per key
 *
 * @dest_new_records OUT: produced new records. Each new record has TWO columns. First: key; second: v
 *
 * @offset: offset of the actual input within src_k and src_p. in simd_t.
 * @len: length of the actual input within src_k and src_p. in simd_t.
 *
 * @vpos: v offset within a Record (in simd_t) // For future use case, like dereference
 * @reclen: # of simd_t in a Record
 *
 * return: error code or 0 on success
 */
int32_t
kpercentile_bykey_one_part_k(x_addr *dest_new_records, x_addr src_k, x_addr src_p, uint32_t offset, uint32_t len_i,
														 idx_t vpos,
														 idx_t reclen, float topP);

/* the invariant of taking kbatch and pbatch as input. output is still *record batch* (not k p batches)
 *
 * description: pick top n number of records per key
 *
 * @dest_new_records OUT: produced new records. Each new record has TWO columns. First: key; second: v
 *
 * @offset: offset of the actual input within src_k and src_p. in simd_t.
 * @len: length of the actual input within src_k and src_p. in simd_t.
 *
 * @vpos: v offset within a Record (in simd_t) // For future use case, like dereference
 * @reclen: # of simd_t in a Record
 *
 * return: error code or 0 on success
 */
int32_t
topk_bykey_one_part_k(x_addr *dest_new_records, x_addr src_k, x_addr src_p, uint32_t offset, uint32_t len_i,
														 idx_t vpos,
														 idx_t reclen, float topP);


/* Get source batch of records and make them multiple segments according to timestamp
 * Caller must free @dests_p and @seg_bases after using it
 *
 * @dests_p		[OUT] : multiple batch addresses produced consisting of record pointer
 * @seg_bases	[OUT] : each segment's base timestamp
 * @n_segs		[OUT] : # of segs produced by this functions
 *
 * @src			[IN] : address of src (full records)
 * @base		[IN] : base timestamp for segment
 * @subrange	[IN] : ts subrange to partition the @src
 * @tspos		[IN] : timestamp position
 * @reclen		[IN] : record length  */
int32_t
segment_kp(x_addr **dests_p, simd_t **seg_bases, uint32_t *n_segs,
				x_addr src, simd_t base, simd_t subrange, idx_t tspos, idx_t reclen);


/* filter. the k/p version.
 * @dests_p [OUT]   the survival p (support multiple outputs)
 *
 * @n_outputs # of outputs
 * @src_k, src_p  input k(for filtering) and p
 * @lower/higher: thresholds
 */
int32_t filter_band_kp(x_addr *dests_p, uint32_t n_outputs,
		x_addr src_k, x_addr src_p, simd_t lower, simd_t higher);
#endif	// defined(KNL) || defined(ARCH_AVX2)

#ifdef KNL
/* functions for batch_records: only useful for ubench/parallel_merge.c */
int binary_search(simd_t *data, int l, int r, simd_t key, idx_t keypos, idx_t reclen); /* don't expose */
void split_batch(x_addr src_a, uint32_t* offset_a, uint32_t* len_a, x_addr src_b, uint32_t* offset_b, uint32_t * len_b, uint32_t n_parts, idx_t keypos, idx_t reclen);
void merge_one_part(x_addr src_a, uint32_t offset_a, uint32_t len_a, x_addr src_b, uint32_t offset_b, uint32_t len_b, x_addr merge_dest,  uint32_t merge_offset, idx_t keypos, idx_t reclen);
void calculate_offset(uint32_t* len_a, uint32_t* len_b, uint32_t* offset, uint32_t n_parts, idx_t reclen);
#endif

#if defined(KNL) || defined(ARCH_AVX2)
void merge_one_part_kp(x_addr src_ak, x_addr src_ap, uint32_t offset_a, uint32_t len_a,
		       x_addr src_bk, x_addr src_bp, uint32_t offset_b, uint32_t len_b,
		       x_addr dest_mergek, x_addr dest_mergep, uint32_t offset_merge);

#else
// hp : arm doesn't support kp
//#error "tbd"
#endif

#ifdef __cplusplus
}		// extern C
#endif

#endif

