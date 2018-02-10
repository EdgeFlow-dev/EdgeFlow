#include "xplane_uarray.h"
#include "utils.h"
#include "params.h"

#include <assert.h>
#include <string.h>
#include <tee_api.h>

extern size_t get_core_pos(void);

int32_t get_uarray_size(x_params *params){
	batch_t *src_batch = (batch_t *)params->srcA;

	XMSG(YELLOW "invoke get_uarray_size" RESET);
	if(!params->srcA) 
		return -X_ERR_NO_XADDR;
	else 
		return src_batch->size;
}

TEE_Result xfunc_uarray_debug(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]){
	x_params *params 	= (x_params *)get_params(p);
//	void *outbuf		= get_outbuf(p);
	int32_t ret = 0;

//	XMSG(YELLOW "invke uarray_debug" RESET);
	if ((TEE_PARAM_TYPE_GET(type, IDX_SRC) != TEE_PARAM_TYPE_MEMREF_INOUT)){
		/* error */
	}

	switch(params->func){
		case uarray_size_t :
			ret = get_uarray_size(params);
		break;
		default:
		break;
	}
	x_return(p, ret);
	return TEE_SUCCESS;
}

TEE_Result xfunc_concatenate(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]){
	x_params *params 	= (x_params *)get_params(p);
	batch_t *src 		= (batch_t *)params->srcA;
	batch_t *dest		= (batch_t *)params->srcB;

	XMSG(YELLOW "invke concatenate" RESET);
	if ((TEE_PARAM_TYPE_GET(type, IDX_SRC) != TEE_PARAM_TYPE_MEMREF_INOUT)){
		/* error */
	}

	if(!dest || !src){
		x_return(p, -X_ERR_NO_XADDR);
		return TEE_SUCCESS;
	}

	if(atomic_load(&dest->state) != BATCH_STATE_OPEN){
		XMSG(RED "cannot access the dest, its state is \"closed\"" RESET);
		x_return(p, -X_ERR_BATCH_CLOSED);
		return TEE_SUCCESS;
	}
	
	XMSG("dest : %p, src : %p", (void *) dest, (void *)src);
	XMSG("dest->size : %d, src->size : %d", dest->size, src->size);
	memcpy(dest->end, src->start, sizeof(int32_t) * src->size);

	batch_update(dest, (int32_t *)dest->end + src->size);
	XMSG("concatenate success");
	XMSG(RED "now, dest->size : %d" RESET, dest->size);
	return TEE_SUCCESS;
}

TEE_Result xfunc_uarray_to_nsbuf(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]){
	x_params *params 	= (x_params *)get_params(p);
	simd_t *outbuf 		= (simd_t *)get_outbuf(p);
	batch_t *src 		= (batch_t *)params->srcA;
	uint32_t len 		= params->n_outputs;	/* size of len */

	XMSG(YELLOW "invoke uarray_to_nsbuf" RESET);
	if ((TEE_PARAM_TYPE_GET(type, IDX_SRC) != TEE_PARAM_TYPE_MEMREF_INOUT)){
		/* error */
	}

	if(sizeof(simd_t) * len > 0x1 << 20)
		len = 0x1 << 20 / sizeof(simd_t);

	if(src->size < len){
		XMSG("src size is smaller than len");
		len = src->size;
	}

	memcpy(outbuf, src->start, sizeof(simd_t) * len);

	/* return values */
	params->n_outputs = len;
	set_outbuf_size(p, sizeof(simd_t) * len);
	x_return(p, X_SUCCESS);
	return TEE_SUCCESS;
}


TEE_Result xfunc_pseudo_source(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]){
	source_params *params	= (source_params *)get_params(p);;
	batch_t *src;
	batch_t *dest;

	x_addr *outbuf			= (x_addr *)get_outbuf(p);

	uint8_t reclen = params->reclen;;
	int32_t *src_data_pos, *dest_data_pos;
	uint32_t i;
	uint32_t cnt = 0;

	XMSG(YELLOW "invoke pseudo_source" RESET);
	if ((TEE_PARAM_TYPE_GET(type, IDX_SRC) != TEE_PARAM_TYPE_MEMREF_INOUT)){
		/* error */
	}

	src = (batch_t *)params->src;
//	EMSG("global src addr : 0x%lx", params->src);
//	EMSG("src->start : %p size : %d", (void *)src->start, src->size);
	dest = batch_new(X_CMD_UA_PSEUDO);
	if(!dest){
		x_return(p, -X_ERR_NO_BATCH);
		return TEE_SUCCESS;
	}

	src_data_pos	= src->start + ((params->src_offset * reclen)  % src->size);

	dest_data_pos 	= dest->start;

	while(cnt < params->count * params->reclen){
		*(dest_data_pos++) = *(src_data_pos++);
		cnt++;
		if(src_data_pos == src->end)
			src_data_pos = src->start;
	}
	/* adding ts phase */
	if(reclen != 1){
		for(i = 0; i < params->count; i++){
			dest->start[reclen * i + params->tspos] = params->ts_start + (params->ts_delta * i);
		}
	}

//	XMSG("dest_data_pos = %p, dest->start pos : %p, %lu", 
//		(void *)dest_data_pos, (void *)dest->start, 
//		(dest_data_pos - dest->start) / sizeof(int));


	assert(atomic_load(&dest->state) == BATCH_STATE_OPEN);

	batch_close(dest, dest_data_pos);
	batch_escape(X_CMD_UA_PSEUDO);

	*outbuf++ = (x_addr) dest;
	set_outbuf_size(p, (outbuf - (x_addr *)p[1].memref.buffer) * sizeof(x_addr));

	XMSG(RED "size of dest : %d, params->count %d" RESET, dest->size, params->count);
	return TEE_SUCCESS;
}


TEE_Result xfunc_retire_uarray(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]){
	batch_t *src;
	x_params *params;

	XMSG(YELLOW "invoke retire" RESET);
	if ((TEE_PARAM_TYPE_GET(type, IDX_SRC) != TEE_PARAM_TYPE_MEMREF_INOUT)){
		/* error */
	}

	params = (x_params *)get_params(p);
	src = (batch_t *)params->srcA;

	/* consider whether it should be killed or not */
	/* handle if batch is opened */
//	if(src->state == BATCH_STATE_OPEN)
//		batch_close(src, src->end);

	X1("batch[%p] kill by retire from normal", (void *)src);
	BATCH_KILL(src, X_CMD_UA_RETIRE);

	x_return(p, X_SUCCESS);
	return TEE_SUCCESS;
}

TEE_Result xfunc_create_uarray(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]){
	x_addr *pos;
	batch_t *new_batch;

	if ((TEE_PARAM_TYPE_GET(type, IDX_OUTBUF) != TEE_PARAM_TYPE_MEMREF_INOUT)){
		/* error */
	}
	
	pos = (x_addr *)get_outbuf(p);
	new_batch = batch_new(X_CMD_UA_CREATE);
	if(!new_batch){
		x_return(p, -X_ERR_NO_BATCH);
		return TEE_SUCCESS;
	}

	*pos++ = (x_addr) new_batch;
	p[1].memref.size = (pos - (x_addr *)p[1].memref.buffer) * sizeof(x_addr);
	return TEE_SUCCESS;
}

TEE_Result xfunc_create_uarray_rand(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]){
	x_addr *pos;
	batch_t *new_batch;
	int32_t *data;
	uint32_t i;

	XMSG("invoke create uarray rand");

	if ((TEE_PARAM_TYPE_GET(type, IDX_OUTBUF) != TEE_PARAM_TYPE_MEMREF_INOUT)){
		/* error */
	}

	pos = (x_addr *)get_outbuf(p);
	
	new_batch = batch_new(X_CMD_UA_RAND);
	if(!new_batch){
		x_return(p, -X_ERR_NO_BATCH);
		return TEE_SUCCESS;
	}

	data = new_batch->start;
	for(i = 0; i < N_TUPLES; i++){
#ifdef	ONE_PER_ITEM
		*data++ = ((rand() % 10000) + 1000);	// key
#endif
#ifdef	THREE_PER_ITEM
		*data++ = ((rand() % 10000) + 1000);	// key
		*data++ = rand() % 10;					// value
		*data++	= i;							// ts
#endif
#ifdef	FOUR_PER_ITEM
		*data++ = ((rand() % 10000) + 1000);	// key
		*data++ = ((rand() % 10000) + 1000);	// key
		*data++ = rand() % 10;					// value
		*data++	= i;							// ts
#endif
	}

	batch_close(new_batch, data);
	
	XMSG("create new urray rand ref : %p size : %d", (void *)new_batch, SIZE_TO_ITEM(new_batch->size));

	*pos++ = (x_addr) new_batch;
	p[1].memref.size = (pos - (x_addr *)p[1].memref.buffer) * sizeof(x_addr);
	return TEE_SUCCESS;
}

TEE_Result xfunc_create_uarray_nsbuf(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]){
	x_addr *pos;
	batch_t *new_batch;
	int32_t *data;
	uint32_t i;
	int32_t *in;
	uint32_t size;
	int32_t state;

//	TEE_ObjectHandle *obj = NULL;
//	TEE_Result res;

	XMSG(YELLOW "invoke ua nsbuf" RESET);

	if ((TEE_PARAM_TYPE_GET(type, IDX_OUTBUF) != TEE_PARAM_TYPE_MEMREF_INOUT)){
		/* error */
	}
	in = (int32_t *) p[IDX_SRC].memref.buffer;
//	XMSG("size of buffer : %d", p[IDX_SRC].memref.size);

	state = p[0].value.a;
	size = p[IDX_SRC].memref.size;
//	XMSG(GREEN "state : %d" RESET, state);

	pos = (x_addr *)get_outbuf(p);

	switch(state){
		case BUF_NORMAL :
		case BUF_START :
			new_batch = batch_new(X_CMD_UA_NSBUF);
			if(!new_batch){
				x_return(p, -X_ERR_NO_BATCH);
				return TEE_SUCCESS;
			}

			data = new_batch->start;
			break;
		case BUF_CONTINUE : case BUF_END : default :
			x_return(p, -X_ERR_NO_BATCH);
			return TEE_SUCCESS;
			break;
	}

	for(i = 0; i < size / sizeof(int32_t); i++)
		*(data++) = *(in++);

	switch(state){
		case BUF_NORMAL :
		case BUF_END :
            batch_update(new_batch, data);
//			batch_close(new_batch, data); // because we want to append to the buffer later
			XMSG(GREEN "created new urray file ref : %p nitems : %d" RESET, \
				(void *)new_batch, new_batch->size);

			*pos++ = (x_addr) new_batch;
			p[1].memref.size = (pos - (x_addr *)p[1].memref.buffer) * sizeof(x_addr);
			break;
		case BUF_START : case BUF_CONTINUE : default :
			break;
	}
	return TEE_SUCCESS;
}

