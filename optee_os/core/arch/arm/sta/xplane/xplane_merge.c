#include "xplane_merge.h"
#include "params.h"

#ifndef MERGE_WIDTH
#define MERGE_WIDTH 4 /* 4, 8, 16 */
#endif

int32_t xplane_merge(x_addr *outbuf, x_params *params){
	batch_t *srcA       = (batch_t *)params->srcA;  
	batch_t *srcB       = (batch_t *)params->srcB;
	batch_t *dest;

	idx_t keypos = params->keypos;
	idx_t reclen = params->reclen;

	uint32_t n_outputs = params->n_outputs;

	int32_t *srcA_data, *srcB_data; 
	int32_t *dest_data;

	uint32_t items_per_destA = (srcA->size / reclen) / n_outputs;
	uint32_t items_per_destB = (srcB->size / reclen) / n_outputs;
	uint32_t remA = (srcA->size / reclen) % n_outputs;
	uint32_t remB = (srcB->size / reclen) % n_outputs;

	uint32_t i = 0;
	uint32_t st;

	X2("n_outputs : %d", n_outputs);
	X2("srcA : %p, srcB: %p", (void *)srcA, (void *)srcB);
	X2("items_per_destA : %d, items_per_destB : %d",
			items_per_destA, items_per_destB);
	XMSG("remA : %d remB : %d", remA, remB);

	st = atomic_load(&srcA->state);
	if(st != BATCH_STATE_OPEN){
		EMSG("input batch is not open. st %u", st);
		panic("bug");
	}

	st = atomic_load(&srcB->state);
	if(st != BATCH_STATE_OPEN){
		EMSG("input batch is not open. st %u", st);
		panic("bug");
	}

	for(i = 0; i < n_outputs - 1; i++){
		srcA_data = srcA->start + (items_per_destA * reclen * i);
		srcB_data = srcB->start + (items_per_destB * reclen * i);
		dest = batch_new(X_CMD_MERGE);

		if(!dest) 
    		return -X_ERR_NO_BATCH;

		dest_data = dest->start;
		/* TODO distinguish whether it is aligend or not ? */
		switch(reclen){
			case 3:
				x3_merge4_not_aligned(srcA_data, srcB_data, dest_data, 
							items_per_destA, items_per_destB, keypos);
				break;
			case 4:
				x4_merge4_not_aligned(srcA_data, srcB_data, dest_data, 
							items_per_destA, items_per_destB, keypos);
				break;
			default:
				return -X_ERR_WRONG_VAL;
				break;
		}
		batch_update(dest, dest_data + 
						(items_per_destA + items_per_destB) * reclen);
		*outbuf++ = (x_addr)dest;
		batch_escape(X_CMD_MERGE);
		DMSG("[%d merged] srcA size: %d srcB size: %d dest size: %d", i, items_per_destA, items_per_destB, dest->size);
	}

	srcA_data = srcA->start + (items_per_destA * i * reclen);
	srcB_data = srcB->start + (items_per_destB * i * reclen);

	items_per_destA += (remA * reclen);
	items_per_destB += (remB * reclen);

	dest = batch_new(X_CMD_MERGE);
	if(!dest) 
   		return -X_ERR_NO_BATCH;
	dest_data = dest->start;

	switch(reclen){
		case 3:
//			x3_merge4_aligned(srcA_data, srcB_data, dest_data, 
//						items_per_destA, items_per_destB, keypos);
			x3_merge4_not_aligned(srcA_data, srcB_data, dest_data, 
						items_per_destA, items_per_destB, keypos);
			break;
		case 4:
			x4_merge4_not_aligned(srcA_data, srcB_data, dest_data, 
						items_per_destA, items_per_destB, keypos);
			break;
		default:
			return -X_ERR_WRONG_VAL;
			break;
	}

	batch_update(dest, dest_data + 
					(items_per_destA + items_per_destB) * reclen);
	*outbuf++ = (x_addr)dest;
	batch_escape(X_CMD_MERGE);
	DMSG("[%d merged] srcA size: %d srcB size: %d dest size: %d", i, items_per_destA, items_per_destB, dest->size);

#ifdef IS_SORTED
	if(x_is_sorted(dest->start, (srcA->size + srcB->size) / reclen, keypos))
		XMSG(RED "sorting is correct" RESET);
#endif
	return X_SUCCESS;
}


TEE_Result xfunc_merge(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]){
	x_params *params	= (x_params *) get_params(p);
	x_addr *outbuf		= (x_addr *) get_outbuf(p);
	batch_t *srcA		= (batch_t * )params->srcA;
	batch_t *srcB		= (batch_t *) params->srcB;
	int32_t xret;

	TEE_Time t_start, t_end;

#ifdef X_DEBUG
	if(pos_checker(params->reclen, params->keypos)){
		x_returnA(p, -X_ERR_POS);
		return TEE_SUCCESS;
	}
#endif

	if (!srcA || !srcB) {
		EMSG("bug: srcA or srcB nullptr");
		x_return(p, -1);
		return TEE_SUCCESS;
	}

//	EMSG("srcA start_pointer : %p, srcA size : %d\n", (void *) srcA->start, srcA->size);
//	EMSG("srcB start_pointer : %p, srcB size : %d\n", (void *) srcB->start, srcB->size);
	if (srcA->start + srcA->size != srcA->end)
		EMSG("-------------- srcA size is large than end of batch");
	if (srcB->start + srcB->size != srcB->end)
		EMSG("-------------- srcB size is large than end of batch");

	if (!srcA->size || !srcB->size) {
		batch_t *tmp_batch;
		for (uint32_t i = 0; i < params->n_outputs; i++) {
			tmp_batch = batch_new(X_CMD_MERGE);
			batch_update(tmp_batch, tmp_batch->start);
			outbuf[i] = (x_addr) tmp_batch;
			batch_escape(X_CMD_MERGE);
		}
		x_return(p, X_SUCCESS);
		return TEE_SUCCESS;
	}

	X2("enter %s", __func__);
	if ((TEE_PARAM_TYPE_GET(type, 1) == TEE_PARAM_TYPE_MEMREF_INOUT) &&
			(TEE_PARAM_TYPE_GET(type, 2) == TEE_PARAM_TYPE_MEMREF_INOUT) &&
			(TEE_PARAM_TYPE_GET(type, 3) == TEE_PARAM_TYPE_MEMREF_INOUT)){
	}

	tee_time_get_sys_time(&t_start);
	xret = xplane_merge(outbuf, params);
	tee_time_get_sys_time(&t_end);

	/* X2("exec time : %d ms", get_delta_time_in_ms(t_start, t_end)); */

	set_outbuf_size(p, params->n_outputs * sizeof(x_addr));
	x_returnA(p, xret);
	x_returnB(p, get_delta_time_in_ms(t_start, t_end));
	return TEE_SUCCESS;
}
