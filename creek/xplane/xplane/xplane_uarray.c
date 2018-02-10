#include "xplane/xplane_uarray.h"
#include "xplane/utils.h"
#include "xplane/params.h"

#include "log.h"

#include <assert.h>
#include <string.h>
//#include <tee_api.h>


int32_t get_uarray_size(x_params *params){
	batch_t *src_batch = (batch_t *)params->srcA;

	XMSG(YELLOW "invoke get_uarray_size" RESET);
	if(!params->srcA) 
		return -X_ERR_NO_XADDR;
	else 
		return src_batch->size;
}

TEE_Result xfunc_uarray_debug(TEE_Param p[TEE_NUM_PARAMS]){
	x_params *params 	= (x_params *)get_params(p);
//	void *outbuf		= get_outbuf(p);
	int32_t ret = 0;

//	XMSG(YELLOW "invke uarray_debug" RESET);
/*
	if ((TEE_PARAM_TYPE_GET(type, IDX_SRC) != TEE_PARAM_TYPE_MEMREF_INOUT)){

	}
*/
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

TEE_Result xfunc_concatenate(TEE_Param p[TEE_NUM_PARAMS]){
	x_params *params 	= (x_params *)get_params(p);
	batch_t *src 		= (batch_t *)params->srcA;
	batch_t *dest		= (batch_t *)params->srcB;

	XMSG(YELLOW "invke concatenate" RESET);

	if(!dest || !src){
		x_return(p, -X_ERR_NO_XADDR);
		return TEE_SUCCESS;
	}

	XMSG("dest : %p, src : %p", (void *) dest, (void *)src);
	XMSG("dest->size : %d, src->size : %d", dest->size, src->size);

	/* if dest buf isn't large enough, need new allocation */
	if(dest->buf_size - (dest->size * sizeof(simd_t)) < src->size * sizeof(simd_t)){
		simd_t *start = dest->start;
		dest->buf_size = (dest->size + src->size) * sizeof(simd_t);	/* new buf size */
		dest->start = realloc(start, dest->buf_size);
		dest->end = dest->start + dest->size;
//		EE("[concatenate] dest doesn't have enough space");
//		exit(1);
#if 0
		/* allocate new batch buf to concatenate both */
		batch_t new_batch = batch_new(X_CMD_UA_CREATE, (src->buf_size + dest->buf_size) * sizeof(simd_t));
		/* copy both two bufs to new batch buf*/
		memcpy(new_batch->end,
				dest->start, sizeof(simd_t) * dest->size);
		memcpy(new_batch->end + dest->size,
				src->start, sizeof(simd_t) * src->size);
		batch_update(new_batch, new_batch->end + dest->size + src->size);

		/* change return addr to new_batch*/
		params->srcB = (void *) new_batch;
		x_return(p, X_SUCCESS);
		return TEE_SUCCESS;
#endif
	}

#ifdef CHECK_OVERFLOW
	batch_check_overflow(dest, (simd_t **)&dest->end, src->size);
#endif
	memcpy(dest->end, src->start, sizeof(simd_t) * src->size);

	batch_update(dest, (simd_t *)dest->end + src->size);
	x_return(p, X_SUCCESS);
	return TEE_SUCCESS;
}

TEE_Result xfunc_uarray_to_nsbuf(TEE_Param p[TEE_NUM_PARAMS]){
	x_params *params 	= (x_params *)get_params(p);
	simd_t *outbuf 		= (simd_t *)get_outbuf(p);
	batch_t *src 		= (batch_t *)params->srcA;
	uint32_t len 		= params->n_outputs;	/* size of len */

	XMSG(YELLOW "invoke uarray_to_nsbuf" RESET);
/*
	if ((TEE_PARAM_TYPE_GET(type, IDX_SRC) != TEE_PARAM_TYPE_MEMREF_INOUT)){

	}
*/
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


TEE_Result xfunc_pseudo_source(TEE_Param p[TEE_NUM_PARAMS]){
	source_params *params	= (source_params *)get_params(p);;
	batch_t *src;
	batch_t *dest;

	x_addr *outbuf			= (x_addr *)get_outbuf(p);

	uint8_t reclen = params->reclen;;
	simd_t *src_data_pos, *dest_data_pos;
	uint32_t i;
	uint32_t cnt = 0;

	XMSG(YELLOW "invoke pseudo_source" RESET);

	src = (batch_t *)params->src;
	DD("global src addr : 0x%lx", params->src);
#ifdef __x86_64
	DD("src->start : %p size : %ld", (void *)src->start, src->size);
#else
	DD("src->start : %p size : %d", (void *)src->start, src->size);
#endif

	/* allocate batch with the same size of input bundle */
	dest = batch_new(X_CMD_UA_PSEUDO, params->count * reclen * sizeof(simd_t));

	if(!dest){
		x_return(p, -X_ERR_NO_BATCH);
		return TEE_SUCCESS;
	}

	src_data_pos	= src->start + ((params->src_offset * reclen)  % src->size);

	dest_data_pos 	= dest->start;

	while(cnt < params->count * reclen){
#ifdef CHECK_OVERFLOW
		batch_check_overflow(dest, &dest_data_pos, 1);
#endif
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


//	assert(atomic_load(&dest->state) == BATCH_STATE_OPEN);

	batch_close(dest, dest_data_pos);
	batch_escape(X_CMD_UA_PSEUDO);

	*outbuf++ = (x_addr) dest;
	set_outbuf_size(p, (outbuf - (x_addr *)p[1].memref.buffer) * sizeof(x_addr));
#ifdef __x86_64
	DD("size of dest : %ld, params->count %d", dest->size, params->count);
#else
	DD("size of dest : %d, params->count %d", dest->size, params->count);
#endif
	x_returnA(p, X_SUCCESS);
	return TEE_SUCCESS;
}


TEE_Result xfunc_retire_uarray(TEE_Param p[TEE_NUM_PARAMS]){
	batch_t *src;
	x_params *params;

	XMSG(YELLOW "invoke retire" RESET);
/*
	if ((TEE_PARAM_TYPE_GET(type, IDX_SRC) != TEE_PARAM_TYPE_MEMREF_INOUT)){

	}
*/

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

TEE_Result xfunc_create_uarray(TEE_Param p[TEE_NUM_PARAMS]){
	x_addr *pos;
	batch_t *new_batch;

	pos = (x_addr *)get_outbuf(p);

	/* just simply allocate deafult size (i.e. 64 MB) */
	new_batch = batch_new(X_CMD_UA_CREATE, (64 << 20));
	if(!new_batch){
		x_return(p, -X_ERR_NO_BATCH);
		return TEE_SUCCESS;
	}

	*pos++ = (x_addr) new_batch;
	p[1].memref.size = (pos - (x_addr *)p[1].memref.buffer) * sizeof(x_addr);
	return TEE_SUCCESS;
}

TEE_Result xfunc_create_uarray_rand(TEE_Param p[TEE_NUM_PARAMS]){
	x_addr *pos;
	batch_t *new_batch;
	simd_t *data;
	uint32_t i;

	XMSG("invoke create uarray rand");

	pos = (x_addr *)get_outbuf(p);

	/* just simply allocate deafult size (i.e. 64 MB) */
	new_batch = batch_new(X_CMD_UA_RAND, (64 << 20));
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

	XMSG("create new urray rand ref : %p size : %d", (void *)new_batch, SIZE_TO_ITEM(new_batch->size, reclen));

	*pos++ = (x_addr) new_batch;
	p[1].memref.size = (pos - (x_addr *)p[1].memref.buffer) * sizeof(x_addr);
	return TEE_SUCCESS;
}

/* get nsbuf and put data into array on batch */
TEE_Result xfunc_create_uarray_nsbuf(TEE_Param p[TEE_NUM_PARAMS]){
	x_addr *pos;
	batch_t *new_batch;
	simd_t *data;
	uint32_t i;
	simd_t *in;
	uint32_t size;
//	int32_t state;

	XMSG(YELLOW "invoke ua nsbuf" RESET);
	DD("size of buffer : %d", p[IDX_SRC].memref.size);
//	EE(GREEN "state : %d" RESET, state);

	in = (simd_t *) p[IDX_SRC].memref.buffer;
	size = p[IDX_SRC].memref.size;


	pos = (x_addr *)get_outbuf(p);

	new_batch = batch_new(X_CMD_UA_NSBUF, size);

	DD("new_batch->start : %p", new_batch->start);
	if(!new_batch){
		x_return(p, -X_ERR_NO_BATCH);
		return TEE_SUCCESS;
	}

	data = new_batch->start;

	VV("size %lu data %p -- %p", size, data, (char *)data + size);
	for(i = 0; i < size / sizeof(simd_t); i++)
		*(data++) = *(in++);

	batch_close(new_batch, data);
#ifdef __x86_64
	DD("created new urray file ref : %p nitems : %ld", \
		(void *)new_batch, new_batch->size);
#else
	DD("created new urray file ref : %p nitems : %d", \
		(void *)new_batch, new_batch->size);
#endif

	*pos++ = (x_addr) new_batch;
	p[1].memref.size = (pos - (x_addr *)p[1].memref.buffer) * sizeof(x_addr);

	return TEE_SUCCESS;
}

