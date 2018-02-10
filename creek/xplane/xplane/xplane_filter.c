#include <time.h>
#include <string.h>		// for memcpy
#include "xplane/xplane_filter.h"
#include "xplane/params.h"


#if 0
/* This function makes four uint16 element one uint64 */
unsigned long all_filtered(uint32x4_t v32)
{
	uint16x4_t v16 = vqmovn_u32(v32);
	uint64x1_t result = vreinterpret_u64_u16(v16);
	return result[0];
}

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

#endif

uint32_t xplane_scalar_filter_band(batch_t **dests, batch_t *src, x_params *params){
	simd_t *src_data = src->start;
	simd_t *out[params->n_outputs];
	idx_t vpos 		= params->vpos;
	idx_t reclen 	= params->reclen;
	int32_t lower 	= (int32_t) params->lower;
	int32_t higher 	= (int32_t) params->higher;


	uint32_t i;
	uint32_t cnt = 0;

	for(i = 0; i < params->n_outputs; i++)
		out[i] = dests[i]->start;

	while(src_data < src->end){
		if(src_data[vpos] > lower && src_data[vpos] < higher){
#ifdef CHECK_OVERFLOW
			batch_check_overflow(dests[cnt], &out[cnt], reclen);
#endif
			memcpy(out[cnt], src_data, sizeof(simd_t) * reclen);
			out[cnt] += reclen;
			cnt = (cnt + 1) % params->n_outputs;
		}
		src_data += reclen;
	}

	for(i = 0; i < params->n_outputs; i++){
		batch_update(dests[i], out[i]);
		X2("filtered batch: %p, # of items : %d", (void *)dests[i], SIZE_TO_ITEM(dests[i]->size), reclen);
	}
	return X_SUCCESS;
}

TEE_Result xfunc_filter(TEE_Param p[TEE_NUM_PARAMS]){
	x_params *params    = (x_params *)get_params(p);
	x_addr *outbuf      = (x_addr *)get_outbuf(p);
	batch_t *src        = (batch_t *)params->srcA;

	idx_t reclen = params->reclen;
	uint32_t n_outputs = params->n_outputs;
	batch_t *dests[n_outputs];

	int32_t xret = 0;
	uint32_t i;

	uint32_t items_per_dest;
	uint32_t rem;


	TEE_Time t_start, t_end;

/*
	if ((TEE_PARAM_TYPE_GET(type, 1) == TEE_PARAM_TYPE_MEMREF_INOUT) &&
			(TEE_PARAM_TYPE_GET(type, 2) == TEE_PARAM_TYPE_MEMREF_INOUT)){

	}
*/
	X2("[invoke filter] src addr : %p", (void *)src);

	items_per_dest = (src->size / params->reclen) / n_outputs;
	rem = (src->size / params->reclen) % n_outputs;

	for(i = 0; i < n_outputs - 1 ; i++){
		/* simply allocate memory with src->bufsize / n_output */
		/* hp: items_per_src can allocate exact size of memory */
		dests[i] = batch_new(X_CMD_FILTER, items_per_dest * reclen * sizeof(simd_t));
		if(!dests[i]){
		    x_returnA(p, -X_ERR_NO_BATCH);
			return TEE_SUCCESS;
		}
		*outbuf++ = (x_addr) dests[i];
		batch_escape(X_CMD_FILTER);
	}

	dests[i] = batch_new(X_CMD_FILTER, (items_per_dest + rem) * reclen * sizeof(simd_t));
	if(!dests[i]){
	    x_returnA(p, -X_ERR_NO_BATCH);
		return TEE_SUCCESS;
	}
	*outbuf++ = (x_addr) dests[i];
	batch_escape(X_CMD_FILTER);

	clock_gettime(CLOCK_MONOTONIC, &t_start);

	xret = xplane_scalar_filter_band(dests, src, params);
//	xret = xplane_neon_filter_band(dests, src, params);

	clock_gettime(CLOCK_MONOTONIC, &t_end);

	set_outbuf_size(p, params->n_outputs * sizeof(x_addr));
	x_returnA(p, xret );
	x_returnB(p, get_delta_time_in_ms(t_start, t_end));
	return TEE_SUCCESS;
}


/* filter. the k/p version.
 * @dests_p [OUT]   the survival p (support multiple outputs)
 *
 * @n_outputs # of outputs
 * @src_k, src_p  input k(for filtering) and p
 * @lower/higher: thresholds
 */
int32_t filter_band_kp(x_addr *dests_p, uint32_t n_outputs,
		            x_addr src_k, x_addr src_p, simd_t lower, simd_t higher) {
	batch_t *batch_src_k = (batch_t *) src_k;
	batch_t *batch_src_p = (batch_t *) src_p;
	batch_t *batch_dests[n_outputs];

	simd_t *data_src_k = batch_src_k->start;
	simd_t *data_src_p = batch_src_p->start;
	simd_t *data_dests[n_outputs];

	simd_t items_per_dest;
	simd_t rem;
	simd_t offset = 0;
	simd_t k = 0;

	uint32_t i;

	/* handle src_p == 0 */
	if (!src_p || !src_k) {
		for (i = 0; i < n_outputs; i++) {
			dests_p[i] = 0;
		}
	}

	items_per_dest = (batch_src_k->size) / n_outputs;
	rem = batch_src_k->size % n_outputs;

	/********** batch allocation **********/
	for (i = 0; i < n_outputs - 1 ; i++) {
		batch_dests[i] = batch_new(X_CMD_FILTER, items_per_dest * sizeof(simd_t));
		if (!batch_dests[i])
			return -1;
		data_dests[i] = batch_dests[i]->start;
		dests_p[i] = (simd_t) batch_dests[i];
	}

	batch_dests[i] = batch_new(X_CMD_FILTER, (items_per_dest + rem) * sizeof(simd_t));
	if (!batch_dests[i]){
		return -1;
	}
	data_dests[i] = batch_dests[i]->start;
	dests_p[i] = (simd_t) batch_dests[i];
	/**************************************/

	while (k < batch_src_k->size) {
		if (data_src_k[k] > lower && data_src_k[k] < higher) {
			printf("%ld\n", data_src_k[k]);
			memcpy(data_dests[offset], &data_src_p[k], sizeof(simd_t));
			data_dests[offset]++;
			offset = (offset + 1) % n_outputs;
		}
		k++;
	}

	for (i = 0; i < n_outputs; i++) {
		batch_update(batch_dests[i], data_dests[i]);
//		printf("batch : %ld, size : %ld\n", dests_p[i], batch_dests[i]->size);
	}

	return X_SUCCESS;

}
