#include "xplane_segment.h"

int32_t xplane_segment(batch_t *dest, batch_t *src, int32_t max){
	item_t *item = (item_t *)src->start;
	item_t *out = (item_t *)dest->start;

	while(SIZE_TO_ITEM(src->size) != 0){
		if(item->ts < max){
			memcpy(out++, item++, sizeof(item_t));
			src->start += ELES_PER_ITEM;
			src->size -= ELES_PER_ITEM;
		} else{
			break;
		}
	}
	batch_close(dest, out);
	return 1;
}

int32_t x1_xplane_segment(batch_t *dest, batch_t *src, int32_t size){
	item_t *item = (item_t *)src->start;
	item_t *out = (item_t *)dest->start;

	while(SIZE_TO_ITEM(src->size) != 0){
		if(size){
			memcpy(out++, item++, sizeof(item_t));
			src->start += ELES_PER_ITEM;
			src->size -= ELES_PER_ITEM;
			size -= ELES_PER_ITEM;
		} else{
			break;
		}
	}
	batch_close(dest, out);
	return 1;
}

TEE_Result xfunc_segment(uint32_t type, TEE_Param p[TEE_NUM_PARAMS])
{
	seg_arg *segarg 		= (seg_arg *)get_params(p);
	x_addr *outbuf			= (x_addr *)get_outbuf(p);
	batch_t *src_batch		= (batch_t *)segarg->src;
	xscalar_t *seg_bases	= (xscalar_t *)get_seg_bases(p);

	int32_t *batch_pos;
	int32_t *start_pos, *end_pos;

	batch_t *tmp_batch;
	x_addr map[64];

	batch_t *cur_batch;

	uint32_t base		= segarg->base;
	uint32_t new_base	= 0;
	uint32_t subrange 	= segarg->subrange;

	idx_t tspos = segarg->tspos;
	idx_t reclen = segarg->reclen;

	int32_t i;

	int32_t n_ref = 0, c_idx = 0;

	int32_t c, n;
	int32_t nrec = 0;
	int32_t n_segs = 0;


#if 0
	seg_arg *segarg;
	item_t *last_item;
	uint32_t n_segs;
	uint32_t rem;
#endif

#ifdef X_DEBUG
	/* segment uses tspos */
	if(pos_checker(reclen, tspos)){
		x_returnA(p, -X_ERR_POS);
		return TEE_SUCCESS;
	}
#endif

	X2("enter %s", __func__);

	if ((TEE_PARAM_TYPE_GET(type, 1) == TEE_PARAM_TYPE_MEMREF_INOUT) &&
			(TEE_PARAM_TYPE_GET(type, 2) == TEE_PARAM_TYPE_MEMREF_INOUT)){
	}
	/* designate shared buffer as store each 
			address of wnd (start batch of wnd) */
	/* maximum shared buffer size is 2M (default) */

#ifdef ONE_PER_ITEM
	/* just for testing */
	max = src->size / 8; /* # of thread */
	for(i = 0; i < 8; i++){
		tmp_batch = batch_new(X_CMD_SEGMENT);
		if(!tmp_batch){
			x_return(p, -X_ERR_NO_BATCH);
			return TEE_SUCCESS;
		}

		x1_xplane_segment(tmp_batch, src, max);
		*pos++ = (x_addr)tmp_batch;
	}
#else



	start_pos = src_batch->start;

	while((uint32_t)start_pos[tspos] < base) {
		start_pos += reclen;
	}

	if((void *)start_pos > src_batch->end){
		x_return(p, -X_ERR_SEG_BASE);
		return TEE_SUCCESS;
	}

	c = (start_pos[tspos] - base) / subrange;
//	c = (src_batch->start[tspos] - base) / subrange;
	/* in case of first item has high ts, need to sum up its ts */
	/* later sub_bases are calcualted based on batch idx */
	/* ex) idx * subrange */

	new_base += c * subrange;	/* seg new base */

	for(i = c - 2; i <= c + 2; i++){
		tmp_batch = batch_new(X_CMD_SEGMENT);
		if(!tmp_batch){
			x_return(p, -X_ERR_NO_BATCH);
			return TEE_SUCCESS;
		}
		map[n_ref++] = (x_addr)tmp_batch;
	}
	c_idx = 2;	/* maintain 5 segments */

	cur_batch = (batch_t *)map[c_idx];
	batch_pos = cur_batch->start;
	end_pos = start_pos;

	while((void *)end_pos < src_batch->end){
		/* which batch can be used for */
		/* need to select batch like tmp = ... */

		/*******************************/
		n = (end_pos[tspos] - base) / subrange;
		if(c != n){
			/* copy previous ones */
			memcpy(batch_pos, start_pos,
					sizeof(int32_t) * reclen * nrec);

			batch_pos += reclen * nrec;
			start_pos += reclen * nrec;

			batch_update(cur_batch, batch_pos);

			if(c < n && n_ref < n + 5){
				for(i = 0; i < (n - c); i++){
					tmp_batch = batch_new(X_CMD_SEGMENT);
					if(!tmp_batch){
						x_return(p, -X_ERR_NO_BATCH);
						return TEE_SUCCESS;
					}
					map[n_ref++] = (x_addr)tmp_batch;
				}
				/* need to close previous batch?? */

			}

			c_idx += (n - c);	/* current map idx */
			c = n;	/* seg_id */
			cur_batch = (batch_t *)map[c_idx];
			batch_pos = cur_batch->end;

			nrec = 0;
		}
		nrec++;
		end_pos += reclen;
	}
	/* copy last segment */
	memcpy(batch_pos, start_pos,
			sizeof(int32_t) * reclen * nrec);

	batch_pos += reclen * nrec;
	batch_update(cur_batch, batch_pos);

	for(i = 0; i < n_ref; i++){
		cur_batch = (batch_t *)map[i];
		if(cur_batch->size){
			n_segs++;
//			batch_close(cur_batch, cur_batch->end);
			batch_update(cur_batch, cur_batch->end);
			*outbuf++ = (x_addr)cur_batch;
			batch_escape(X_CMD_SEGMENT);

			*seg_bases++ = base + new_base + (i - 2) * subrange;

		} else{
			X1("Kill emtpy batch[%p]", (void *)cur_batch);
			BATCH_KILL(cur_batch, X_CMD_SEGMENT);
		}
	}

	set_outbuf_size(p, (outbuf - (x_addr *)p[1].memref.buffer) * sizeof(x_addr));
	set_seg_bases_size(p, (seg_bases - (xscalar_t *)p[3].memref.buffer) * sizeof(xscalar_t));
	segarg->n_segs = n_segs;

#endif
	x_return(p, X_SUCCESS);
	return TEE_SUCCESS;

}
