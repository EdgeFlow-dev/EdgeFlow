#include "xplane_sumcount.h"
#include "params.h"

extern size_t get_core_pos(void);

/* TODO implement avg */
/* turn off BATCH_KILL(src) for testing */

/* no count in input but just count # of items sharing same key */
int32_t sum_perkey(x_addr *dests, int32_t *src_data, int32_t *src_end, x_params *params){
	batch_t *dest_batch = batch_new(X_CMD_SUMCOUNT);
	idx_t reclen		= params->reclen;
	idx_t keypos		= params->keypos;
	idx_t vpos			= params->vpos;
//	idx_t countpos		= params->countpos;

	int32_t *dest_data	= dest_batch->start;
	int32_t sum 		= 0;
	int32_t curkey		= src_data[keypos];

	X1("core%lu: %s enter ++++++++++++ " RESET, get_core_pos(), __func__);
	if(!dest_batch)
    	return -X_ERR_NO_BATCH;

	/* start with value pos and continue adding reclen */
	while(src_data < src_end){
		if(curkey == src_data[keypos]){
			sum += src_data[vpos];
		} else{
			dest_data[keypos] = curkey;
			dest_data[vpos] = sum;	/* store sum into vpos */
//			dest_data[countpos] = 0;

			dest_data += reclen;
			curkey = src_data[keypos];
			sum	= 0;
		}
		src_data += reclen;
	}

	batch_update(dest_batch, dest_data);
	*dests = (x_addr)dest_batch;
	batch_escape(X_CMD_SUMCOUNT);

//	X1("size of dests : %d", dest_batch->size / reclen);
	X1("core%lu: %s leave batch[%p]++++++++++++ " RESET, get_core_pos(), __func__, (void *)dest_batch);

	return X_SUCCESS;
}

/* no count in input but just count # of items sharing same key */
int32_t sumcount1_perkey(x_addr *dests, int32_t *src_data, int32_t *src_end, x_params *params){
	batch_t *dest_batch = batch_new(X_CMD_SUMCOUNT);
	idx_t reclen		= params->reclen;
	idx_t keypos		= params->keypos;
	idx_t vpos			= params->vpos;
	idx_t countpos		= params->countpos; 

	int32_t *dest_data	= dest_batch->start;
	int32_t sum 		= 0;
	int32_t count		= 0;
	int32_t curkey		= src_data[keypos];

	X1("core%lu: %s enter ++++++++++++ " RESET, get_core_pos(), __func__);
	if(!dest_batch)
    	return -X_ERR_NO_BATCH;

	/* start with value pos and continue adding reclen */
	while(src_data < src_end){
		if(curkey == src_data[keypos]){
			sum += src_data[vpos];
			count++;
		} else{
			XMSG(GREEN "curkey : %d sum : %d, count : %d" RESET, curkey, sum, count);
			dest_data[keypos] = curkey;
			dest_data[vpos] = sum;	/* store sum into vpos */
			dest_data[countpos] = count;

			dest_data += reclen;
			curkey = src_data[keypos];
			sum	= 0;
			count = 0;
		}
		src_data += reclen;
	}

	batch_update(dest_batch, dest_data);
	*dests = (x_addr)dest_batch;
	batch_escape(X_CMD_SUMCOUNT);

//	X1("size of dests : %d", dest_batch->size / reclen);
	X1("core%lu: %s leave batch[%p]++++++++++++ " RESET, get_core_pos(), __func__, (void *)dest_batch);

	return X_SUCCESS;
}

/* countpos is both in and out */
int32_t sumcount_perkey(x_addr *dests, int32_t *src_data, int32_t *src_end, x_params *params){
	batch_t *dest_batch = batch_new(X_CMD_SUMCOUNT);
	idx_t reclen		= params->reclen;
	idx_t keypos		= params->keypos;
	idx_t vpos			= params->vpos;
	idx_t countpos		= params->countpos;

	int32_t *dest_data	= dest_batch->start;
	int32_t sum 		= 0;
	int32_t count		= 0;
	int32_t curkey		= src_data[keypos];

	X2("enter : %s", __func__);
	if(!dest_batch)
    	return -X_ERR_NO_BATCH;

	/* start with value pos and continue adding reclen */
	while(src_data < src_end){
		if(curkey == src_data[keypos]){
			sum += src_data[vpos];
			count += src_data[params->countpos];
		} else{
//			XMSG(GREEN "curkey : %d sum : %d, count : %d" RESET, curkey, sum, count);
			dest_data[keypos] = curkey;
			dest_data[vpos] = sum;	/* store sum into vpos */
			dest_data[countpos] = count;

			dest_data += reclen;
			curkey = src_data[keypos];
			sum	= 0;
			count = 0;
		}
		src_data += reclen;
	}

	batch_update(dest_batch, dest_data);
	*dests = (x_addr)dest_batch;
	batch_escape(X_CMD_SUMCOUNT);
	return X_SUCCESS;
}

int32_t avg_perkey(x_addr *dests, int32_t *src_data, int32_t *src_end, x_params *params){
	batch_t *dest_batch = batch_new(X_CMD_SUMCOUNT);
	idx_t reclen		= params->reclen;
	idx_t keypos		= params->keypos;
	idx_t vpos			= params->vpos;
	idx_t countpos		= params->countpos;

	int32_t *dest_data	= dest_batch->start;
	int32_t sum 		= 0;
	int32_t count		= 0;
	int32_t curkey		= src_data[keypos];
//	float avg;

	XMSG(YELLOW "avg_perkey" RESET);
	if(!dest_batch)
    	return -X_ERR_NO_BATCH;

	/* start with value pos and continue adding reclen */
	while(src_data < src_end){
		if(curkey == src_data[keypos]){
			sum += src_data[vpos];
			count += src_data[params->countpos];
		} else{
//			XMSG(GREEN "curkey : %d sum : %d, count : %d" RESET, curkey, sum, count);
			dest_data[keypos] = curkey;
//			avg = (float)sum / count;
//			memcpy(&dest_data[vpos], &avg, sizeof(float));
			dest_data[vpos] = (float)sum / count;	/* store avg into vpos */
			dest_data[countpos] = count;

			dest_data += reclen;
			curkey = src_data[keypos];
			sum	= 0;
			count = 0;
		}
		src_data += reclen;
	}

	batch_update(dest_batch, dest_data);
	*dests = (x_addr)dest_batch;
	batch_escape(X_CMD_SUMCOUNT);
	return X_SUCCESS;
}

/*****************************************************************************/

int32_t sumcount1_all(x_addr *dest, batch_t *src, x_params *params){
	batch_t *dest_batch = batch_new(X_CMD_SUMCOUNT);
	idx_t reclen		= params->reclen;
	idx_t keypos		= params->keypos;
	idx_t vpos			= params->vpos;
	idx_t countpos		= params->countpos;

	int32_t *src_data	= src->start;
	int32_t *dest_data	= dest_batch->start;
	int32_t sum 		= 0;
	int32_t count		= 0;
	int32_t curkey		= src_data[keypos];
	uint32_t i;

	XMSG(YELLOW "sumcount1_all" RESET);
	if(!dest_batch)
    	return -X_ERR_NO_BATCH;

	/* start with value pos and continue adding reclen */
	for(i = vpos; i < src->size; i+= reclen){
		sum += src_data[i];
	}
	switch(reclen){
		case 3 :
			count = x3_size_to_item(src->size);
			break;
		case 4 :
			count = x4_size_to_item(src->size);
			break;
		default :
			XMSG("error");
			break;
	}

//	XMSG("sum : %d, count : %d", sum, count);

	dest_data[keypos] = curkey;	/* nothing in keypos */
	dest_data[vpos] = sum;	/* store sum into vpos */
	dest_data[countpos] = count;
	dest_data += reclen;

	batch_update(dest_batch, dest_data);
	*dest = (x_addr)dest_batch;
	batch_escape(X_CMD_SUMCOUNT);
	return X_SUCCESS;
}

int32_t sumcount_all(x_addr *dest, batch_t *src, x_params *params){
	batch_t *dest_batch = batch_new(X_CMD_SUMCOUNT);
	idx_t reclen		= params->reclen;
	idx_t keypos		= params->keypos;
	idx_t vpos			= params->vpos;
	idx_t countpos		= params->countpos;

	int32_t *src_data	= src->start;
	int32_t *dest_data	= dest_batch->start;
	int32_t sum 		= 0;
	int32_t count		= 0;
	int32_t curkey		= src_data[keypos];

	XMSG(YELLOW "sumcount_all" RESET);
	if(!dest_batch)
    	return -X_ERR_NO_BATCH;

	/* start with value pos and continue adding reclen */
	while((void *)src_data < src->end){
		sum += src_data[vpos];
		count += src_data[countpos];
		src_data += reclen;
	}

//	XMSG("sum : %d, count : %d", sum, count);

	dest_data[keypos] = curkey;	/* nothing in keypos */
	dest_data[vpos] = sum;	/* store sum into vpos */
	dest_data[countpos] = count;
	dest_data += reclen;

	batch_update(dest_batch, dest_data);
	*dest = (x_addr)dest_batch;
	batch_escape(X_CMD_SUMCOUNT);
	return X_SUCCESS;
}

int32_t avg_all(x_addr *dest, batch_t *src, x_params *params){
	batch_t *dest_batch = batch_new(X_CMD_SUMCOUNT);
	idx_t reclen		= params->reclen;
	idx_t keypos		= params->keypos;
	idx_t vpos			= params->vpos;
	idx_t countpos		= params->countpos;

	int32_t *src_data	= src->start;
	int32_t *dest_data	= dest_batch->start;
	int32_t sum 		= 0;
	int32_t count		= 0;
	int32_t curkey		= src_data[keypos];
//	float avg;

	XMSG(YELLOW "avg_all" RESET);
	if(!dest_batch)
    	return -X_ERR_NO_BATCH;

	/* start with value pos and continue adding reclen */
	while((void *)src_data < src->end){
		sum += src_data[vpos];
		count += src_data[countpos];
		src_data += reclen;
	}

//	XMSG("sum : %d, count : %d", sum, count);

	dest_data[keypos] = curkey;	/* nothing in keypos */
//	avg = (float)sum / count;
//	memcpy(&dest_data[vpos], &avg, sizeof(float));
	dest_data[vpos] = (float)sum / count;	/* store avg into vpos */
	dest_data[countpos] = count;
	dest_data += reclen;

	batch_update(dest_batch, dest_data);
	*dest = (x_addr)dest_batch;
	batch_escape(X_CMD_SUMCOUNT);
	return X_SUCCESS;
}

TEE_Result xfunc_sumcount(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]){
	x_params *params	= (x_params *)get_params(p);
	x_addr *outbuf		= (x_addr *)get_outbuf(p);
	batch_t *src		= (batch_t *)params->srcA;

	int32_t *src_data	= src->start;
	int32_t *src_end	= src->end;
	int32_t src_size	= src->size;
	uint32_t n_outputs 	= params->n_outputs;

	uint32_t i;
	uint32_t items_per_dest;
	uint32_t rem;
	int32_t xret = 0;

	TEE_Time t_start, t_end;

	uint32_t st;

	XMSG(YELLOW "invoke sumcount" RESET);

#ifdef X_DEBUG
	/* sumcount uses keypos,vpos and countpos */
	if(params->func == sum_perkey_t){
		if(pos_checker(params->reclen, params->keypos) ||
				pos_checker(params->reclen, params->vpos)){
			x_returnA(p, -X_ERR_POS);
			return TEE_SUCCESS;
		}
	} else{
		/* sumcount uses keypos, vpos and countpos */
		if(pos_checker(params->reclen, params->keypos) ||
				pos_checker(params->reclen, params->vpos) ||
				pos_checker(params->reclen, params->countpos)){
			x_returnA(p, -X_ERR_POS);
			return TEE_SUCCESS;
		}
	}

#endif

	/* hp: check src has nothing, if then return empty batches */
	if (!src->size) {
		batch_t *tmp_batch;
		for (i = 0; i < n_outputs; i++) {
			tmp_batch = batch_new(X_CMD_SUMCOUNT);
			batch_update(tmp_batch, tmp_batch->start);
			outbuf[i] = (x_addr) tmp_batch;
			batch_escape(X_CMD_SUMCOUNT);
		}
		x_returnA(p, X_SUCCESS);
		return TEE_SUCCESS;
	}
	/***********************************************************/

	if ((TEE_PARAM_TYPE_GET(type, 1) == TEE_PARAM_TYPE_MEMREF_INOUT) &&
			(TEE_PARAM_TYPE_GET(type, 2) == TEE_PARAM_TYPE_MEMREF_INOUT)){
	}

	tee_time_get_sys_time(&t_start);

	/* handling multiple outputs */
	if(params->func == sumcount1_perkey_t ||
		params->func == sumcount_perkey_t ||
		params->func == sum_perkey_t ||
		params->func == avg_perkey_t){

		items_per_dest = (src_size / params->reclen) / n_outputs;
		rem = (src_size / params->reclen) % n_outputs;

		for(i = 0; i < n_outputs - 1; i++){
//			src_data += i * (items_per_dest * params->reclen);
			src_end	= src_data + (items_per_dest * params->reclen);
			switch(params->func){
				case sumcount1_perkey_t:
					xret = sumcount1_perkey(&outbuf[i], src_data, src_end, params);
					break;
				case sumcount_perkey_t:
					xret = sumcount_perkey(&outbuf[i], src_data, src_end, params);
					break;
				case sum_perkey_t:
					xret = sum_perkey(&outbuf[i], src_data, src_end, params);
					break;
				case avg_perkey_t:
					xret = avg_perkey(&outbuf[i], src_data, src_end, params);
					break;
				default:
					break;
			}
			src_data += (items_per_dest * params->reclen);
		}
		/* handling last output */
		/* last output should include the remained */
//		src_data += i * (items_per_dest * params->reclen);
		src_end	= src_data + ((items_per_dest + rem) * params->reclen);
		switch(params->func){
			case sumcount1_perkey_t:
				xret = sumcount1_perkey(&outbuf[i], src_data, src_end, params);
				break;
			case sumcount_perkey_t:
				xret = sumcount_perkey(&outbuf[i], src_data, src_end, params);
				break;
			case sum_perkey_t:
				xret = sum_perkey(&outbuf[i], src_data, src_end, params);
				break;
			case avg_perkey_t:
				xret = avg_perkey(&outbuf[i], src_data, src_end, params);
				break;
			default:
				break;
		}
	} else{
		switch(params->func){
			case sumcount1_all_t:
				xret = sumcount1_all(outbuf, src, params);
				break;
			case sumcount_all_t:
				xret = sumcount_all(outbuf, src, params);
				break;
			case avg_all_t:
				xret = avg_all(outbuf, src, params);
				break;
			default:
				break;
		}
	}

	tee_time_get_sys_time(&t_end);

	set_outbuf_size(p, (params->n_outputs) * sizeof(x_addr));
	x_returnA(p, xret);
	x_returnB(p, get_delta_time_in_ms(t_start, t_end));
	X2(BLUE "exec time : %u ms" RESET, get_delta_time_in_ms(t_start, t_end));

	for (i = 0; i < n_outputs; i++) {
		st = atomic_load(& (((batch_t*)outbuf[i])->state) );
		if (st != BATCH_STATE_OPEN) {
			EMSG("sumcount returns a batch which is not open. st %u", st);
			panic("bug");
		}
	}

//	BATCH_KILL(src);
	return TEE_SUCCESS;
}
