//
// Created by xzl on 11/30/17.
// separated from common_sort_merge.c

#include <stdlib.h>
#include <string.h>
#include <log.h>
#include "xplane/qsort_cmp.h"
#include "mm/batch.h"
#include "xplane_lib.h"

#define SORT_STABLE	1

/* @vpos: alrady sorted pos */
int32_t sort_stable(x_addr *dests, uint32_t n_outputs, x_addr src, idx_t keypos, idx_t vpos, idx_t reclen){
	batch_t *src_in = (batch_t *) src;
	uint64_t len = src_in->size/reclen; // # of records

	/* first (n_outputs - 1) partition have the same len */
	uint64_t each_len = len/n_outputs;
	/* last partition may has different len with previous partitions' len */
	uint64_t last_len = len - (n_outputs - 1)*each_len;

	simd_t* idx = src_in->start;

	/* sort first (n_outputs - 1) parts*/
	for(unsigned int i = 0; i < n_outputs - 1; i++){
		batch_t *tmp = batch_new(SORT_STABLE, each_len * reclen * sizeof(simd_t));
		memcpy(tmp->start, idx, each_len * reclen * sizeof(simd_t));
		batch_update(tmp, tmp->start + each_len * reclen);
		dests[i] = (x_addr) tmp;
		idx = idx + tmp->size;

		switch(keypos){
			case 0: /* assume sorted by key 1 */
				switch(vpos){
					case 1:
						qsort(tmp->start, each_len / reclen,
									sizeof(simd_t) * reclen, sort0_sorted1);
						break;
					case 2:
						qsort(tmp->start, each_len / reclen,
									sizeof(simd_t) * reclen, sort0_sorted2);
						break;
					case 3:
						qsort(tmp->start, each_len / reclen,
									sizeof(simd_t) * reclen, sort0_sorted3);
						break;
					default:
						//return -X_ERR_POS;
						EE("wrong vpos");
						abort();
				}
				break;
			case 1:
				switch(vpos){
					case 0:
						qsort(tmp->start, each_len / reclen,
									sizeof(simd_t) * reclen, sort1_sorted0);
						break;
					case 2:
						qsort(tmp->start, each_len / reclen,
									sizeof(simd_t) * reclen, sort1_sorted2);
						break;
					case 3:
						qsort(tmp->start, each_len / reclen,
									sizeof(simd_t) * reclen, sort1_sorted3);
						break;
					default:
						//return -X_ERR_POS;
						EE("wrong vpos");
						abort();
				}
				break;
			case 2:	/* assume sorted by key 1 */
				switch(vpos){
					case 0:
						qsort(tmp->start, each_len / reclen,
									sizeof(simd_t) * reclen, sort2_sorted0);
						break;
					case 1:
						qsort(tmp->start, each_len / reclen,
									sizeof(simd_t) * reclen, sort2_sorted1);
						break;
					case 3:
						qsort(tmp->start, each_len / reclen,
									sizeof(simd_t) * reclen, sort2_sorted3);
						break;
					default:
						//return -X_ERR_POS;
						EE("wrong vpos");
						abort();
				}
				break;
			case 3:	/* assume sorted by key 1 */
				switch(vpos){
					case 0:
						qsort(tmp->start, each_len / reclen,
									sizeof(simd_t) * reclen, sort3_sorted0);
						break;
					case 1:
						qsort(tmp->start, each_len / reclen,
									sizeof(simd_t) * reclen, sort3_sorted1);
						break;
					case 2:
						qsort(tmp->start, each_len / reclen,
									sizeof(simd_t) * reclen, sort3_sorted2);
						break;
					default:
						//return -X_ERR_POS;
						EE("wrong vpos");
						abort();
				}
				break;
			default:
				//return -X_ERR_POS;
				/* wrong vpos*/
				EE("wrong keypos");
				abort();
		}
	}

	/* sort last part */
	batch_t *tmp = batch_new(SORT_STABLE, last_len * reclen * sizeof(simd_t));
	memcpy(tmp->start, idx, last_len * reclen * sizeof(simd_t));
	batch_update(tmp, tmp->start + last_len * reclen);
	dests[n_outputs - 1] = (x_addr) tmp;
	idx = idx + tmp->size;
	switch(keypos){
		case 0: /* assume sorted by key 1 */
			switch(vpos){
				case 1:
					qsort(tmp->start, last_len / reclen,
								sizeof(simd_t) * reclen, sort0_sorted1);
					break;
				case 2:
					qsort(tmp->start, last_len / reclen,
								sizeof(simd_t) * reclen, sort0_sorted2);
					break;
				case 3:
					qsort(tmp->start, last_len / reclen,
								sizeof(simd_t) * reclen, sort0_sorted3);
					break;
				default:
					//return -X_ERR_POS;
					EE("wrong vpos");
					abort();
			}
			break;
		case 1:
			switch(vpos){
				case 0:
					qsort(tmp->start, last_len / reclen,
								sizeof(simd_t) * reclen, sort1_sorted0);
					break;
				case 2:
					qsort(tmp->start, last_len / reclen,
								sizeof(simd_t) * reclen, sort1_sorted2);
					break;
				case 3:
					qsort(tmp->start, last_len / reclen,
								sizeof(simd_t) * reclen, sort1_sorted3);
					break;
				default:
					//return -X_ERR_POS;
					EE("wrong vpos");
					abort();
			}
			break;
		case 2:	/* assume sorted by key 1 */
			switch(vpos){
				case 0:
					qsort(tmp->start, last_len / reclen,
								sizeof(simd_t) * reclen, sort2_sorted0);
					break;
				case 1:
					qsort(tmp->start, last_len / reclen,
								sizeof(simd_t) * reclen, sort2_sorted1);
					break;
				case 3:
					qsort(tmp->start, last_len / reclen,
								sizeof(simd_t) * reclen, sort2_sorted3);
					break;
				default:
					//return -X_ERR_POS;
					EE("wrong vpos");
					abort();
			}
			break;
		case 3:	/* assume sorted by key 1 */
			switch(vpos){
				case 0:
					qsort(tmp->start, last_len / reclen,
								sizeof(simd_t) * reclen, sort3_sorted0);
					break;
				case 1:
					qsort(tmp->start, last_len / reclen,
								sizeof(simd_t) * reclen, sort3_sorted1);
					break;
				case 2:
					qsort(tmp->start, last_len / reclen,
								sizeof(simd_t) * reclen, sort3_sorted2);
					break;
				default:
					//return -X_ERR_POS;
					EE("wrong vpos");
					abort();
			}
			break;
		default:
			//return -X_ERR_POS;
			/* wrong vpos*/
			EE("wrong keypos");
			abort();
	}
	return 0;
}


