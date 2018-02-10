#include "xplane_median.h"
#include "xplane_sort.h"
#include "params.h"

/* Assume src is sorted by key after sorted by val */
int32_t kpercentile_bykey(batch_t **dests, batch_t *src, x_params *params){
	idx_t reclen	= params->reclen;
	idx_t keypos	= params->keypos;
//	idx_t vpos		= params->vpos

	float k			= params->k;
//	uint8_t reverse = params->reverse;

	int32_t *bundle_start 	= src->start;
	int32_t *bundle_end		= src->start;
	int32_t curkey		= bundle_start[keypos];

	int32_t *out[params->n_outputs];
	int32_t cnt = 0;
	int32_t n = 0;
	int32_t nk = 0;
	uint32_t i;

#ifdef X_DEBUG
	if(k < 0 || k > 1)
		return -X_ERR_PARAMS;
#endif

	X2("enter %s", __func__);

	for(i = 0; i < params->n_outputs; i++){
		out[i] = dests[i]->start;
	}

	while((void *)bundle_end < src->end){
		if(curkey != bundle_end[keypos]){
			n = (bundle_end - bundle_start) / reclen;

			nk = (int32_t)(n * k);
			memcpy(out[cnt], (bundle_start + (nk * reclen)),
					sizeof(int32_t) * reclen);

			bundle_start = bundle_end;
			curkey = bundle_end[keypos];

			out[cnt] += reclen;
			cnt = (cnt + 1) % params->n_outputs;
		}
		bundle_end += reclen;	/* next record */
	}

	for(i = 0; i < params->n_outputs; i++){
		batch_update(dests[i], out[i]);
		X2("top k %p, # of items : %d", (void *)dests[i], dests[i]->size / reclen);
	}

	return X_SUCCESS;
}

int32_t kpercentile_all(batch_t **dest, batch_t *src, x_params *params){
	idx_t reclen	= params->reclen;
//	idx_t vpos		= params->vpos

	float k			= params->k;
//	uint8_t reverse = params->reverse;
//
	int32_t *out;
	int32_t n = src->size / reclen;	/* # of records */
	int32_t nk = 0;

#ifdef X_DEBUG
	if(k < 0 || k > 1)
		return -X_ERR_PARAMS;
#endif

	X2("enter %s", __func__);

	out = dest[0]->start;	/* only one output */

	nk = (int32_t)(n * k);
	memcpy(out, (src->start + (nk * reclen)),
			sizeof(int32_t) * reclen);

	out += reclen;
	batch_update(dest[0], out);

	return X_SUCCESS;
}

/* Assume src is sorted by key after sorted by val */
int32_t topk_bykey(batch_t **dests, batch_t *src, x_params *params){
	idx_t reclen	= params->reclen;
	idx_t keypos	= params->keypos;
//	idx_t vpos		= params->vpos

	float k			= params->k;
	uint8_t reverse = params->reverse;

	int32_t *bundle_start 	= src->start;
	int32_t *bundle_end		= src->start;
	int32_t curkey		= bundle_start[keypos];

	int32_t *out[params->n_outputs];
	int32_t cnt = 0;
	int32_t n = 0;
	int32_t nk = 0;
	uint32_t i;


#ifdef X_DEBUG
	if(k < 0 || k > 1)
		return -X_ERR_PARAMS;
#endif

	X2("enter %s", __func__);

	for(i = 0; i < params->n_outputs; i++){
		out[i] = dests[i]->start;
	}

	while((void *)bundle_end < src->end){
		if(curkey != bundle_end[keypos]){
			n = (bundle_end - bundle_start) / reclen;

			nk = (int32_t)(n * k);

			if(!reverse){
				memcpy(out[cnt], bundle_start,
					nk * sizeof(int32_t) * reclen);
			} else{
				memcpy(out[cnt], bundle_end - (nk * reclen),
					nk * sizeof(int32_t) * reclen);
			}

			bundle_start = bundle_end;
			curkey = bundle_end[keypos];

			out[cnt] += nk * reclen;
			cnt = (cnt + 1) % params->n_outputs;
		}
		bundle_end += reclen;	/* next record */
	}



	for(i = 0; i < params->n_outputs; i++){
		batch_update(dests[i], out[i]);
		X2("top k %p, # of items : %d", (void *)dests[i], dests[i]->size / reclen);
	}

	return X_SUCCESS;
}

/* Assume src is sorted by key after sorted by val */
int32_t med_bykey(batch_t **dests, batch_t *src, x_params *params){
	idx_t reclen	= params->reclen;
	idx_t keypos	= params->keypos;
	idx_t vpos		= params->vpos;

	int32_t *bundle_start 	= src->start;
	int32_t *bundle_end		= src->start;
	int32_t curkey		= bundle_start[keypos];

	int32_t *out[params->n_outputs];

	int32_t cnt = 0;
	int32_t n = 0;
	uint32_t i;


	X2("enter %s", __func__);

	for(i = 0; i < params->n_outputs; i++){
		out[i] = dests[i]->start;
	}

	while((void *)bundle_end < src->end){
		if(curkey != bundle_end[keypos]){
			n = (bundle_end - bundle_start) / reclen;
			if(n % 2 ){
				(bundle_start + (reclen * (n/2)))[vpos] +=
					(bundle_start + (reclen * (n/2 - 1)))[vpos];
				(bundle_start + (reclen * (n/2)))[vpos] /= 2;
			}
			memcpy(out[cnt], bundle_start + (reclen * (n/2)),
					sizeof(int32_t) * reclen);

			bundle_start = bundle_end;
			curkey = bundle_end[keypos];

			out[cnt] += reclen;
			cnt = (cnt + 1) % params->n_outputs;
		}
		bundle_end += reclen;	/* next record */
	}


	for(i = 0; i < params->n_outputs; i++){
		batch_update(dests[i], out[i]);
		X2("median batch %p, # of items : %d", (void *)dests[i], dests[i]->size / reclen);
	}
	return X_SUCCESS;
}

int32_t med_all(batch_t *dest, batch_t *src, x_params *params){
	XMSG(YELLOW "med_all" RESET);
	dest = dest;
	src = src;
	params = params;

	return X_SUCCESS;
}


TEE_Result xfunc_median(__unused uint32_t type, TEE_Param p[TEE_NUM_PARAMS]){
	x_params *params	= (x_params *)get_params(p);
	x_addr *outbuf		= (x_addr *)get_outbuf(p);
	batch_t *src		= (batch_t *)params->srcA;

	uint32_t n_outputs 	= params->n_outputs;
	batch_t *dests[n_outputs];

	uint32_t i;
	int32_t xret = 0;

	TEE_Time t_start, t_end;

#ifdef X_DEBUG
	/* median uses keypos */
	if(pos_checker(params->reclen, params->keypos)){
		x_returnA(p, -X_ERR_POS);
		return TEE_SUCCESS;
	}
#endif

	X2("enter %s", __func__);

//	if ((TEE_PARAM_TYPE_GET(type, 1) == TEE_PARAM_TYPE_MEMREF_INOUT) &&
//			(TEE_PARAM_TYPE_GET(type, 2) == TEE_PARAM_TYPE_MEMREF_INOUT)){
//	}

	/* hp: check src has nothing, if then return empty batches */
	if (!src->size) {
		batch_t *tmp_batch;
		for (i = 0; i < n_outputs; i++) {
			tmp_batch = batch_new(X_CMD_MEDIAN);
			batch_update(tmp_batch, tmp_batch->start);
			outbuf[i] = (x_addr) tmp_batch;
			batch_escape(X_CMD_MEDIAN);
		}
		x_returnA(p, X_SUCCESS);
		return TEE_SUCCESS;
	}
	/***********************************************************/

	/****************************************/
	tee_time_get_sys_time(&t_start);

	for(i = 0; i < n_outputs; i++){
		dests[i] = batch_new(X_CMD_MEDIAN);
		if(!dests[i]){
			x_returnA(p, -X_ERR_NO_BATCH);
			return TEE_SUCCESS;
		}
		*outbuf++ = (x_addr) dests[i];
		batch_escape(X_CMD_MEDIAN);
	}

	switch(params->func){
		case med_bykey_t:
			xret = med_bykey(dests, src, params);
			break;
		case topk_bykey_t:
			xret = topk_bykey(dests, src, params);
			break;
		case kpercentile_bykey_t:
			kpercentile_bykey(dests, src, params);
			break;

		case kpercentile_all_t:
			kpercentile_all(dests, src, params);
			break;
		case med_all_t:
			xret = med_all(*dests, src, params);
			break;
		case med_all_s_t:
//			xret = med_all_s(dests, src, params);
			break;
		default:
			break;
	}
	tee_time_get_sys_time(&t_end);
	/*****************************************/

	set_outbuf_size(p, params->n_outputs * sizeof(x_addr));
	x_returnA(p, xret);
	x_returnB(p, get_delta_time_in_ms(t_start, t_end));
	return TEE_SUCCESS;
}
