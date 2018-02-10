#include <stdlib.h>
#include <mm/batch.h>
#include <mm/sbuff.h>
#include "log.h"

#define X_SUCCESS			0

/* turn on if want to check batch overflow */
//#define CHECK_OVERFLOW

/* @ batch 		= target batch structure
 * @ cur_ptr 	= current write pointer of array
 * @ n 			= # of simd_t (size of next writing)
 */
void batch_check_overflow(struct batch *batch, simd_t **cur_ptr, int n){
	simd_t *start = batch->start;
	int32_t offset;

	if(*cur_ptr + n > start + batch->buf_size / sizeof(simd_t)) {
//		I("cur_ptr : %p, n = %d, start = %p, end = %p", *cur_ptr, n, start, start + SIZE_PER_BUNDLE);
		EE("batch overflow.. do realloc");
		offset = *cur_ptr - start;
		batch->buf_size *= 4;
		batch->start = realloc(start, batch->buf_size);
		*cur_ptr = batch->start + offset;
	}
}

struct batch *batch_init(void *addr) {
	struct batch *batch = (struct batch *)addr;
	OBJ_TYPE *data_addr = (OBJ_TYPE *)((uint64_t)addr + sizeof(struct batch));
	/* DMSG("start a new batch at %p, data %p\n", addr, (void *)data_addr); */
	/* sz_show_resource(); */
	/* batch_count++; */
	batch->start= data_addr;
	batch->end = data_addr;
	batch->size = 0;
	batch->next = NULL;
	batch->state = BATCH_STATE_OPEN;
	return batch;
}


//#define BUFF_SIZE_1M (1000000 * 3 * sizeof(simd_t))
//#define BUFF_SIZE_128M (128000000 * 3 * sizeof(simd_t))
struct batch *batch_new(__attribute__((unused)) uint32_t idx, uint32_t buf_size){
	struct batch *new_batch;
	simd_t *data;

	new_batch = (struct batch *) malloc (sizeof(struct batch));
	if(!new_batch){
		abort();
	}

#ifndef ARCH_ARM
//	mempool_stat_get(buf_size);
#endif

#ifdef USE_MEM_POOL

	int poolid = mempool_dispatch(buf_size);
	xzl_bug_on(poolid < 0);
	data = get_slow(poolid);
	new_batch->buf_size = get_pool_config(poolid).grain;

#if 0
	if(buf_size <= BUFF_SIZE_4K){
		data = get_slow_4k();
		new_batch->buf_size = BUFF_SIZE_4K;

	}else if (buf_size <= BUFF_SIZE_BUNDLE_KP_PART){
		data = get_slow_bundle_kp_part();
		new_batch->buf_size = BUFF_SIZE_BUNDLE_KP_PART;
	
	}else if (buf_size <= BUFF_SIZE_BUNDLE_KP){
		data = get_slow_bundle_kp();
		new_batch->buf_size = BUFF_SIZE_BUNDLE_KP;
	
	}else if (buf_size <= BUFF_SIZE_BUNDLE_RECORD){
		data = get_slow_bundle_record();
		new_batch->buf_size = BUFF_SIZE_BUNDLE_RECORD;
	
	}else if (buf_size <= BUFF_SIZE_WINDOW_KP){
		data = get_slow_window_kp();
		new_batch->buf_size = BUFF_SIZE_WINDOW_KP;
	
	}else if (buf_size <= BUFF_SIZE_WINDOW_RECORD){
		data = get_slow_window_record();
		new_batch->buf_size = BUFF_SIZE_WINDOW_RECORD;
	
	}else{
		EE("unsupported buffer size: %d\n", buf_size);	
		abort();
	}
#endif

/*
	if(buf_size <= BUFF_SIZE_1M){
		data = get_bundle_slow_1M();
		new_batch->buf_size = BUFF_SIZE_1M;
	}else if (buf_size <= BUFF_SIZE_128M){
		data = get_bundle_slow_128M();
		new_batch->buf_size = BUFF_SIZE_128M;
	}else{
		EE("unsupported buffer size: %d\n", buf_size);	
		abort();
	}
*/
#else
	/* allocate given size of buffer in batch struct */
	data = (simd_t *) malloc (buf_size);
	if(!data){
		abort();
	}
	new_batch->buf_size = buf_size;
#endif
	xzl_bug_on(!data);
	new_batch->start = data;
	//EE("batch_addr : %p, start : %p\n", new_batch, new_batch->start);
	new_batch->size = 0;
	new_batch->end = data;
   	new_batch->next = NULL;
   	new_batch->state = BATCH_STATE_OPEN;

   	return new_batch;
}

struct batch *batch_new_after(struct batch *batch, uint32_t idx){
	struct batch *new_batch;
	new_batch = batch_new(idx, batch->buf_size);
	return new_batch;
}

int32_t batch_close(struct batch *batch, void *end){
	batch->end = end;
    batch->state = BATCH_STATE_CLOSE;
    batch->size = ((simd_t *)end - (simd_t *)batch->start);
//    batch->size = ((simd_t *)end - (simd_t *)batch->start) / sizeof(simd_t);
	return X_SUCCESS;
}

void batch_update(struct batch *batch, void *end){
	batch->end = end;
    batch->size = ((simd_t *)end - (simd_t *)batch->start);
//    batch->size = ((simd_t *)end - (simd_t *)batch->start) / sizeof(simd_t);
//	EE("batch: %p, start: %p end: %p", batch, batch->start, end);
//	I("end - start : %d", (simd_t*)end - batch->start);
	/* if size > 2MB, print out error msg */
}

void batch_escape(uint32_t idx){

}


void BATCH_KILL(struct batch *batch, uint32_t idx) {
#ifdef USE_MEM_POOL
	if(batch) {

		mempool_stat_put(batch->buf_size);

		int poolid = mempool_dispatch(batch->buf_size);
		xzl_bug_on(poolid < 0);
		return_slow(poolid, batch->start);

#if 0
		if(batch->buf_size == BUFF_SIZE_4K){
			return_slow_4k(batch->start);
		
		}else if(batch->buf_size == BUFF_SIZE_BUNDLE_KP_PART){
			return_slow_bundle_kp_part(batch->start);
		
		}else if(batch->buf_size == BUFF_SIZE_BUNDLE_KP){
			return_slow_bundle_kp(batch->start);
		
		}else if(batch->buf_size == BUFF_SIZE_BUNDLE_RECORD){
			return_slow_bundle_record(batch->start);
		
		}else if(batch->buf_size == BUFF_SIZE_WINDOW_KP){
			return_slow_window_kp(batch->start);
		
		}else if(batch->buf_size == BUFF_SIZE_WINDOW_RECORD){
			return_slow_window_record(batch->start);
		
		}else{
			EE("Return a bundle with wrong buf_size: %d, batch is %ld", batch->buf_size, (x_addr)batch);
			abort();
		}
#endif
	}
/*
	if(batch){
		if(batch->buf_size == BUFF_SIZE_1M){
			return_bundle_slow_1M(batch->start);
		}else if(batch->buf_size == BUFF_SIZE_128M){
			return_bundle_slow_128M(batch->start);
		}else{
			EE("Return a bundle with wrong buf_size: %d, batch is %ld", batch->buf_size, (x_addr)batch);
			abort();
		}
	}
*/
#else
    if (batch) {
#ifndef ARCH_ARM
//	    	mempool_stat_put(batch->buf_size);
#endif
        free(batch->start);
		free(batch);
		}
#endif
	//if(batch){
	//	free(batch);
	//}
}
