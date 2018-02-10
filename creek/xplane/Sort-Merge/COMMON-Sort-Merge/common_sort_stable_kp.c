#include "log.h"
#include "xplane_lib.h"
#include "mm/batch.h"
#include <assert.h>

#include "WikiSort.h"

#define NUM_KEY		10
#define REC_NUM		30

typedef Test kv_t;

/* comapre function used for WikiSort */
bool kpCompare(kv_t item1, kv_t item2) {
	return (item1.k < item2.k);
}

/* packed key and pointer batch to kp batch
   do one part since caller might have multiple outputs (n_outputs)
 * @dest_kp	[OUT]: address of kp batch
 * @src_k	[IN] : address of source key batch
 * @src_p	[IN] : address of source pointer batch
 * @offset	[IN] : offset of source
 * @len		[IN] : length of source */
int32_t
pack_kp_one_part(x_addr *dest_kp, x_addr src_k, x_addr src_p,
		uint32_t offset, uint32_t len) {
	batch_t *batch_src_k	= (batch_t *) src_k;
	batch_t *batch_src_p	= (batch_t *) src_p;

	batch_t *batch_dest	= batch_new(0 /* unsued */, \
						len * sizeof(simd_t) * 2 /* <k,p> */);

	simd_t *data_src_k	= batch_src_k->start;
	simd_t *data_src_p	= batch_src_p->start;
	simd_t *data_dest	= batch_dest->start;

	uint64_t i;

	if (!src_k || !src_p) {
		EE("src is empty");
		return -1;
	}
	/* copy both key and pointer to the dest */
	for (i = offset; i < offset + len; i++) {
		*data_dest++ = data_src_k[i];
		*data_dest++ = data_src_p[i];
	}

	batch_update(batch_dest, data_dest);

	*dest_kp = (x_addr) batch_dest;
	return 0;

}

/* seperate kp batch to key batch and pointer batch 
 * @dest_k		[OUT]: address of seperated key batch
 * @dest_p		[OUT]: address of seperated pointer batch
 * @packed_kp	[IN] : address of key pointer packed batch */
int32_t
seperate_kp_one_part(x_addr *dest_k, x_addr *dest_p, x_addr packed_kp) {
	batch_t *batch_kp	= (batch_t *) packed_kp;
	batch_t *batch_k	= batch_new(0, batch_kp->size / 2 * sizeof(simd_t));
	batch_t *batch_p	= batch_new(0, batch_kp->size / 2 * sizeof(simd_t));
	uint64_t i;

	for (i = 0; i < batch_kp->size / 2 ; i++) {
		batch_k->start[i] = batch_kp->start[i * 2];
		batch_p->start[i] = batch_kp->start[i * 2 + 1];
	}

	batch_update(batch_k, batch_k->start + batch_kp->size / 2);
	batch_update(batch_p, batch_p->start + batch_kp->size / 2);

	*dest_k = (x_addr) batch_k;
	*dest_p = (x_addr) batch_p;
	return 0;
}

static int32_t
_sort_stable_kp(x_addr dest){
	batch_t *batch_dest = (batch_t *) dest;
	kv_t *input;

	input = (kv_t *) batch_dest->start;

	WikiSort(input, batch_dest->size / 2, kpCompare);

#if 0
	for (int i = 0; i < batch_dest->size / 2; i++) {
		I("key : %ld value : %ld\n", input[i].k, input[i].p);
	}
#endif
	return 0;
}


/* caller allocate space for dests_k and dests_p 
 * @dests_k		[OUT]: addresses of key batch
 * @dests_p		[OUT]: addresses of pointer batch
 * @n_outputs	[IN] : number of outputs
 * @src_k		[IN] : address of source key batch
 * @src_p		[IN} : address of source pointer batch  */
int32_t
sort_stable_kp(x_addr *dests_k, x_addr *dests_p, uint32_t n_outputs,
				x_addr src_k, x_addr src_p) {
	batch_t *batch_src_p	= (batch_t *) src_p;
	x_addr packed_kp = 0;

	/* first (n_outputs - 1) partition have the same len */
	uint32_t each_len = batch_src_p->size / n_outputs;
	/* last pasrtition may has different len with previous partitions' len */
	uint32_t last_len = batch_src_p->size - ((n_outputs - 1) * each_len);
	uint32_t offset = 0;
	uint64_t i = 0;

	/* sort first (n_outputs - 1) parts */
	for (i = 0; i < n_outputs - 1; i++) {
		pack_kp_one_part(&packed_kp, src_k, src_p, offset, each_len);
		_sort_stable_kp(packed_kp);
		seperate_kp_one_part(&dests_k[i], &dests_p[i], packed_kp);
		BATCH_KILL((batch_t *) packed_kp, 0);
		offset += each_len;
	}
	pack_kp_one_part(&packed_kp, src_k, src_p, offset, last_len);
	_sort_stable_kp(packed_kp);
	seperate_kp_one_part(&dests_k[i], &dests_p[i], packed_kp);
	BATCH_KILL((batch_t *) packed_kp, 0);
	
	return 0;
}



