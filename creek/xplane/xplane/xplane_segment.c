#include <log.h>
#include "xplane/params.h"
#include "xplane-internal.h"
#include "xplane/xplane_segment.h"

#define TEMP_SEG_NUM		100
#define RESERVED_SEG_NUM	2

TEE_Result xfunc_segment(TEE_Param p[TEE_NUM_PARAMS])
{
	seg_arg *segarg 		= (seg_arg *)get_params(p);
	x_addr *outbuf			= (x_addr *)get_outbuf(p);
	batch_t *src_batch		= (batch_t *)segarg->src;
	xscalar_t *seg_bases	= (xscalar_t *)get_seg_bases(p);

	simd_t *batch_pos;
	simd_t *start_pos, *end_pos;

	batch_t *tmp_batch;
	x_addr map[64];

	batch_t *cur_batch;

	simd_t base		= segarg->base;
	simd_t new_base	= 0;
	simd_t subrange 	= segarg->subrange;

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

	DD("enter %s\n", __func__);

	/* designate shared buffer as store each 
			address of wnd (start batch of wnd) */
	/* maximum shared buffer size is 2M (default) */

#if 0
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

	while(start_pos[tspos] < base) {
		start_pos += reclen;
	}

	if(start_pos > src_batch->end){
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
		tmp_batch = batch_new(X_CMD_SEGMENT, src_batch->size * sizeof(simd_t));
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
#ifdef __x86_64
	DD("src_batch : %p end : %p size : %ld\n",
#else
	DD("src_batch : %p end : %p size : %d\n",
#endif
			(void *) src_batch, src_batch->end, src_batch->size);

	/*
	DD(RED "src_batch->start : %p\n" RESET, (void *) src_batch->start);
	DD(RED "end_pos : %p\n" RESET, (void *) end_pos);
	DD("%d\n", (simd_t *)src_batch->end - src_batch->start);
	DD(RED "batch_pos : %p\n" RESET, (void *) batch_pos);
	*/

	while(end_pos < src_batch->end){
		/* which batch can be used for */
		/* need to select batch like tmp = ... */

		/*******************************/
		n = (end_pos[tspos] - base) / subrange;
		if(c != n){
#ifdef CHECK_OVERFLOW
			batch_check_overflow(cur_batch, &batch_pos, reclen * nrec);
#endif
			/* copy previous ones */
			memcpy(batch_pos, start_pos,
					sizeof(simd_t) * reclen * nrec);
//			DD("batch_pos : %p, start_pos : %p\n", batch_pos, start_pos);
//			DD("nrec : %d\n", nrec);
			batch_pos += reclen * nrec;
			start_pos += reclen * nrec;

			batch_update(cur_batch, batch_pos);

			if(c < n && n_ref < n + 5){
				for(i = 0; i < (n - c); i++){
					tmp_batch = batch_new(X_CMD_SEGMENT, src_batch->size * reclen * sizeof(simd_t));
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

#ifdef CHECK_OVERFLOW
	batch_check_overflow(cur_batch, &batch_pos, reclen * nrec);
#endif
	/* copy last segment */
	memcpy(batch_pos, start_pos,
			sizeof(simd_t) * reclen * nrec);

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

struct node {
	batch_t *cur_batch;
	simd_t seg_id;
	struct node *prev;
	struct node *next;
};

int32_t
insert_node(struct node **node, batch_t *cur_batch, simd_t seg_id){
	struct node *cur_node = *node;
	struct node *new_node = (struct node *) malloc (sizeof(struct node));
//	I("batch : %p\n", (void *) cur_batch);
	new_node->next = NULL;
	new_node->prev = cur_node;
	new_node->seg_id = seg_id;
	new_node->cur_batch = cur_batch;
//	I("cur_batch : %p\n", (void *) new_node->cur_batch);

	cur_node->next = new_node;

	return 0;
}

/* Get source batch of recordS And make them multiple segments according to timestamp
 * Caller must free @dests_p and @seg_bases after using it
 *
 * @dests_p		[OUT] : multiple batch addresses produced consisting of record pointer
 * @seg_bases	[OUT] : each segment's base timestamp
 * @n_segs		[OUT] : # of segs produced by this functions
 *
 * @src			[IN] : address of src (full records)
 * @base		[IN] : base timestamp for segment
 * @subrange	[IN] : ts subrange to partition the @src
 * @tspos		[IN] : timestamp position
 * @reclen		[IN] : record length */
int32_t
segment_kp(x_addr **dests_p, simd_t **seg_bases, uint32_t *n_segs,
		x_addr src, simd_t base, simd_t subrange, idx_t tspos, idx_t reclen) {
	*dests_p = (x_addr *) malloc (sizeof(x_addr) * TEMP_SEG_NUM);
	*seg_bases = (simd_t *) malloc (sizeof(simd_t) * TEMP_SEG_NUM);

	batch_t *batch_tmp;
	batch_t *batch_src	= (batch_t *) src;

	simd_t *start_pos	= batch_src->start;
	simd_t *end_pos;
	simd_t *dest_pos;

	uint64_t cur_seg_id, next_seg_id;
	uint64_t i;
	uint32_t nsegs = 0;

	/* skip records with smaller ts than base */
	while (start_pos[tspos] < base) {
		start_pos += reclen;
	}

	if (start_pos > batch_src->end) {
		EE("no recrod has ts smaller than base");
		return -1;
	}

	cur_seg_id = (start_pos[tspos] - base) / subrange;
	I("start seg_id : %ld, ts : %d\n", cur_seg_id, start_pos[tspos]);

	struct node *cur_node = (struct node *) malloc (sizeof(struct node));
	struct node *start_node = cur_node;
	batch_tmp = batch_new(X_CMD_SEGMENT, batch_src->size * reclen * sizeof(simd_t));
	cur_node->prev = NULL;
	cur_node->next = NULL;
	cur_node->seg_id = cur_seg_id - RESERVED_SEG_NUM;
	cur_node->cur_batch = batch_tmp;

	for (i = 0; i < RESERVED_SEG_NUM; i++) {
		batch_tmp = batch_new(X_CMD_SEGMENT, batch_src->size * reclen * sizeof(simd_t));
		if (!batch_tmp) {
			EE("batch_new fails");
			return -1;
		}
		insert_node(&cur_node, batch_tmp, cur_node->seg_id + 1);

		cur_node = cur_node->next;
		I("cur_node-> seg_id: %ld\n", cur_node->seg_id);
	}

	end_pos = start_pos;
	dest_pos = cur_node->cur_batch->end;

	while (end_pos < batch_src->end) {
		next_seg_id = (end_pos[tspos] - base) / subrange;
		if (cur_seg_id != next_seg_id) {
			/* copy corresponding pointers belonging to this segment  */
			while (start_pos < end_pos) {
				*dest_pos++	= (x_addr) start_pos;
				start_pos	+= reclen;
			}
			batch_update(cur_node->cur_batch, dest_pos);

			/* change segment */
			if (cur_seg_id < next_seg_id) {
				for (i = 0; i < next_seg_id - cur_seg_id; i++) {
					if (cur_node->next) {
						cur_node = cur_node->next;
					} else {
						batch_tmp = batch_new(X_CMD_SEGMENT, batch_src->size * reclen * sizeof(simd_t));
						insert_node(&cur_node, batch_tmp, cur_seg_id + i + 1);
						cur_node = cur_node->next;
					}
				}
			} else {
				for (i = 0; i < cur_seg_id - next_seg_id; i++) {
					cur_node = cur_node->prev;
					if (!cur_node) {
						EE("reserved segments are not enough");
						abort();
					}
				}
			}
			cur_seg_id = next_seg_id;
			dest_pos = cur_node->cur_batch->end;
		}
		end_pos += reclen;
	}

	/* copy last part into seg  */
	I("start_pos : %ld, end_pos : %ld\n", start_pos, end_pos);
	while (start_pos < end_pos) {
		*dest_pos++	= (x_addr) start_pos;
		start_pos	+= reclen;
	}
	batch_update(cur_node->cur_batch, dest_pos);

	cur_node = start_node;
	struct node *next_node;
	uint32_t cnt = 0;
	do {
		batch_tmp = cur_node->cur_batch;
		if (batch_tmp->size) {
			I("seg_id : %ld\n", cur_node->seg_id);
			I("cnt : %d, included batch : %ld\n", cnt, (x_addr) batch_tmp);
			(*dests_p)[nsegs] = (x_addr) batch_tmp;
			(*seg_bases)[nsegs] = base + cur_node->seg_id * subrange;
//			(*seg_bases)[nsegs] = base + (cnt - RESERVED_SEG_NUM) * subrange;
			nsegs++;
		} else {
			I("Kill emtpy batch[%p]\n", (void *)batch_tmp);
			BATCH_KILL(batch_tmp, X_CMD_SEGMENT);
		}
		cnt++;
		next_node = cur_node->next;
		free(cur_node);
		cur_node = next_node;
	} while(cur_node);

	*n_segs = nsegs;
	return 0;
}
