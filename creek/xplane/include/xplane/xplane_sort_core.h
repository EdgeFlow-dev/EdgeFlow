#ifndef XPLANE_SORT_CORE_H
#define XPLANE_SORT_CORE_H

#include <string.h>
#include "type.h"
#include "xplane/type.h"
#include "xplane-internal.h"
#include "neon_common.h"

//__attribute__((__noreturn__))
int32_t *L1_chunk_sort_tmp(int32_t *inarr, int32_t *outarr, idx_t keypos, idx_t reclen);
int32_t *L1_chunk_sort(int32_t *inarr, int32_t *outarr, idx_t keypos, idx_t reclen);
batch_t *merge_two_batches(batch_t *inA, batch_t *inB, uint32_t t_idx, idx_t reclen);
batch_t *merge_chunks(int32_t *start, uint32_t nitems, idx_t keypos, idx_t reclen);


/***************************	1 elements per record	**********************************************/
void x1_inregister_sort(int32_t *items, int32_t *output, uint32_t t_idx);
void x1_merge4_eqlen_aligned(int32_t * const inpA, int32_t * const inpB,
							int32_t * const out, const uint32_t lenA, const uint32_t lenB, uint32_t t_idx);
void x1_merge8_varlen_aligned(int32_t * restrict inpA, int32_t * restrict inpB, int32_t * restrict Out,
							const uint32_t lenA, const uint32_t lenB, uint32_t t_idx);
/*****************************************************************************************************/

/***************************	3 elements per record	**********************************************/
void x3_inregister_sort(int32_t *items, int32_t *output, uint32_t t_idx);
void x3_merge4_aligned(int32_t * const inpA, int32_t * const inpB, int32_t * const out, 
							const uint32_t lenA, const uint32_t lenB, uint32_t t_idx);
void x3_merge4_not_aligned(int32_t * restrict inpA, int32_t * restrict inpB, int32_t * restrict Out,
							const uint32_t lenA, const uint32_t lenB, uint32_t t_idx);
/*****************************************************************************************************/

/***************************	4 elements per record	**********************************************/
void x4_inregister_sort(int32_t *items, int32_t *output, uint32_t t_idx);
void x4_merge4_aligned(int32_t * const inpA, int32_t * const inpB, int32_t * const out,
							const uint32_t lenA, const uint32_t lenB, uint32_t t_idx);
void x4_merge4_not_aligned(int32_t * restrict inpA, int32_t * restrict inpB, int32_t * restrict Out,
							const uint32_t lenA, const uint32_t lenB, uint32_t t_idx);
/*****************************************************************************************************/


inline __attribute((always_inline))
	void x3_swap(x3_t * a, x3_t *b){
		x3_t tmp;

		tmp = *a;
		*a = *b;
		*b = tmp;
	}

static inline __attribute((always_inline))
	void xplane_transpose4x4(int32x4_t* ra, int32x4_t* rb, int32x4_t* rc, int32x4_t* rd){
		int32x4_t ra2, rb2, rc2, rd2;

		/* ra={ a1, b1, c1, d1}				ra={ a1, a2, a3, a4}
		 * 	*  rb={ a2, b2, c2, d2}		==>		ra={ b1, b2, b3, b4}
		 * 		*  rc={ a3, b3, c3, d3}				ra={ c1, c2, c3, c4}
		 * 			*  rd={ a4, b4, c4, d4}				ra={ d1, d2, d3, d4} */ 

		/* ra2={ a1 a3 b1 b3 }
		 * 	 * rb2={ c1 c3 d1 d3  } */
		ra2 = vzip1q_s32(*ra, *rc);
		rb2 = vzip2q_s32(*ra, *rc);

		/* rc2={ a2 a4 b2 b4 }
		 * 	 * rd2={ c2 c4 d2 d4 } */
		rc2 = vzip1q_s32(*rb, *rd);
		rd2 = vzip2q_s32(*rb, *rd);

		/* ra={ a1 a2 a3 a4 }
		 * 	 * rb={ b1 b2 b3 b4 } */
		*ra = vzip1q_s32(ra2, rc2);
		*rb = vzip2q_s32(ra2, rc2);

		/* rc={ c1 c2 c3 c4 }
		 * 	 * rd={ d1 d2 d3 d4 } */
		*rc = vzip1q_s32(rb2, rd2);
		*rd = vzip2q_s32(rb2, rd2);
	}

void x3_qsort(x3_t *arr, int l, int r, uint32_t cmp);

#if 0
/* To deal with not aligned items */
static inline __attribute((always_inline))
	void x3_qsort(item_t* arr, int l, int r, uint32_t cmp){
		int i, j;
		int32_t *pivot = (int32_t *)&arr[l];

		if(l < r) {
			i = l;
			j = r + 1;
			while(1) {
				do ++i; while (*((int32_t *)&arr[i] + cmp) <= pivot[cmp] && i <= r);
				do --j; while (*((int32_t *)&arr[j] + cmp) > pivot[cmp]);
				if(i >= j) break;
				x3_swap(&arr[i], &arr[j]);
			}
			x3_swap(&arr[l], &arr[j]);
			x3_qsort(arr, l, j-1, cmp);
			x3_qsort(arr, j+1, r, cmp);
		}
	}
#endif
/* Return max and min matrices with x3 scatter and gather */
static inline void x3_cmp_s32x4(int32x4x3_t *ra, int32x4x3_t *rb, uint32_t cmp){
	uint32x4_t mask;
	int32x4x3_t max, min;
	int32_t i;

	mask = vcgtq_s32(ra->val[cmp], rb->val[cmp]);

	for(i = 0 ; i < 3; i++) {
		min.val[i] = vbslq_s32(mask, rb->val[i], ra->val[i]);
		max.val[i] = vbslq_s32(mask, ra->val[i], rb->val[i]); 
	}

	*ra = min;
	*rb = max;
}

/* bitonic4 merge with x3 scatter and gather */
static inline void x3_bitonic_merge4(int32x4x3_t *ra, int32x4x3_t *rb, uint32_t cmp){
	int32_t i;
	int32x4x3_t ra1, rb1;
	int32x4x3_t ra2, rb2;

	for(i = 0; i < 3; i ++)
		REVERSE(rb->val[i]);

	ra1 = *ra; rb1 = *rb;

	x3_cmp_s32x4(&ra1, &rb1, cmp);

	for(i = 0; i < 3; i++) {
		ra2.val[i] = vzip1q_s32(ra1.val[i], rb1.val[i]);
		rb2.val[i] = vzip2q_s32(ra1.val[i], rb1.val[i]);
	}

	x3_cmp_s32x4(&ra2, &rb2, cmp);

	for(i = 0; i < 3; i++) {
		ra1.val[i] = vzip1q_s32(ra2.val[i], rb2.val[i]);
		rb1.val[i] = vzip2q_s32(ra2.val[i], rb2.val[i]);
	}

	x3_cmp_s32x4(&ra1, &rb1, cmp);

	for(i = 0; i < 3; i++){
		ra->val[i] = vzip1q_s32(ra1.val[i], rb1.val[i]);
		rb->val[i] = vzip2q_s32(ra1.val[i], rb1.val[i]);
	}
}

/* Return max and min matrices with x4 scatter and gather */
static inline void x4_cmp_s32x4(int32x4x4_t *ra, int32x4x4_t *rb, uint32_t cmp){
	uint32x4_t mask;
	int32x4x4_t max, min;
	int32_t i;

	mask = vcgtq_s32(ra->val[cmp], rb->val[cmp]);

	for(i = 0 ; i < 4; i++) {
		min.val[i] = vbslq_s32(mask, rb->val[i], ra->val[i]);
		max.val[i] = vbslq_s32(mask, ra->val[i], rb->val[i]); 
	}

	*ra = min;
	*rb = max;
}

/* bitonic4 merge with x4 scatter and gather */
static inline void x4_bitonic_merge4(int32x4x4_t *ra, int32x4x4_t *rb, uint32_t cmp){
	int32_t i;
	int32x4x4_t ra1, rb1;
	int32x4x4_t ra2, rb2;

	for(i = 0; i < 4; i++)
		REVERSE(rb->val[i]);

	ra1 = *ra; rb1 = *rb;

	x4_cmp_s32x4(&ra1, &rb1, cmp);

	for(i = 0; i < 4; i++) {
		ra2.val[i] = vzip1q_s32(ra1.val[i], rb1.val[i]);
		rb2.val[i] = vzip2q_s32(ra1.val[i], rb1.val[i]);
	}

	x4_cmp_s32x4(&ra2, &rb2, cmp);

	for(i = 0; i < 4; i++) {
		ra1.val[i] = vzip1q_s32(ra2.val[i], rb2.val[i]);
		rb1.val[i] = vzip2q_s32(ra2.val[i], rb2.val[i]);
	}

	x4_cmp_s32x4(&ra1, &rb1, cmp);

	for(i = 0; i < 4; i++){
		ra->val[i] = vzip1q_s32(ra1.val[i], rb1.val[i]);
		rb->val[i] = vzip2q_s32(ra1.val[i], rb1.val[i]);
	}
}

/************************************************************/
/*				Comparison with double keys					*/
/* Return max and min matrices with x3 scatter and gather 	*/
static inline void x3_double_cmp_s32x4(int32x4x3_t *ra, int32x4x3_t *rb,
		idx_t keyA, idx_t keyB){
	uint32x4_t maskA;
	uint32x4_t maskB1, maskB2;
	uint32x4_t mask;

	int32x4x3_t max, min;
	int32_t i;

	/* If ra's keyA > rb's keyA : return 1 */
	maskA = vcgtq_s32(ra->val[keyA], rb->val[keyA]);

	/* If ra's KeyA == rb's keyB */
	maskB1 = vceqq_s32(ra->val[keyA], rb->val[keyA]);
	/* Check second key (KeyB) */
	maskB2 = vcgtq_s32(ra->val[keyB], rb->val[keyB]);

	/* maskB1 : ra's keyA > rb's keyA &&
	 * 	 * 			ra's keyB > rb's keyB */
	maskB1 = vandq_u32(maskB1, maskB2);

	/* final mask : maskA (case of keyA > keyB) ||
	 * 	 * 					maskB1 (case of same keyA but larger KeyB) */
	mask = vorrq_u32(maskA, maskB1);

	for(i = 0 ; i < 3; i++) {
		min.val[i] = vbslq_s32(mask, rb->val[i], ra->val[i]);
		max.val[i] = vbslq_s32(mask, ra->val[i], rb->val[i]); 
	}

	*ra = min;
	*rb = max;
}

/* bitonic4 merge with x3 scatter and gather */
static inline void x3_double_bitonic_merge4(int32x4x3_t *ra, int32x4x3_t *rb, idx_t keyA, idx_t keyB){
	int32_t i;
	int32x4x3_t ra1, rb1;
	int32x4x3_t ra2, rb2;

	for(i = 0; i < 3; i ++)
		REVERSE(rb->val[i]);

	ra1 = *ra; rb1 = *rb;

	x3_double_cmp_s32x4(&ra1, &rb1, keyA, keyB);

	for(i = 0; i < 3; i++) {
		ra2.val[i] = vzip1q_s32(ra1.val[i], rb1.val[i]);
		rb2.val[i] = vzip2q_s32(ra1.val[i], rb1.val[i]);
	}

	x3_double_cmp_s32x4(&ra1, &rb1, keyA, keyB);

	for(i = 0; i < 3; i++) {
		ra1.val[i] = vzip1q_s32(ra2.val[i], rb2.val[i]);
		rb1.val[i] = vzip2q_s32(ra2.val[i], rb2.val[i]);
	}

	x3_double_cmp_s32x4(&ra1, &rb1, keyA, keyB);

	for(i = 0; i < 3; i++){
		ra->val[i] = vzip1q_s32(ra1.val[i], rb1.val[i]);
		rb->val[i] = vzip2q_s32(ra1.val[i], rb1.val[i]);
	}
}

#endif
