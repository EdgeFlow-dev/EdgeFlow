/* xzl: this file used to be armtz-types.h */

#ifndef ARMTZ_TYPES_H
#define ARMTZ_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "xplane/type.h"

//typedef int32_t OBJ_TYPE;
//typedef uint64_t x_addr;

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

/* ------------------------------- */
/* xzl: moved from xplane/type.h */
typedef struct { int32_t val[1]; } x1_t;
typedef struct { int32_t val[2]; } x2_t;
typedef struct { int32_t val[3]; } x3_t;
typedef struct { int32_t val[4]; } x4_t;

typedef struct { x1_t val[4]; } x1x4_t;
typedef struct { x2_t val[4]; } x2x4_t;
typedef struct { x3_t val[4]; } x3x4_t;
typedef struct { x4_t val[4]; } x4x4_t;


struct batch {
	simd_t *start;		/* start pointer of data */
	simd_t *end;		/* end pointer of data */
	simd_t size;		/* size of data in the buffer (# of simd_t )*/

	struct batch *next;
	uint32_t state;

	uint32_t buf_size;	/* real buffer size in the batch (Byte) */
};

typedef struct batch batch_t;

#ifdef ONE_PER_ITEM
typedef struct _item_t { int32_t ts } item_t;
#endif
#ifdef TWO_PER_ITEM
typedef struct _item_t { int32_t ts, value; } item_t;
#endif
#ifdef THREE_PER_ITEM
typedef struct _item_t { int32_t ts, key, value; } item_t;
#endif
#ifdef FOUR_PER_ITEM
typedef struct _item_t { int32_t ts, key1, key2, value; } item_t;
#endif
/* ------------------------------- */

#if 0 /* xzl: already moved to individual .h ? */
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
#endif


typedef uint8_t func_t;

#if 0 /* xzl: moved */
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
#endif

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

#if 0
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

#endif
