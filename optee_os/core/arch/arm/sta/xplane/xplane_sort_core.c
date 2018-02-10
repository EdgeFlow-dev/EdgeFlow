#include <stdatomic.h>

#include <trace.h>
#include <string.h>
#include "utils.h"
#include "xplane_sort_core.h"
#include "xplane_common.h"
#include "params.h"

void x3_qsort(x3_t *arr, int l, int r, uint32_t cmp){
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

/* hp : for x3 */
/* # of items need to be asgiend with 16 */
/* @ inbatch : one large stream */
/* @ outarr : sorted array in terms of L1 cache size */
//item_t *L1_chunk_sort_tmp(item_t *inarr, item_t *outarr, uint32_t t_idx){


/* hp : for x3 */
/* # of items need to be asgiend with 16 */
/* @ inbatch : one large stream */
/* @ outarr : sorted array in terms of L1 cache size */
int32_t *L1_chunk_sort(int32_t *inarr, int32_t *outarr, idx_t keypos, idx_t reclen){
	int32_t *ptrs[2];
	int32_t *in 	= inarr;
	int32_t *out	= outarr;
	int32_t *end	= inarr + (ITEMS_PER_L1 * reclen);

	void (*inregister_sort) (int32_t *input, int32_t *output, uint32_t keypos);
	void (*inregister_merge) (int32_t * const inpA, int32_t * const inpB,
						int32_t * const out, const uint32_t lenA,
						const uint32_t lenB, uint32_t keypos);

	uint32_t i;
	int32_t ptridx;
	int32_t inlen, outlen;

//	XMSG("in : %p, out %p end :%p", (void *)in, (void *)out, (void *)end);

	inregister_sort		= x3_inregister_sort;
	inregister_merge	= x3_merge4_aligned;

	switch(reclen){
		case 1:
			inregister_sort		= x1_inregister_sort;
			inregister_merge	= x1_merge4_eqlen_aligned;
			break;
		case 3:
			inregister_sort		= x3_inregister_sort;
			inregister_merge	= x3_merge4_aligned;
			break;
		case 4:
			inregister_sort 	= x4_inregister_sort;
			inregister_merge	= x4_merge4_aligned;
			break;
		default:
			break;
	}

	while(in < end){
		/* sort each 4 items with 3 elements of array */
		/* output : sorted 4 items <itme0 | item1 | item2 | item 3> */
		inregister_sort(in, out, keypos);
//		XMSG("in : %p, out %p end :%p", (void *)in, (void *)out, (void *)end);
		in += 16 * reclen;	/* hp: total 16 items can be sorted */
		out += 16 * reclen;	/* # of items need to be asgiend with 16 */
	}

	ptrs[0] = outarr;
	ptrs[1] = inarr;

	for(i = 2; i < my_log2(ITEMS_PER_L1 / 4) + 2; i++){
		ptridx = i & 1;
		in = ptrs[ptridx];
		out = ptrs[ptridx ^ 1];
		end = in + ITEMS_PER_L1 * reclen;

//		XMSG("in : %p, out %p end :%p", (void *)in, (void *)out, (void *)end);
		inlen = (1 << i);	/* start with 4 */
		outlen = (inlen << 1);


//		XMSG("inptr : %p, outptr : %p endptr : %p outelen : %d", (void *)ptrs[ptridx], (void *)ptrs[ptridx ^ 1], (void *)end,  outlen);
		while(in < end){
			inregister_merge(in, (in + (inlen * reclen)), out, inlen, inlen, keypos);
			in	+= (outlen * reclen);
			out	+= (outlen * reclen);
		}
	}

#ifdef IS_SORTED
	if(x_is_sorted((int32_t*) outarr, ITEMS_PER_L1, keypos, reclen))
		X2("L1 sorting is correct");
#endif
//	out = outarr + (ITEMS_PER_L1 * reclen);
//	XMSG("in : %p, out %p", (void *)in, (void *)out);
	return out;
}


/* Merge two batches, and return the merged one.
 * Will create a new batch for return.
 * As side effect, will kill @inA and @inB.
 *
 * caller is reponsible to close the returned batch */
/* status of input batches should be "OPEN" */
batch_t *merge_two_batches(batch_t *inA, batch_t *inB, uint32_t t_idx, idx_t reclen) {
	int32_t *inpA	= (int32_t *)inA->start;
	int32_t *inpB	= (int32_t *)inB->start;
	int32_t *out;
	int32_t sizeA = SIZE_TO_ITEM(inA->size);
	int32_t sizeB = SIZE_TO_ITEM(inB->size);


	void (*merge) (int32_t * const inpA, int32_t * const inpB,
						int32_t * const out, const uint32_t lenA,
						const uint32_t lenB, uint32_t keypos);

	/* close previous batches */
	if (batch_close(inA, inA->end) != X_SUCCESS)
		return NULL;

	if (batch_close(inB, inB->end) != X_SUCCESS)
		return NULL;

	batch_t *dest = batch_new_after(inB, X_CMD_SORT);
	if(!dest){
		EMSG("No batch available");
		return NULL;
	}

	out	= (int32_t *)dest->start;

	switch(reclen){
		case 1:
			merge = x1_merge4_eqlen_aligned;
			break;
		case 3:
			merge = x3_merge4_not_aligned;
			break;
		case 4:
			merge = x4_merge4_not_aligned;
			break;
		default:
			merge = x3_merge4_not_aligned;
			break;
	}

	/* hp : can use x3_merge4_eqlen */

	merge(inpA, inpB, out, sizeA, sizeB, t_idx);
	batch_update(dest, out + inA->size + inB->size);

//	XMSG("dest->size : %d", dest->size);
//	XMSG("dest addr : %p", (void *) dest);
#ifdef IS_SORTED
	if(!x_is_sorted((int32_t *) dest->start, dest->size / ELES_PER_ITEM, t_idx))
		XMSG(RED "[merged] sorting is incorrect" RESET);
#endif

	BATCH_KILL(inA, X_CMD_SORT);
	BATCH_KILL(inB, X_CMD_SORT);
	return dest;
}
extern uint64_t cmp_time;
/* @input size need to be same as L2 Cache size  */
/* size should be size of item */
/* static inline uint64_t sz_time_diff(TEE_Time *start, TEE_Time *end){ */
/*     return (end->seconds - start->seconds) * 1000000 + end->micros - start->micros; */
/* } */

static inline uint64_t sz_time_diff(TEE_Time *start, TEE_Time *end){
    return (end->seconds - start->seconds) * 1000000 + end->micros - start->micros;
}

//#define TIME_MEASURE
batch_t *merge_chunks(int32_t *start, uint32_t nitems, idx_t keypos, idx_t reclen){
	batch_t *left, *right;
	int32_t *start_left = start;
	int32_t *start_right = start + ((nitems / 2) * reclen);
        batch_t *ret;
#ifdef TIME_MEASURE
        TEE_Time start_t, end_t;
#endif
	if(nitems > ITEMS_PER_L1){
		left = merge_chunks(start_left, nitems / 2, keypos, reclen);
		if(!left) return NULL;

		right = merge_chunks(start_right, nitems / 2, keypos, reclen);
		if(!right) return NULL;
#ifdef TIME_MEASURE
                tee_time_get_sys_time(&start_t);
#endif
                ret =  merge_two_batches(left, right, keypos, reclen);
#ifdef TIME_MEASURE
                tee_time_get_sys_time(&end_t);
		atomic_fetch_add(&cmp_time, sz_time_diff(&start_t, &end_t));
#endif
		return ret;
	} else{
//		XMSG("make L1 chunk [size] : %d", nitems);
		left = batch_new(X_CMD_SORT);
		if(!left)
		    return NULL;
#ifdef TIME_MEASURE
                tee_time_get_sys_time(&start_t);
#endif
		L1_chunk_sort(start, left->start, keypos, reclen);
#ifdef TIME_MEASURE
                tee_time_get_sys_time(&end_t);
                atomic_fetch_add(&cmp_time, sz_time_diff(&start_t, &end_t));
#endif
//		L1_chunk_sort_tmp(start, left->start, keypos, reclen);
		batch_update(left, left->start + nitems * reclen);
		return left;
	}
}
