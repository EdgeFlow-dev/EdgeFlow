#include <time.h>
#include <string.h>

#include "xplane/xplane_median.h"
#include "xplane/params.h"

/* Assume src is sorted by key after sorted by val */
int32_t x_kpercentile_bykey(batch_t **dests, batch_t *src, x_params *params){
	idx_t reclen	= params->reclen;
	idx_t keypos	= params->keypos;
//	idx_t vpos		= params->vpos

	float k			= params->k;
//	uint8_t reverse = params->reverse;

	simd_t *bundle_start 	= src->start;
	simd_t *bundle_end		= src->start;
	simd_t curkey		= bundle_start[keypos];

	simd_t *out[params->n_outputs];
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

	while(bundle_end < src->end){
		if(curkey != bundle_end[keypos]){
			n = (bundle_end - bundle_start) / reclen;

			nk = (int32_t)(n * k);
#ifdef CHECK_OVERFLOW
			batch_check_overflow(dests[cnt], &out[cnt], reclen);
#endif
			memcpy(out[cnt], (bundle_start + (nk * reclen)),
					sizeof(simd_t) * reclen);

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

int32_t x_kpercentile_all(batch_t **dest, batch_t *src, x_params *params){
	idx_t reclen	= params->reclen;
//	idx_t vpos		= params->vpos

	float k			= params->k;
//	uint8_t reverse = params->reverse;
//
	simd_t *out;
	int32_t n = src->size / reclen;	/* # of records */
	int32_t nk = 0;

#ifdef X_DEBUG
	if(k < 0 || k > 1)
		return -X_ERR_PARAMS;
#endif

	X2("enter %s", __func__);

	out = dest[0]->start;	/* only one output */

	nk = (int32_t)(n * k);
#ifdef CHECK_OVERFLOW
	batch_check_overflow(dest[0], &out, reclen);
#endif
	memcpy(out, (src->start + (nk * reclen)),
			sizeof(simd_t) * reclen);

	out += reclen;
	batch_update(dest[0], out);

	return X_SUCCESS;
}

/* Assume src is sorted by key after sorted by val */
int32_t x_topk_bykey(batch_t **dests, batch_t *src, x_params *params){
	idx_t reclen	= params->reclen;
	idx_t keypos	= params->keypos;
//	idx_t vpos		= params->vpos

	float k			= params->k;
	uint8_t reverse = params->reverse;

	simd_t *bundle_start 	= src->start;
	simd_t *bundle_end		= src->start;
	simd_t curkey		= bundle_start[keypos];

	simd_t *out[params->n_outputs];
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

	while(bundle_end < src->end){
		if(curkey != bundle_end[keypos]){
			n = (bundle_end - bundle_start) / reclen;

			nk = (int32_t)(n * k);

#ifdef CHECK_OVERFLOW
			batch_check_overflow(dests[cnt], &out[cnt], nk * reclen);
#endif
			if(!reverse){
				memcpy(out[cnt], bundle_start,
					nk * sizeof(simd_t) * reclen);
			} else{
				memcpy(out[cnt], bundle_end - (nk * reclen),
					nk * sizeof(simd_t) * reclen);
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
int32_t x_med_bykey(batch_t **dests, batch_t *src, x_params *params){
	idx_t reclen	= params->reclen;
	idx_t keypos	= params->keypos;
	idx_t vpos		= params->vpos;

	simd_t *bundle_start 	= src->start;
	simd_t *bundle_end		= src->start;
	simd_t curkey		= bundle_start[keypos];

	simd_t *out[params->n_outputs];

	int32_t cnt = 0;
	int32_t n = 0;
	uint32_t i;


	X2("enter %s", __func__);

	for(i = 0; i < params->n_outputs; i++){
		out[i] = dests[i]->start;
	}

	while(bundle_end < src->end){
		if(curkey != bundle_end[keypos]){
			n = (bundle_end - bundle_start) / reclen;
			if(n % 2 ){
				(bundle_start + (reclen * (n/2)))[vpos] +=
					(bundle_start + (reclen * (n/2 - 1)))[vpos];
				(bundle_start + (reclen * (n/2)))[vpos] /= 2;
			}
#ifdef CHECK_OVERFLOW
			batch_check_overflow(dests[cnt], &out[cnt], reclen);
#endif
			memcpy(out[cnt], bundle_start + (reclen * (n/2)),
					sizeof(simd_t) * reclen);

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

int32_t x_med_all(batch_t *dest, batch_t *src, x_params *params){
	XMSG(YELLOW "med_all" RESET);
	dest = dest;
	src = src;
	params = params;

	return X_SUCCESS;
}


TEE_Result xfunc_median(TEE_Param p[TEE_NUM_PARAMS]){
	x_params *params	= (x_params *)get_params(p);
	x_addr *outbuf		= (x_addr *)get_outbuf(p);
	batch_t *src		= (batch_t *)params->srcA;

	idx_t reclen = params->reclen;
	uint32_t n_outputs 	= params->n_outputs;
	batch_t *dests[n_outputs];

	uint32_t i;
	int32_t xret = 0;

	uint32_t items_per_dest;
	uint32_t rem;

	TEE_Time t_start, t_end;

#ifdef X_DEBUG
	/* median uses keypos */
	if(pos_checker(params->reclen, params->keypos)){
		x_returnA(p, -X_ERR_POS);
		return TEE_SUCCESS;
	}
#endif

	X2("enter %s", __func__);

	items_per_dest = (src->size / params->reclen) / n_outputs;
	rem = (src->size / params->reclen) % n_outputs;

	/****************************************/
	clock_gettime(CLOCK_MONOTONIC, &t_start);

	for(i = 0; i < n_outputs - 1; i++){
		/* allocate memory (buf_size / n_outputs) on each */
		dests[i] = batch_new(X_CMD_MEDIAN, items_per_dest * reclen * sizeof(simd_t));
		if(!dests[i]){
			x_returnA(p, -X_ERR_NO_BATCH);
			return TEE_SUCCESS;
		}
		*outbuf++ = (x_addr) dests[i];
		batch_escape(X_CMD_MEDIAN);
	}

	dests[i] = batch_new(X_CMD_MEDIAN, (items_per_dest + rem) * reclen * sizeof(simd_t));
	if(!dests[i]){
		x_returnA(p, -X_ERR_NO_BATCH);
		return TEE_SUCCESS;
	}
	*outbuf++ = (x_addr) dests[i];
	batch_escape(X_CMD_MEDIAN);

	switch(params->func){
		case med_bykey_t:
			xret = x_med_bykey(dests, src, params);
			break;
		case topk_bykey_t:
			xret = x_topk_bykey(dests, src, params);
			break;
		case kpercentile_bykey_t:
			xret = x_kpercentile_bykey(dests, src, params);
			break;

		case kpercentile_all_t:
			xret = x_kpercentile_all(dests, src, params);
			break;
		case med_all_t:
			xret = x_med_all(*dests, src, params);
			break;
		case med_all_s_t:
			//			xret = x_med_all_s(dests, src, params);
			break;
		default:
			break;
	}
	clock_gettime(CLOCK_MONOTONIC, &t_end);
	/*****************************************/

	set_outbuf_size(p, params->n_outputs * sizeof(x_addr));
	x_returnA(p, xret);
	x_returnB(p, get_delta_time_in_ms(t_start, t_end));
	return TEE_SUCCESS;
}


/* the invariant of taking kbatch and pbatch as input. output is still *record batch* (not k p batches)
 *
 * @dest_new_records OUT: produced new records. Each new record has TWO columns. First: key; second: v
 *
 * @offset: offset of the actual input within src_k and src_p. in simd_t.
 * @len: length of the actual input within src_k and src_p. in simd_t.
 *
 * @vpos: v offset within a Record (in simd_t) // For future use case, like dereference
 * @reclen: # of simd_t in a Record
 *
 * return: error code or 0 on success
 */
int32_t
kpercentile_bykey_one_part_k(x_addr *dest_new_records, x_addr src_k, x_addr src_p,
		uint32_t offset, uint32_t len_i, idx_t vpos, idx_t reclen, float topP) {
	/* don't know exact size of output */
//	batch_t *batch_dest = batch_new(X_CMD_MEDIAN, (len_i * topP) * 2 /* key + value */ * sizeof(simd_t));
	batch_t *batch_dest = batch_new(X_CMD_MEDIAN, len_i * 2 /* key + value */ * sizeof(simd_t));

	batch_t *batch_k = (batch_t *) src_k;
	batch_t *batch_p = (batch_t *) src_p;

	simd_t *data_k			= batch_k->start;
	simd_t *data_p 			= batch_p->start;
	simd_t *data_dest		= batch_dest->start;

	simd_t curkey			= data_k[offset];
	uint64_t i, temp;
	uint64_t n, nk;

	if(topP < 0 || topP > 1) {
		abort();
	}

	for(i = offset, temp = offset; i < offset + len_i; i++) {
		if(curkey != data_k[i]) {
			n = i - temp;
			nk = (simd_t)(n * topP);
			/* copy both key from src_k and value from src_p
			   src_p contains points so use vpos to pickt it out */

			memcpy(data_dest++, &data_k[temp + nk], sizeof(simd_t));
			memcpy(data_dest++, (((simd_t *) data_p[temp + nk]) + vpos), sizeof(simd_t));

			temp = i;
			curkey = data_k[i];
		}
	}
	n = i - temp;
	nk = (simd_t)(n * topP);
	/* copy both key from src_k and value from src_p
	   src_p contains points so use vpos to pickt it out */

	memcpy(data_dest++, &data_k[temp + nk], sizeof(simd_t));
	memcpy(data_dest++, (((simd_t *) data_p[temp + nk]) + vpos), sizeof(simd_t));

	batch_update(batch_dest, data_dest);

	*dest_new_records = (x_addr)batch_dest;
	return 0;
}

/* the invariant of taking kbatch and pbatch as input. output is still *record batch* (not k p batches)
 *
 * @dest_new_records OUT: produced new records. Each new record has TWO columns. First: key; second: v
 *
 * @offset: offset of the actual input within src_k and src_p. in simd_t.
 * @len: length of the actual input within src_k and src_p. in simd_t.
 *
 * @vpos: v offset within a Record (in simd_t) // For future use case, like dereference
 * @reclen: # of simd_t in a Record
 *
 * return: error code or 0 on success
 */
int32_t
topk_bykey_one_part_k(x_addr *dest_new_records, x_addr src_k, x_addr src_p,
		uint32_t offset, uint32_t len_i, idx_t vpos, idx_t reclen, float topP) {
	/* simply allocate same size of memory to dest batch */
//	batch_t *batch_dest = batch_new(X_CMD_MEDIAN, (len_i * topP) * 2 /* key + value */ * sizeof(simd_t));
	batch_t *batch_dest = batch_new(X_CMD_MEDIAN, len_i * 2 /* key + value */ * sizeof(simd_t));

	batch_t *batch_k = (batch_t *) src_k;
	batch_t *batch_p = (batch_t *) src_p;

	simd_t *data_k			= batch_k->start;
	simd_t *data_p 			= batch_p->start;
	simd_t *data_dest		= batch_dest->start;

	simd_t curkey			= data_k[offset];
	uint64_t i, temp;
	uint64_t n, nk;

	if(topP < 0 || topP > 1) {
		abort();
	}

	for(i = offset, temp = offset; i < offset + len_i; i++) {
		if(curkey != data_k[i]) {
			n = i - temp;
			nk = (simd_t)(n * topP);
			/* copy both key from src_k and value from src_p
			   src_p contains points so use vpos to pickt it out */

			for(uint64_t j = 0; j <= nk; j++){
				memcpy(data_dest++, &data_k[temp + j], sizeof(simd_t));
				memcpy(data_dest++, (((simd_t *) data_p[temp + j]) + vpos), sizeof(simd_t));
			}

			temp = i;
			curkey = data_k[i];
		}
	}
	n = i - temp;
	nk = (simd_t)(n * topP);
	/* copy both key from src_k and value from src_p
	   src_p contains points so use vpos to pickt it out */

	for(uint64_t j = 0; j <= nk; j++){
		memcpy(data_dest++, &data_k[temp + j], sizeof(simd_t));
		memcpy(data_dest++, (((simd_t *) data_p[temp + j]) + vpos), sizeof(simd_t));
	}

	batch_update(batch_dest, data_dest);

	*dest_new_records = (x_addr)batch_dest;
	return 0;
}

