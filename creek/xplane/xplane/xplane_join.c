#include <time.h>

#include "xplane/xplane_join.h"
#include "xplane/params.h"

extern void sz_show_resource(void);

int32_t x_join_bykey(x_addr *dest, batch_t *srcA, batch_t *srcB, x_params *params){
	idx_t reclen        = params->reclen;
	idx_t keypos        = params->keypos;
	batch_t *dest_batch = batch_new(X_CMD_JOIN, (srcA->size + srcB->size) * reclen * sizeof(simd_t));

	simd_t *dest_data  = dest_batch->start;

	simd_t *srcA_data = srcA->start;
	simd_t *srcB_data = srcB->start;
	simd_t *src_tmp;

	uint32_t matches = 0;
	int32_t matched_key;

	if(!dest_batch)
		return -X_ERR_NO_BATCH;

#ifdef X_DEBUG
	/* join_bykey uses keypos */
	if(pos_checker(params->reclen, params->keypos))
		return -X_ERR_POS;
#endif

	while(srcA_data < srcA->end && srcB_data < srcB->end){
		if(srcA_data[keypos] < srcB_data[keypos])
			srcA_data += reclen;
		else if(srcA_data[keypos] > srcB_data[keypos])
			srcB_data += reclen;
		else{	/* keyA == key B */
			matched_key = srcA_data[keypos];
			matches = 0;
			src_tmp = srcA_data;

			do{
				matches++;
				srcA_data += reclen;
			}while(srcA_data[keypos] == matched_key && srcA_data < srcA->end);
#ifdef CHECK_OVERFLOW
			batch_check_overflow(dest_batch, &dest_data, matches * reclen);
#endif
			memcpy(dest_data, src_tmp, sizeof(simd_t) * reclen * matches);
			dest_data += (srcA_data - src_tmp);

			matches = 0;
			src_tmp = srcB_data;
			do{
				matches++;
				srcB_data += reclen;
			}while(srcB_data[keypos] == matched_key && srcB_data < srcB->end);
#ifdef CHECK_OVERFLOW
			batch_check_overflow(dest_batch, &dest_data, matches * reclen);
#endif
			memcpy(dest_data, src_tmp, sizeof(simd_t) * reclen * matches);
			dest_data += (srcB_data - src_tmp);


		}

	}
	batch_update(dest_batch, dest_data);
	batch_escape(X_CMD_JOIN);
	*dest = (x_addr)dest_batch;
	return X_SUCCESS;
}

int32_t x_join_byfilter(x_addr *dest, batch_t *srcA, batch_t *srcB, x_params *params){
	idx_t reclen        = params->reclen;
	idx_t vpos			= params->vpos;
	batch_t *dest_batch = batch_new(X_CMD_JOIN, (srcA->size + srcB->size) * reclen * sizeof(simd_t));

	simd_t *dest_data  = dest_batch->start;

	simd_t *srcA_data = srcA->start;
	simd_t *srcB_data = srcB->start;

	if(!dest_batch)
		return -X_ERR_NO_BATCH;

#ifdef X_DEBUG
	/* join_byfilter uses vpos */
	if(pos_checker(params->reclen, params->vpos))
		return -X_ERR_POS;
#endif

	/* assume that srcB has only one record */
	while(srcA_data < srcA->end){
		if(srcA_data[vpos] > srcB_data[vpos]){
#ifdef CHECK_OVERFLOW
			batch_check_overflow(dest_batch, &dest_data, reclen);
#endif
			memcpy(dest_data, srcA_data, sizeof(simd_t) * reclen);
			dest_data += reclen;
		}
		srcA_data += reclen;
	}

	batch_update(dest_batch, dest_data);
	batch_escape(X_CMD_JOIN);
	*dest = (x_addr)dest_batch;
	return X_SUCCESS;
}

TEE_Result xfunc_join(TEE_Param p[TEE_NUM_PARAMS]){
	x_params *params	= (x_params *)get_params(p);
	x_addr *outbuf		= (x_addr *)get_outbuf(p);
	batch_t *srcA		= (batch_t *)params->srcA;
	batch_t *srcB		= (batch_t *)params->srcB;

	TEE_Time t_start, t_end;
	int32_t xret = 0;

/*
	if ((TEE_PARAM_TYPE_GET(type, 1) == TEE_PARAM_TYPE_MEMREF_INOUT) &&
			(TEE_PARAM_TYPE_GET(type, 2) == TEE_PARAM_TYPE_MEMREF_INOUT)){
	}
*/


	clock_gettime(CLOCK_MONOTONIC, &t_start);


	switch(params->func){
		case join_bykey_t:
			xret = x_join_bykey(outbuf, srcA, srcB, params);
			break;
		case join_byfilter_t:
			xret = x_join_byfilter(outbuf, srcA, srcB, params);
			break;
		default:
			break;
	}

	clock_gettime(CLOCK_MONOTONIC, &t_end);

	set_outbuf_size(p, (params->n_outputs) * sizeof(x_addr));
	x_returnA(p, xret);
	x_returnB(p, get_delta_time_in_ms(t_start, t_end));
	return TEE_SUCCESS;
}
