#include <time.h>

#include "xplane/xplane_sort.h"
#include "xplane/xplane_sort_core.h"
#include "xplane/params.h"

#include "measure.h"
#include "log.h"

//#include <stdatomic.h>
//#define Q_SORT

/* params must have keypos, vpos (used as already sorted key) */
int32_t xplane_qsort(x_addr *dests, int32_t *src_data, int32_t *src_end, x_params *params){
	uint32_t src_size 		= src_end - src_data;
	idx_t reclen			= params->reclen;
	idx_t keypos			= params->keypos;
	// NOTE: use vpos as already sorted key
	idx_t vpos			= params->vpos;

	batch_t *tmp			= batch_new(X_CMD_SORT, (src_end - src_data) * reclen * sizeof(simd_t));

	XMSG("qsort start");
	X2("keypos : %d vpose : %d", keypos, vpos);
	X2("%ld", &src_data[10] - &src_data[20]);
	switch(keypos){
		case 0: /* assume sorted by key 1 */
#if 0
			/* just for measuring perf of x1 with qsort; is not stable */
			qsort(src_data, src_size / reclen,
					sizeof(int32_t) * reclen, cmpfunc_0);
#endif
			switch(vpos){
				case 1:
					qsort(src_data, src_size / reclen,
						sizeof(int32_t) * reclen, sort0_sorted1);
					break;
				case 2:
					qsort(src_data, src_size / reclen,
						sizeof(int32_t) * reclen, sort0_sorted2);
					break;
				case 3:
					qsort(src_data, src_size / reclen,
						sizeof(int32_t) * reclen, sort0_sorted3);
					break;
				default:
					return -X_ERR_POS;
			}
			break;
		case 1:
			switch(vpos){
				case 0:
					qsort(src_data, src_size / reclen,
						sizeof(int32_t) * reclen, sort1_sorted0);
					break;
				case 2:
					qsort(src_data, src_size / reclen,
						sizeof(int32_t) * reclen, sort1_sorted2);
					break;
				case 3:
					qsort(src_data, src_size / reclen,
						sizeof(int32_t) * reclen, sort1_sorted3);
					break;
				default:
					return -X_ERR_POS;
			}
			break;
		case 2:	/* assume sorted by key 1 */
			switch(vpos){
				case 0:
					qsort(src_data, src_size / reclen,
						sizeof(int32_t) * reclen, sort2_sorted0);
					break;
				case 1:
					qsort(src_data, src_size / reclen,
						sizeof(int32_t) * reclen, sort2_sorted1);
					break;
				case 3:
					qsort(src_data, src_size / reclen,
						sizeof(int32_t) * reclen, sort2_sorted3);
					break;
				default:
					return -X_ERR_POS;
			}
			break;
		case 3:	/* assume sorted by key 1 */
			switch(vpos){
				case 0:
					qsort(src_data, src_size / reclen,
						sizeof(int32_t) * reclen, sort3_sorted0);
					break;
				case 1:
					qsort(src_data, src_size / reclen,
						sizeof(int32_t) * reclen, sort3_sorted1);
					break;
				case 2:
					qsort(src_data, src_size / reclen,
						sizeof(int32_t) * reclen, sort3_sorted2);
					break;
				default:
					return -X_ERR_POS;
			}
			break;
		default:
			return -X_ERR_POS;
			X2("err");
			break;
	}

	memcpy(tmp->start, src_data, sizeof(int32_t) * src_size);
	batch_update(tmp, tmp->start + src_size);
	batch_escape(X_CMD_SORT);

	*dests = (x_addr) tmp;

	XMSG("sorted batch : %p, # of items : %d", (void *)tmp, tmp->size/reclen);

#ifdef IS_SORTED
//#if 1
	if(x_is_sorted((int32_t *) tmp->start, \
		tmp->size / reclen /* # of items */, keypos /* target index */, reclen))
		X2(RED "sorting is correct" RESET);
#endif
	return X_SUCCESS;
}

/* extract # of items inside of src */ 
/* @chunk : L1 cache size / 2, @segment : L2 cache size */
extern uint64_t cmp_time;
int32_t xplane_sort(x_addr *dests, int32_t *src_data, int32_t *src_end, x_params *params){
	batch_t *tmp			= NULL;
	idx_t reclen			= params->reclen;
	idx_t keypos			= params->keypos;

//	W("src_data : %p", (void *) src_data);
	uint32_t n_chunks 		= ((src_end - src_data) / reclen) / ITEMS_PER_L1;
	uint32_t L1_unaligned 	= ((src_end - src_data) / reclen) % ITEMS_PER_L1;
	uint32_t i, j;

	uint32_t dest_chunks;
	uint32_t n_merged;
	uint32_t cnt;
        /* TEE_Time start_t, end_t; */


	/* # of 1 in n_chunks bits */
	n_merged = bitcnt(n_chunks);
	x_addr merged[n_merged];

//	W("n_merged : %d", n_merged);
	/* all fianl merged and sorted chunks and segments are opened yet */
	if(n_chunks & 0x1){	/* odd number */
		for(i = 0 ; i < n_merged - 1; i++){
			dest_chunks = x_pow(2, my_log2(n_chunks));

			XMSG("n_chunks : %d", n_chunks);
			XMSG("dest chunks: %u", dest_chunks);
           	tmp = merge_chunks(src_data, dest_chunks * ITEMS_PER_L1, keypos, reclen);

//			EE("tmp size : %d, buf_size : %d", tmp->size, tmp->buf_size);

			if(!tmp)
		    	return -X_ERR_NO_BATCH;
			xpos_mov(src_data, reclen, dest_chunks * ITEMS_PER_L1);

			merged[i] = (x_addr) tmp;
			n_chunks ^= dest_chunks;
		}
		/* sort last 1 chunk */
		tmp = batch_new(X_CMD_SORT, ITEMS_PER_L1 * reclen * sizeof(simd_t));
		if(!tmp)
		    return -X_ERR_NO_BATCH;

		L1_chunk_sort(src_data, tmp->start, keypos, reclen);
		// TODO: dynamic L1 merge
//		L1_chunk_sort_tmp(src_data, tmp->start, keypos, reclen);
		batch_update(tmp, tmp->start + ITEMS_PER_L1 * reclen);

//		EE("tmp size : %d, buf_size : %d", tmp->size, tmp->buf_size);

		xpos_mov(src_data, reclen, ITEMS_PER_L1);
		merged[n_merged - 1] = (x_addr) tmp;
	} else {
		for(i = 0 ; i < n_merged; i++){
			dest_chunks = x_pow(2, my_log2(n_chunks));


			XMSG("n_chunks : %d", n_chunks);
			XMSG("dest chunks: %u", dest_chunks);
                        /* tee_time_get_sys_time(&start_t); */
			tmp = merge_chunks(src_data, dest_chunks * ITEMS_PER_L1, keypos, reclen);
                        /* tee_time_get_sys_time(&end_t); */
                        /* atomic_fetch_add(&cmp_time, sz_time_diff(&start_t, &end_t)); */
			if(!tmp)
		    	return -X_ERR_NO_BATCH;

//			EE("tmp size : %d, buf_size : %d", tmp->size, tmp->buf_size);

			xpos_mov(src_data, reclen, dest_chunks * ITEMS_PER_L1);

			merged[i] = (x_addr) tmp;
			n_chunks ^= dest_chunks;
		}
	}
/*********************/
	if(L1_unaligned){
		x3_qsort((x3_t *)src_data, 0, L1_unaligned - 1, keypos);
//		qsort(src_data, L1_unaligned, sizeof(x3_t), cmpfunc);

		tmp = batch_new(X_CMD_SORT, L1_unaligned * reclen * sizeof(simd_t));
		if(!tmp)
		    return -X_ERR_NO_BATCH;


		memcpy(tmp->start, src_data, L1_unaligned * reclen * sizeof(int32_t));
		batch_update(tmp, tmp->start + L1_unaligned * reclen);

//		EE("tmp size : %d, buf_size : %d", tmp->size, tmp->buf_size);

		merged[n_merged++] = (x_addr) tmp;
	}
/*********************/


	/* xzl: @n_merged: # of total batches to merge in each pass */
	for(i = 0; n_merged > 2; i++){
		cnt = 0;
		for(j = 0; j < n_merged - 1; j += 2){
                    /* tee_time_get_sys_time(&start_t); */
			tmp = merge_two_batches((batch_t *)merged[j],
						(batch_t *)merged[j+1], keypos, reclen);
                        /* tee_time_get_sys_time(&end_t); */
                        /* atomic_fetch_add(&cmp_time, sz_time_diff(&start_t, &end_t)); */

			if(!tmp) {
				perror("bug: merge failed");
    		return -X_ERR_NO_BATCH;
			}

//			EE("tmp size : %d, buf_size : %d", tmp->size, tmp->buf_size);

			merged[cnt++] = (x_addr) tmp;
			/* TODO need to batch close since merged one is not closed yet */
		}
		if(n_merged % 2 != 0)
			merged[cnt++] = merged[j];

		n_merged = cnt;   /* xzl: @n_merged shrinks here */
	}

//	W("n_merged : %d", n_merged);
	if(n_merged == 2) {
		/* merged_two_batches returns the merged batch without closing */
            /* tee_time_get_sys_time(&start_t); */
            tmp = merge_two_batches((batch_t *)merged[0], 
					(batch_t *)merged[1], keypos, reclen);
                /* tee_time_get_sys_time(&end_t); */
                /* atomic_fetch_add(&cmp_time, sz_time_diff(&start_t, &end_t)); */
		if(!tmp) {
			perror("bug: merge failed.");
	    return -X_ERR_NO_BATCH;
		}
	} else {
		merged[0] = merged[0];
		tmp = (batch_t *)merged[0];
	

	}
//	W("tmp size : %d, buf_size : %d", tmp->size, tmp->buf_size);
	if (tmp->size > tmp->buf_size) {
		EE("size is larger than the allocated");
		abort();
	}
	*dests = (x_addr) tmp;

#if 0
	uint32_t st = atomic_load(&tmp->state);
	if (st != BATCH_STATE_OPEN) {
		EMSG("sort returns a batch which is not open. st %u", st);
		perror("bug");
	}
#endif

#ifdef IS_SORTED
//#if 1
	if(x_is_sorted((int32_t *) tmp->start, \
		tmp->size / reclen /* # of items */, keypos /* target index */, reclen))
		X2(RED "sorting is correct" RESET);
#endif
	batch_escape(X_CMD_SORT);
	return X_SUCCESS;
}

TEE_Result xfunc_sort(x_arg p[NUM_ARGS]){
	x_params *params    = (x_params *)get_params(p);
	x_addr *outbuf      = (x_addr *)get_outbuf(p);
	batch_t *src        = (batch_t *)params->srcA;

//	EE("src->size: %d src->buf_size: %d", src->size, src->buf_size);

	int32_t *src_data	= src->start;
	int32_t *src_end	= src->end;
	int32_t src_size    = src->size;
	uint32_t n_outputs  = params->n_outputs;

	uint32_t i = 0;
	uint32_t items_per_dest;
	uint32_t rem;
	int32_t xret = 0;

	uint32_t t_start = 0, t_end = 0;

#ifdef X_DEBUG
	/* sort uses keypos */
	if(pos_checker(params->reclen, params->keypos)){
		x_return(p, -X_ERR_POS);
		return TEE_SUCCESS;
	}
#endif

	/* hp: in case, src batch has nothing
 	 * just create empty batches and return them */
	if (!src_size) {
		batch_t *tmp_batch;
		for (i = 0; i < n_outputs; i++) {
			tmp_batch = batch_new(X_CMD_SORT, 0);
			outbuf[i] = (x_addr) tmp_batch;
		}
		x_returnA(p, X_SUCCESS);
		return TEE_SUCCESS;
	}

	X2("enter %s", __func__);
/*
	if ((TEE_PARAM_TYPE_GET(type, 1) == TEE_PARAM_TYPE_MEMREF_INOUT) &&
			(TEE_PARAM_TYPE_GET(type, 2) == TEE_PARAM_TYPE_MEMREF_INOUT)){
	}
*/

	X2(BLUE"n_outputs : %u, src-> start : %p, size %u" RESET, n_outputs, 
			(void *)src->start, src->size);

	X2("x_sort len : %d", params->reclen);

	/* split input into multiple dests */
	items_per_dest = (src_size / params->reclen) / n_outputs;
	rem = (src_size / params->reclen) % n_outputs;

//	t_start = k2_measure("test");
	switch(params->func){
		case x_sort_t:
			for(i = 0; i < n_outputs - 1; i++) {
				src_end = src_data + (items_per_dest * params->reclen);
				xret = xplane_sort(&outbuf[i], src_data, src_end, params);
				src_data += (items_per_dest * params->reclen);
			}

			/* last ouptput should include the remained */
			src_end = src_data + ((items_per_dest + rem) * params->reclen);
			xret = xplane_sort(&outbuf[i], src_data, src_end, params);
			break;
		case qsort_t:
			for(i = 0; i < n_outputs - 1; i++) {
				src_end = src_data + (items_per_dest * params->reclen);
				xret = xplane_qsort(&outbuf[i], src_data, src_end, params);
				src_data += (items_per_dest * params->reclen);
			}

			/* last ouptput should include the remained */
			src_end = src_data + ((items_per_dest + rem) * params->reclen);
			xret = xplane_qsort(&outbuf[i], src_data, src_end, params);
			break;
		case stable_sort_t:
			break;
		default:
			x_return(p, -X_ERR_NO_FUNC);
			break;
	}

//	t_end = k2_measure("sort end");
//	I(BLUE "exec : %d", t_end - t_start);

	/* EMSG(BLUE "exec time : %u ms" RESET, get_delta_time_in_ms(t_start, t_end)); */

//	EE(BLUE "exec time : %u ms" RESET, get_delta_time_in_ms(t_start, t_end));
	set_outbuf_size(p, params->n_outputs * sizeof(x_addr));
	x_returnA(p, xret);
	x_returnB(p, t_end - t_start);
//	x_returnB(p, get_delta_time_in_ms(t_start, t_end));
	return TEE_SUCCESS;
}


