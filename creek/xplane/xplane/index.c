//
// Created by xzl on 12/11/17.
//

#include <string.h>
#include <immintrin.h> /* AVX intrinsics */

#include "log.h"
#include "mm/batch.h"
#include "xplane_lib.h"

int32_t create_index(x_addr *dest, x_addr src, idx_t keypos, idx_t reclen)
{
	batch_t *bsrc = (batch_t *) src;
	simd_t len_src = bsrc->size / reclen;

	if (bsrc->size >= SIMDT_MAX / reclen)
		return X_ERR_WRONG_PARAMETER; /* index in a batch too large to be encoded in simd_t */

	struct batch *bdest = batch_new(1 /* ? */, bsrc->size);
	simd_t *s = bsrc->start;
	simd_t *d = bdest->start;

	for (simd_t i = 0; i < len_src; i++) {
		/* copy key over. reclen for d is 2 ( <key,idx> ) */
		d[i * 2] = s[i * reclen + keypos];
		d[i * 2 + 1] = i;
	}

	batch_update((struct batch *)bdest, d + len_src * 2);

	*dest = (x_addr)bdest;

	return 0;
}


int32_t shuffle_index(x_addr *dest, x_addr src, x_addr idx, idx_t reclen)
{
	batch_t *bsrc = (batch_t *) src;
	simd_t len_src = bsrc->size / reclen;

	batch_t *bidx = (batch_t *) idx;
	xzl_bug_on(len_src != bidx->size / reclen);

	struct batch * bdest = batch_new(1 /* ? */, bsrc->size);

	simd_t * s = bsrc->start;
	simd_t * i = bidx->start;
	simd_t * d = bdest->start;

	for (simd_t x = 0; x < len_src; x++) {
		simd_t index = i[x * 2 + 1];
		for (simd_t y = 0u; y < reclen; y++) {
			xzl_assert(index * reclen + y < bsrc->size);
			d[x * reclen + y] = s[index * reclen + y];
		}
	}

	batch_update((struct batch *)bdest, d + len_src * reclen);

	*dest = (x_addr)bdest;

	return 0; } /*
 * Taking a batch of records, produce a batch of keys and a batch of pointers
 * @src: the input batch of records
 * @dest_k: the output batch of keys
 * @dest_p: the output batch of pointers
 *
 * unsupported if size(simd_t) < pointer size
 */
int32_t extract_kp(x_addr *dest_k, x_addr *dest_p, x_addr src, idx_t keypos, idx_t reclen)
{
	if(src == 0){
		*dest_k = 0;
		*dest_p = 0;
		return 0;
	}
	/* hp: size of each dest of p and k is nitems = batch_src->size * sizeof(simd_t) */
	batch_t *batch_src 		= (batch_t *) src;
	batch_t *batch_dest_k 	= batch_new(1 /* unsued */, batch_src->size * sizeof(simd_t) / reclen);
	batch_t *batch_dest_p	= batch_new(1 /* unsued */, batch_src->size * sizeof(simd_t) / reclen);

	simd_t *data_src 	= batch_src->start;
	simd_t *data_dest_k = batch_dest_k->start;
	simd_t *data_dest_p = batch_dest_p->start;

	uint32_t i;

	/* size checker */
	xzl_bug_on_msg(sizeof(simd_t) < sizeof(x_addr),
	"size of simd_t is smaller than x_addr");

	for (i = 0; i < batch_src->size / reclen; i++) {
		*data_dest_k++ = data_src[i * reclen + keypos];
		*data_dest_p++ = (simd_t) &data_src[i * reclen];
	}

	batch_update(batch_dest_k, data_dest_k);
	batch_update(batch_dest_p, data_dest_p);

	*dest_k = (x_addr) batch_dest_k;
	*dest_p = (x_addr) batch_dest_p;

	return 0;
}

#ifdef KNL
#include "immintrin.h"
#endif

/* Taking a batch of record pointers, produce a batch of keys that extracted from a specific
 * field (column) of each record.
 *
 * @src_p: the input batch of pointers
 * @dest: the output batch of keys
 *
 * @reclen, @keypos: ditto
 * return value: ditto
 *
 * unsupported if size(simd_t) < pointer size
 */
int32_t deref_k(x_addr *dest, x_addr src_p, idx_t keypos, idx_t reclen) {
	if(src_p == 0){
		*dest = 0;
		return 0;
	}

	batch_t *batch_src_p 	= (batch_t *) src_p;
	/* hp: we don't have to multiply size by reclen (only one field) */
	batch_t *batch_dest	= batch_new(1 /* unsued */, batch_src_p->size * sizeof(simd_t));

	simd_t *data_src_p 	= batch_src_p->start;
	simd_t *data_dest 	= batch_dest->start;

	uint32_t i;

	xzl_bug_on_msg(sizeof(simd_t) < sizeof(x_addr),
			"size of simd_t is smaller than x_addr");

#ifdef KNL
	/* for hym */

#else
	/* do scatter and take batch of keys (only one field) */
	uint32_t len_4 = (batch_src_p->size / 4) * 4;

	double const* base_addr = 0;

	__m256i ld_idx;
	volatile __m256d ld_data;

	for (i = 0; i < len_4; i += 4) {
		ld_idx = _mm256_loadu_si256((__m256i const *) &data_src_p[i]);
		ld_data = _mm256_i64gather_pd(base_addr + keypos, ld_idx, 1);
		_mm256_storeu_pd((double *)data_dest, ld_data);

		data_dest += 4;
	}
	//len_last(< 4): use memcpy
	for(i = len_4; i < batch_src_p->size; i++){
		/* copy corresponding colum in the record */
		memcpy(data_dest, (simd_t*) data_src_p[i] + keypos,
				1 * sizeof(simd_t));
		data_dest ++;
	}
#endif
	batch_update(batch_dest, data_dest);
	*dest = (x_addr) batch_dest;

	return 0;
}


/* Taking a batch of pointers, assemble a batch of records based on the pointers.
 * @src_p: the input batch of pointers
 * @dest: output batch of records
 *
 * the caller must ensure that src batches (pointed by @src_p) are valid
 *
 * unsupported if size(simd_t) < pointer size
 */
int32_t expand_p(x_addr *dest, x_addr src_p, idx_t reclen) {
	
	if(src_p == 0){
		*dest = 0;
		return 0;
	}

	batch_t *batch_src_p 	= (batch_t *) src_p;
	batch_t *batch_dest	= batch_new(1 /* unsued */, batch_src_p->size * sizeof(simd_t) * reclen);

	/* hp: size of data_dest = batch_src_p->size * reclen */
	simd_t *data_src_p 	= batch_src_p->start;
	simd_t *data_dest 	= batch_dest->start;

	uint32_t i;

	xzl_bug_on_msg(sizeof(simd_t) < sizeof(x_addr),
								 "size of simd_t is smaller than x_addr");

#ifdef KNL
	uint32_t j;
	uint32_t len_8 = (batch_src_p->size/8) * 8;

	__m512i ld_idx;
	__m512i st_idx = _mm512_set_epi64(0, 1*reclen, 2*reclen, 3*reclen, 4*reclen, 5*reclen, 6*reclen, 7*reclen);
	volatile __m512d ld_data;
	//len_8(8 aligned pointers: e.g. 8, 16,32..): use gather/acatter
	void *base_addr = 0;
	for(i = 0; i < len_8; i = i+8){
		ld_idx = _mm512_set_epi64(data_src_p[i], data_src_p[i+1],
					  data_src_p[i+2], data_src_p[i+3],
					  data_src_p[i+4], data_src_p[i+5],
					  data_src_p[i+6], data_src_p[i+7]);
		for(j = 0; j < reclen; j++){
			ld_data = _mm512_i64gather_pd(ld_idx, (void *)(base_addr + j*8), 1);	
			_mm512_i64scatter_pd((void *)(data_dest + j), st_idx, ld_data, 8);
		}
		data_dest += reclen * 8;
	}

	//len_last(< 8): use memcpy
	for(i = len_8; i < batch_src_p->size; i++){
		memcpy(data_dest, (simd_t*) data_src_p[i],
				reclen * sizeof(simd_t));
		data_dest += reclen;
	}

#else
	/* for AVX, scatter intrinsic is not supported
	   hp: use gather only in case of reclen = 1 (need column offset param)
	   @dest batch consists of one column data
	   TODO: we can get col_offset as parameter of the function or
	   		 make new interface to expand one column
	*/
	if (reclen == 1) {
		uint32_t len_4 = (batch_src_p->size/4) * 4;

		idx_t col_offset = 1;	/* temporal offset */
		double const* base_addr = 0;

		__m256i ld_idx;
		volatile __m256d ld_data;

		for (i = 0; i < len_4; i += 4) {
			ld_idx = _mm256_loadu_si256((__m256i const *) &data_src_p[i]);
//			__m256d dst = _mm256_i64gather_pd(double const* base_addr, __m256i vindex, const int scale)
			ld_data = _mm256_i64gather_pd(base_addr + col_offset, ld_idx, 1);

			_mm256_storeu_pd((double *)data_dest, ld_data);

			data_dest += 4;
		}
		//len_last(< 4): use memcpy
		I("len_4 : %ld, batch_src_p->size : %d", len_4, batch_src_p->size);
		for(i = len_4; i < batch_src_p->size; i++){
			/* copy corresponding colum in the record */
			memcpy(data_dest, (simd_t*) data_src_p[i] + col_offset,
					1 * sizeof(simd_t));
			data_dest += reclen;
		}
	} else {
		/* Otherwise, just read one pointer and expand corresponding record */
		for(i = 0; i < batch_src_p->size; i++) {
			memcpy(data_dest, (simd_t*) data_src_p[i],
					reclen * sizeof(simd_t));
			data_dest += reclen;
		}
	}
#endif
	batch_update(batch_dest, data_dest);
	*dest = (x_addr) batch_dest;

	return 0;
}

/* hym: only expand @records start from offset of pbatch
 *      @dest should be preallocated
 */
int32_t expand_p_one_part(x_addr *dest, x_addr src_p, idx_t reclen, uint32_t offset, uint32_t len) {
	
	if(src_p == 0){
		*dest = 0;
		return 0;
	}
	xzl_bug_on_msg(dest == NULL, "dest is not preallocated in expand_p_one_part");
	
	batch_t *batch_src_p = (batch_t *) src_p;
	batch_t *batch_dest = (batch_t *) (*dest);
	
	simd_t *data_src_p = batch_src_p->start + offset;
	simd_t *data_dest = batch_dest->start + (offset * reclen);

	uint32_t i;

	xzl_bug_on_msg(sizeof(simd_t) < sizeof(x_addr), "size of simd_t is smaller than x_addr");

	for(i = 0; i < len; i++) {
		memcpy(data_dest, (simd_t*) data_src_p[i],
				reclen * sizeof(simd_t));
		data_dest += reclen;
	}

	return 0;
}
