#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

typedef int32_t OBJ_TYPE;
typedef uint64_t x_addr;
#define x_addr_null 0

/************************ typedefs **********************
 * xzl: moved from xplane_lib.h
 */
//typedef uint32_t simd_t ; /* the element type that simd operates on */
//typedef uint64_t xscalar_t; /* encrypted 32-bit value */
//typedef uint8_t idx_t; /* column index, # of cols, etc. */

/*
enum record_format {
	record_format_x3,
	record_format_x4,
};
*/

enum func_sort {
	x_sort_t,
	qsort_t,
	stable_sort_t
};

enum func_sumcount {
	sumcount1_perkey_t,
	sumcount_perkey_t,
	sum_perkey_t,
	avg_perkey_t,
	sumcount1_all_t,
	sumcount_all_t,
	avg_all_t
};

enum func_join {
	join_bykey_t,
	join_byfilter_t,
	join_byfilter_s_t
};

enum func_median {
	med_bykey_t,
	med_all_t,
	med_all_s_t,
	topk_bykey_t,
	kpercentile_bykey_t,
	kpercentile_all_t
};

enum func_ua_debug {
    uarray_size_t,
};

enum func_misc {
	unique_key_t,
};


/************************ typedefs **********************/
typedef uint32_t simd_t ; /* the element type that simd operates on */
typedef uint64_t xscalar_t; /* encrypted 32-bit value */
typedef uint8_t idx_t; /* column index, # of cols, etc. */

typedef uint8_t func_t;

typedef struct {
	uint32_t cmd;

	x_addr src;

	uint32_t src_offset;
	uint32_t count;

	idx_t reclen;
	idx_t tspos;

	int32_t ts_start;
	int32_t ts_delta;
} source_params;


typedef struct {
	x_addr srcA;
	x_addr srcB;

	uint32_t n_outputs;

	func_t func;

	idx_t keypos;
	idx_t vpos;
	idx_t countpos;

	idx_t reclen;

	simd_t lower;
	simd_t higher;

	float k;
	bool reverse;
} x_params;


typedef struct {
	xscalar_t seg_base;
	x_addr seg_ref;
} seg_t;

typedef struct {
	/* output */
	uint32_t n_segs;

	/* input */
	x_addr src;

	uint32_t n_outputs;
	uint32_t base;
	uint32_t subrange;

	idx_t tspos;
	idx_t reclen;
} seg_arg;

#endif
