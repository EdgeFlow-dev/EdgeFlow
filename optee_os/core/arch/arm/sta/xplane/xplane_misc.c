#include "xplane_misc.h"
#include "params.h"


int32_t unique_key(x_addr *dest, batch_t *src, x_params *params){
	batch_t *dest_batch = batch_new(X_CMD_MISC);
	idx_t reclen		= params->reclen;
	idx_t keypos		= params->keypos;
//	idx_t vpos			= params->vpos;
//	idx_t countpos		= params->countpos;

	int32_t *src_data	= src->start;
	int32_t *dest_data	= dest_batch->start;
	int32_t curkey		= src_data[keypos];

	X2("unique_key");
	if(!dest_batch)
    	return -X_ERR_NO_BATCH;

	/* start with value pos and continue adding reclen */
	while((void *)src_data < src->end){
		if(curkey != src_data[keypos]){
			dest_data[keypos] 	= curkey;
			X2("curkey : %d next : %d", curkey, src_data[keypos]);
//			dest_data[vpos] 	= 0;
//			dest_data[countpos] = 0;

			curkey = src_data[keypos];
			dest_data += reclen;
		}
		src_data += reclen;
	}

	batch_update(dest_batch, dest_data);
	batch_escape(X_CMD_MISC);
	*dest = (x_addr)dest_batch;
	return X_SUCCESS;
}

TEE_Result xfunc_misc(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]){
	x_params *params	= (x_params *)get_params(p);
	x_addr *outbuf		= (x_addr *)get_outbuf(p);
	batch_t *src		= (batch_t *)params->srcA;

	int32_t xret = 0;

	TEE_Time t_start, t_end;

#ifdef X_DEBUG
	if(pos_checker(params->reclen, params->keypos) ||
		pos_checker(params->reclen, params->vpos)){
		x_returnA(p, -X_ERR_POS);
		return TEE_SUCCESS;
	}
#endif

	X2("invoke xfunc misc");

	if ((TEE_PARAM_TYPE_GET(type, 1) == TEE_PARAM_TYPE_MEMREF_INOUT) &&
			(TEE_PARAM_TYPE_GET(type, 2) == TEE_PARAM_TYPE_MEMREF_INOUT)){
	}

	/* hp: check src has nothing, if then return empty batches */
	if (!src->size) {
		batch_t *tmp_batch;
		for (uint32_t i = 0; i < params->n_outputs; i++) {
			tmp_batch = batch_new(X_CMD_MISC);
			batch_update(tmp_batch, tmp_batch->start);
			outbuf[i] = (x_addr) tmp_batch;
			batch_escape(X_CMD_MISC);
		}
		x_returnA(p, X_SUCCESS);
		return TEE_SUCCESS;
	}
	/***********************************************************/
	/* TODO Uniqeu key doesn't support n_outputs yet */

	tee_time_get_sys_time(&t_start);

	switch(params->func){
		case unique_key_t:
			xret = unique_key(outbuf, src, params);
			break;
		default:
			x_return(p, X_ERR_NO_FUNC);
			return TEE_SUCCESS;
			break;
	}

	tee_time_get_sys_time(&t_end);

	set_outbuf_size(p, (params->n_outputs) * sizeof(x_addr));
	x_returnA(p, xret);
	x_returnB(p, get_delta_time_in_ms(t_start, t_end));
	return TEE_SUCCESS;
}
