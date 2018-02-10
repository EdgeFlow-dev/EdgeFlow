#include "xplane_filter.h"
#include "params.h"
#include <string.h>		// for memcpy


/* This function makes four uint16 element one uint64 */
unsigned long all_filtered(uint32x4_t v32)
{
	uint16x4_t v16 = vqmovn_u32(v32);
	uint64x1_t result = vreinterpret_u64_u16(v16);
	return result[0];
}

#if 1
uint32_t xplane_neon_filter_band(batch_t **dests, batch_t *src, x_params *params){
	int32_t *src_data = src->start;
	int32_t *out[params->n_outputs];
	idx_t vpos = params->vpos;
	idx_t reclen = params->reclen;

	int32x4x3_t ra;

	register int32x4_t v_lower;
	register int32x4_t v_higher;

	uint32_t i;
	uint32_t cnt = 0;

	uint32x4_t low_mask;
	uint32x4_t high_mask;
	uint32x4_t filtered_mask;

	uint64_t filtered;
//	uint32x4_t result;
	uint32x4_t s_result;

	v_lower		= vdupq_n_s32(params->lower);
	v_higher	= vdupq_n_s32(params->higher);

	for(i = 0; i < params->n_outputs; i++)
		out[i] = dests[i]->start;

	/* ra->val[t_idx] ... if index of key is 0, t_idx = 0*/
	while((void *)src_data < src->end){
		/* hp: ra->val[t_idx] <- int32x4_t filled with keys */
		ra = vld3q_s32((int32_t *)src_data);

		low_mask = vcgtq_s32(ra.val[vpos], v_lower);
		high_mask = vcltq_s32(ra.val[vpos], v_higher);
		filtered_mask = vandq_u32(low_mask, high_mask);

		s_result = vshrq_n_u32(filtered_mask, 31);
		filtered = all_filtered(s_result);
		if(filtered){
			for(i = 0; i < 4; i++){
				memcpy(out[cnt], src_data, sizeof(int32_t) * reclen);
				src_data += reclen;
				out[cnt] += (s_result[i] * reclen);
				cnt = (cnt + 1) % params->n_outputs;
			}
		} else{
			src_data += (4 * reclen);
		}
	}
	for(i = 0; i < params->n_outputs; i++){
		batch_update(dests[i], out[i]);
		DMSG("filtered batch: %p, # of items : %d", (void *)dests[i], SIZE_TO_ITEM(dests[i]->size));
	}
	return X_SUCCESS;
}
#endif

uint32_t xplane_scalar_filter_band(batch_t **dests, batch_t *src, x_params *params){
	int32_t *src_data = src->start;
	int32_t *out[params->n_outputs];
	idx_t vpos 		= params->vpos;
	idx_t reclen 	= params->reclen;
	int32_t lower 	= (int32_t) params->lower;
	int32_t higher 	= (int32_t) params->higher;


	uint32_t i;
	uint32_t cnt = 0;

	int32x4x3_t ra;

	for(i = 0; i < params->n_outputs; i++)
		out[i] = dests[i]->start;

	while((void *) src_data < src->end){
		ra = vld3q_s32((int32_t *)src_data);
		for(i = 0; i < 4; i++){
			if(ra.val[vpos][i] > lower &&
					ra.val[vpos][i] < higher){
				memcpy(out[cnt], src_data, sizeof(int32_t) * reclen);
				out[cnt] += reclen;
				cnt = (cnt + 1) % params->n_outputs;
			}
			src_data += reclen;
		}
	}

	for(i = 0; i < params->n_outputs; i++){
		batch_update(dests[i], out[i]);
		X2("filtered batch: %p, # of items : %d", (void *)dests[i], SIZE_TO_ITEM(dests[i]->size));
	}
	return X_SUCCESS;
}


TEE_Result xfunc_filter(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]){
	x_params *params    = (x_params *)get_params(p);
	x_addr *outbuf      = (x_addr *)get_outbuf(p);
	batch_t *src        = (batch_t *)params->srcA;

	uint32_t n_outputs = params->n_outputs;
	batch_t *dests[n_outputs];

	int32_t xret = 0;
	uint32_t i;

	TEE_Time t_start, t_end;

	X2("[invoke filter] src addr : %p", (void *)src);

	if ((TEE_PARAM_TYPE_GET(type, 1) == TEE_PARAM_TYPE_MEMREF_INOUT) &&
			(TEE_PARAM_TYPE_GET(type, 2) == TEE_PARAM_TYPE_MEMREF_INOUT)){

	}

	/* hp: check src has nothing, if then return empty batches */
	if (!src->size) {
		batch_t *tmp_batch;
		for (i = 0; i < n_outputs; i++) {
			tmp_batch = batch_new(X_CMD_FILTER);
			batch_update(tmp_batch, tmp_batch->start);
			outbuf[i] = (x_addr) tmp_batch;
			batch_escape(X_CMD_FILTER);
		}
		x_returnA(p, X_SUCCESS);
		return TEE_SUCCESS;
	}
	/***********************************************************/

	for(i = 0; i < n_outputs; i++){
		dests[i] = batch_new(X_CMD_FILTER);
		if(!dests[i]){
		    x_returnA(p, -X_ERR_NO_BATCH);
			return TEE_SUCCESS;
		}

		*outbuf++ = (x_addr) dests[i];
		batch_escape(X_CMD_FILTER);
	}

	tee_time_get_sys_time(&t_start);

	xret = xplane_scalar_filter_band(dests, src, params);
//	xret = xplane_neon_filter_band(dests, src, params);

	tee_time_get_sys_time(&t_end);

	set_outbuf_size(p, params->n_outputs * sizeof(x_addr));
	x_returnA(p, xret);
	x_returnB(p, get_delta_time_in_ms(t_start, t_end));
	return TEE_SUCCESS;
}






