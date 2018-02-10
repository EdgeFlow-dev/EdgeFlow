#include <stdlib.h>
#include <string.h>
#include <log.h>
#include "xplane/qsort_cmp.h"
#include "mm/batch.h"
#include "xplane_lib.h" 


/* generic impl of sort/merge. only gets compiled when arch does not impl sort/merge */

#define COMMON_SORT_MERGE	1

/* use qsort */
static int cmp_keypos0(const void *a, const void *b){
	return ((simd_t *)a)[0] - ((simd_t *)b)[0];
}

static int cmp_keypos1(const void *a, const void *b){
	return ((simd_t *)a)[1] - ((simd_t *)b)[1];
}

static int cmp_keypos2(const void *a, const void *b){
	return ((simd_t *)a)[2] - ((simd_t *)b)[2];
}

static int cmp_keypos3(const void *a, const void *b){
	return ((simd_t *)a)[3] - ((simd_t *)b)[3];
}

int32_t sort(x_addr *dests, uint32_t n_outputs, x_addr src,
				idx_t keypos, idx_t reclen){
	batch_t *src_batch = (batch_t *) src;
	batch_t *out[n_outputs];
	simd_t *src_data = src_batch->start;

	int (*cmp_func)(const void *a, const void *b);
	uint32_t items_per_dest = (src_batch->size / reclen) / n_outputs;
	uint32_t rem = (src_batch->size / reclen) % n_outputs;
	uint32_t i;

	DD("input size : %d\n", src_batch->size);
	DD("items_per_dest : %d\n", items_per_dest);

	switch(keypos){
		case 0:
			cmp_func = cmp_keypos0;
			break;
		case 1:
			cmp_func = cmp_keypos1;
			break;
		case 2:
			cmp_func = cmp_keypos2;
			break;
		default:
			cmp_func = cmp_keypos3;
			break;
	}

	for(i = 0 ; i < n_outputs - 1; i++){
		// params: arbitrary num
		// hp: change batch_new later
		out[i] = batch_new(COMMON_SORT_MERGE, items_per_dest * reclen * sizeof(simd_t));
		qsort(src_data, items_per_dest,
				sizeof(simd_t *) * reclen, cmp_func);
#ifdef CHECK_OVERFLOW
		batch_check_overflow(out[i], &out[i]->start, items_per_dest);
#endif
		memcpy(out[i]->start, src_data, items_per_dest * reclen * sizeof(simd_t));
		batch_update(out[i], out[i]->start + reclen * items_per_dest);
		src_data += items_per_dest;
		dests[i] = (x_addr) out[i];
	}
	out[i] = batch_new(COMMON_SORT_MERGE, (items_per_dest + rem) * reclen * sizeof(simd_t));
	qsort(src_data, items_per_dest + rem,
			sizeof(simd_t *) * reclen, cmp_func);

#ifdef CHECK_OVERFLOW
	batch_check_overflow(out[i], &out[i]->start, items_per_dest);
#endif
	memcpy(out[i]->start, src_data, items_per_dest * reclen * sizeof(simd_t));
	batch_update(out[i], out[i]->start + reclen * (items_per_dest + rem));
	dests[i] = (x_addr) out[i];

	return 0;
}

static int32_t merge_core(batch_t *dest, simd_t *srcA, simd_t *srcB,
				uint32_t lenA, uint32_t lenB, idx_t keypos, idx_t reclen){
	simd_t *out = dest->start;
	simd_t *in;
	uint32_t cmp;
	uint32_t ai = 0, bi = 0;

	DD("keypos : %d\n", keypos);
	while(ai < lenA && bi < lenB){
		in = srcB;
		cmp = srcA[keypos] < srcB[keypos];

		ai += cmp;
		bi += (!cmp);

		if(cmp){
			in = srcA;
		}
#ifdef CHECK_OVERFLOW
		batch_check_overflow(dest, &out, reclen);
#endif
		memcpy(out, in, sizeof(simd_t) * reclen);
		out += reclen;
		srcA += (cmp * reclen);
		srcB += ((!cmp) * reclen);
	}

	if(ai < lenA){
#ifdef CHECK_OVERFLOW
		batch_check_overflow(dest, &out, reclen * (lenA - ai));
#endif
		memcpy(out, srcA, sizeof(simd_t) * reclen * (lenA - ai));
	} else{
#ifdef CHECK_OVERFLOW
		batch_check_overflow(dest, &out, reclen * (lenB - bi));
#endif
		memcpy(out, srcB, sizeof(simd_t) * reclen * (lenB - bi));
	}
	return 0;
}

int32_t merge(x_addr *dests, uint32_t n_outputs, x_addr srca, x_addr srcb,
				idx_t keypos, idx_t reclen){
	batch_t *srcA = (batch_t *) srca;
	batch_t *srcB = (batch_t *) srcb;
	batch_t *out[n_outputs];

	simd_t *dataA = srcA->start;
	simd_t *dataB = srcB->start;

	uint32_t items_per_destA = (srcA->size / reclen) / n_outputs;
	uint32_t items_per_destB = (srcB->size / reclen) / n_outputs;
	uint32_t remA = (srcA->size / reclen) % n_outputs;
	uint32_t remB = (srcB->size / reclen) % n_outputs;
	uint32_t i = 0;

	uint32_t lenA = items_per_destA;
	uint32_t lenB = items_per_destB;

	for(i = 0; i <n_outputs - 1; i++){
		out[i] = batch_new(COMMON_SORT_MERGE, (lenA + lenB) * reclen * sizeof(simd_t));
		merge_core(out[i], dataA, dataB, lenA, lenB, keypos, reclen);
		batch_update(out[i], out[i]->start + reclen * (lenA + lenB));
		dests[i] = (x_addr) out[i];
	}

	lenA = items_per_destA + remA;
	lenB = items_per_destB + remB;
	out[i] = batch_new(COMMON_SORT_MERGE, (lenA + lenB) * reclen * sizeof(simd_t));
	merge_core(out[i], dataA, dataB, lenA, lenB, keypos, reclen);
	batch_update(out[i], out[i]->start + reclen * (lenA + lenB));
	dests[i] = (x_addr) out[i];

	return 0;
}

