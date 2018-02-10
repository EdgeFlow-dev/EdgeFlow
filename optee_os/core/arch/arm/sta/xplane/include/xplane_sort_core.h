#ifndef XPLANE_SORT_CORE_H
#define XPLANE_SORT_CORE_H

#include "type.h"
#include "neon_common.h"
#include "xplane_common.h"

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
#endif
