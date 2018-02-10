#include <assert.h>
/* #include "avxsort.h" */
#include "avxsort_core.h"
#include "utils.h"
#include "params.h"
#include <xplane_lib.h>
#include <log.h>
#include <mm/batch.h>
#include <iostream>
#include <cstring>

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

using namespace std;
/** L2 Cache size of the system in bytes, used for determining block size */
#ifndef L2_CACHE_SIZE
#define L2_CACHE_SIZE (256*1024)
#endif

/** Number of tuples that can fit into L2 cache divided by 2 */
#ifndef BLOCKSIZE
#define BLOCKSIZE (L2_CACHE_SIZE / (4 * sizeof(int64_t)))
#endif

/* sz: TODO:
   Use AVX gather instr to split keys & ptrs

   AVX2 does not support scatter ~.~
*/
void
avxsort_unaligned(int64_t ** inputptr, int64_t ** inputptrv,
                  int64_t ** outputptr, int64_t ** outputptrv, uint64_t nitems)
{
    if(nitems <= 0)
        return;

    // dump_arr_int64("input", *inputptr, nitems);

    int64_t * input  = * inputptr;
    int64_t * output = * outputptr;
    int64_t * inputv  = * inputptrv;
    int64_t * outputv = * outputptrv;

    uint64_t i;
    uint64_t nchunks = (nitems / BLOCKSIZE);
    int rem = (nitems % BLOCKSIZE);

    /* each chunk keeps track of its temporary memory offset */
    int64_t * ptrs[nchunks+1][2];/* [chunk-in, chunk-out-tmp] */
    int64_t * ptrsv[nchunks+1][2];/* [chunk-in, chunk-out-tmp] */
    uint32_t sizes[nchunks+1];

    for(i = 0; i <= nchunks; i++) {
        ptrs[i][0] = input + i *  BLOCKSIZE;
        ptrsv[i][0] = inputv + i *  BLOCKSIZE;
        ptrs[i][1] = output + i * BLOCKSIZE;
        ptrsv[i][1] = outputv + i * BLOCKSIZE;
        sizes[i]   = BLOCKSIZE;
    }

    /** 1) Divide the input into chunks fitting into L2 cache. */
    /* one more chunk if not divisible */
    for(i = 0; i < nchunks; i++) {
        avxsort_block(&ptrs[i][0], &ptrsv[i][0],  &ptrs[i][1], &ptrsv[i][1], BLOCKSIZE);
	// dump_arr_int64("check 0", ptrs[i][1], BLOCKSIZE);
        swap(&ptrs[i][0], &ptrs[i][1]);
        swap(&ptrsv[i][0], &ptrsv[i][1]);
    }

    if(rem) {
        // xzl_bug_on(1);
        /* sort the last chunk which is less than BLOCKSIZE */
        avxsort_rem(&ptrs[i][0], &ptrsv[i][0], &ptrs[i][1], &ptrsv[i][1], rem);
	// dump_arr_int64("check 1", ptrs[i][1], rem);
        swap(&ptrs[i][0], &ptrs[i][1]);
        swap(&ptrsv[i][0], &ptrsv[i][1]);
        sizes[i] = rem;
    }


    /**
     * 2.a) for itr = [(logM) .. (logN -1)], merge sequences of length 2^itr to
     * obtain sorted sequences of length 2^{itr+1}.
     */
    nchunks += (rem > 0);
    /* printf("Merge chunks = %d\n", nchunks); */
    const uint64_t logN = ceil(log2(nitems));
    for(i = LOG2_BLOCKSIZE; i < logN; i++) {

        uint64_t k = 0;
        for(uint64_t j = 0; j < (nchunks-1); j += 2) {
            int64_t * inpA  = ptrs[j][0];
            int64_t * inpB  = ptrs[j+1][0];
            int64_t * out   = ptrs[j][1];
            int64_t * inpAv  = ptrsv[j][0];
            int64_t * inpBv  = ptrsv[j+1][0];
            int64_t * outv   = ptrsv[j][1];

            uint32_t  sizeA = sizes[j];
            uint32_t  sizeB = sizes[j+1];

            /* need to change */
            merge16_varlen(inpA, inpAv, inpB, inpBv, out, outv, sizeA, sizeB);

            /* setup new pointers */
            ptrs[k][0] = out;
            ptrs[k][1] = inpA;
            ptrsv[k][0] = outv;
            ptrsv[k][1] = inpAv;

            sizes[k]   = sizeA + sizeB;
            k++;
        }

        if((nchunks % 2)) {
            /* just move the pointers */
            ptrs[k][0] = ptrs[nchunks-1][0];
            ptrs[k][1] = ptrs[nchunks-1][1];
            ptrsv[k][0] = ptrsv[nchunks-1][0];
            ptrsv[k][1] = ptrsv[nchunks-1][1];

            sizes[k]   = sizes[nchunks-1];
            k++;
        }

        nchunks = k;
    }

    /* finally swap input/output pointers, where output holds the sorted list */
    * outputptr = ptrs[0][0];
    * inputptr  = ptrs[0][1];
    * outputptrv = ptrsv[0][0];
    * inputptrv  = ptrsv[0][1];

}

int32_t sort(x_addr *dests, uint32_t n_outputs, x_addr src,
             idx_t keypos, idx_t reclen)
{
    uint32_t nitems_perbuff, remain;
    uint32_t i;
    struct batch *ink, *inv, *outk, *outv, *src_batch;
    struct batch **dests_batch = (struct batch **)dests;
    src_batch = (struct batch*)src;

    // FILE *fh;

    /* sz: temporary don't support  */
    // cout << "simd_t size" << sizeof(simd_t) << endl;
    // cout << ((struct batch*)src)->size << endl;
    // cout << (uint32_t)reclen << endl;
    // cout << (uint32_t)BLOCKSIZE << endl;
    // cout << ((struct batch*)src)->start << endl;
    // dump_arr_rec("in", ((struct batch*)src)->start, reclen, ((struct batch*)src)->size);
    // std::cout << "src size " << ((struct batch*)src)->size << endl;
    // if (((((struct batch*)src)->size / reclen) % BLOCKSIZE)) {
        // xzl_bug_on((((struct batch*)src)->size / reclen) % BLOCKSIZE);
    // }
    // xzl_bug_on(nchunks % n_outputs);

    if (!src_batch->size) {
        for (i = 0; i < n_outputs; ++i)
            dests_batch[i] = batch_new(0, 4); // alloc nonzero space
        return 0;
    }

    nitems_perbuff = src_batch->size / reclen / n_outputs;
    remain = (src_batch->size / reclen) % n_outputs;

    ink = batch_new(0, (nitems_perbuff + remain) * sizeof(simd_t));
    inv = batch_new(0, (nitems_perbuff + remain) * sizeof(simd_t));
    outk = batch_new(0, (nitems_perbuff + remain) * sizeof(simd_t));
    outv = batch_new(0, (nitems_perbuff + remain) * sizeof(simd_t));

    for (i = 0; i < n_outputs - 1; i++) {
         kv_split(src_batch->start + i * nitems_perbuff * reclen,
                  ink->start, inv->start, keypos, reclen, nitems_perbuff);
         // dump_arr_int64("ink", ink->start, 64);
         // dump_arr_int64("inv", inv->start, 64);

         avxsort_unaligned(&ink->start, &inv->start, &outk->start, &outv->start,
                           nitems_perbuff);
	 // check_arr(outk->start, nitems_perbuff);

         // dump_arr_int64("outk", outk->start, 1024);
         dests_batch[i] = batch_new(0, nitems_perbuff * reclen * sizeof(simd_t));
         kv_merge(dests_batch[i]->start, outv->start, reclen, nitems_perbuff);
         batch_update(dests_batch[i], dests_batch[i]->start + nitems_perbuff * reclen);
    }
    kv_split(src_batch->start + i * nitems_perbuff * reclen,
             ink->start, inv->start, keypos, reclen, nitems_perbuff + remain);
    // dump_arr_int64("ink", ink->start, 64);
    // dump_arr_int64("inv", inv->start, 64);

    avxsort_unaligned(&ink->start, &inv->start, &outk->start, &outv->start,
                      nitems_perbuff + remain);
    // check_arr(outk->start, nitems_perbuff + remain);

    // dump_arr_int64("outk", outk->start, 1024);
    dests_batch[i] = batch_new(0, src_batch->size * sizeof(simd_t));
    kv_merge(dests_batch[i]->start, outv->start, reclen, nitems_perbuff + remain);
    batch_update(dests_batch[i], dests_batch[i]->start + (nitems_perbuff + remain) * reclen);

    // fh = fopen("output.txt", "w+");
    // fprintf(fh, "input %ld\n", src_batch->size / reclen);
    // for (j = 0; j < n_outputs; j++) {
    //     int64_t *outp = dests_batch[j]->start;
    //     fprintf(fh, "-------------n_output %d---%ld------------\n", j,
    //             dests_batch[j]->size / reclen);
    //     for (i = 0; i < dests_batch[j]->size / reclen; i++) {
    //         fprintf(fh, "%ld %ld %ld\n", outp[i * reclen], outp[i * reclen + 1],
    //                 outp[i * reclen + 2]);
    //     }
    // }
    // fclose(fh);
    BATCH_KILL(ink, 0);
    BATCH_KILL(inv, 0);
    BATCH_KILL(outk, 0);
    BATCH_KILL(outv, 0);

    return 0;
}

int32_t sort_kp(x_addr *dests_k, x_addr *dests_p, uint32_t n_outputs,
                x_addr src_k, x_addr src_p)
{
    uint32_t nitems_perbuff, remain;
    uint32_t i;
    struct batch *ink, *inv, **outk, **outv, *ink_tmp, *inv_tmp;

    ink = (struct batch*)src_k;
    inv = (struct batch*)src_p;
    outk = (struct batch**)dests_k;
    outv = (struct batch**)dests_p;

    if (!ink->size) {
        for (i = 0; i < n_outputs; ++i) {
            outk[i] = batch_new(0, 4); // alloc nonzero space
            outv[i] = batch_new(0, 4);
        }
        return 0;
    }

    nitems_perbuff = ink->size / n_outputs;
    remain = ink->size % n_outputs;

    ink_tmp = batch_new(0, (nitems_perbuff + remain) * sizeof(simd_t));
    inv_tmp = batch_new(0, (nitems_perbuff + remain) * sizeof(simd_t));
    for (i = 0; i < n_outputs - 1; i++) {
        outk[i] = batch_new(0, (nitems_perbuff + remain) * sizeof(simd_t));
        outv[i] = batch_new(0, (nitems_perbuff + remain) * sizeof(simd_t));
        memcpy(ink_tmp->start, ink->start + i * nitems_perbuff,
               nitems_perbuff * sizeof(simd_t));
        memcpy(inv_tmp->start, inv->start + i * nitems_perbuff,
               nitems_perbuff * sizeof(simd_t));
        avxsort_unaligned(&ink_tmp->start, &inv_tmp->start, &outk[i]->start,
                          &outv[i]->start, nitems_perbuff);
        batch_update(outk[i], outk[i]->start + nitems_perbuff);
        batch_update(outv[i], outv[i]->start + nitems_perbuff);
    }
    outk[i] = batch_new(0, (nitems_perbuff + remain) * sizeof(simd_t));
    outv[i] = batch_new(0, (nitems_perbuff + remain) * sizeof(simd_t));
    memcpy(ink_tmp->start, ink->start + i * nitems_perbuff,
           (nitems_perbuff + remain) * sizeof(simd_t));
    memcpy(inv_tmp->start, inv->start + i * nitems_perbuff,
           (nitems_perbuff + remain) * sizeof(simd_t));

    avxsort_unaligned(&ink_tmp->start, &inv_tmp->start, &outk[i]->start, &outv[i]->start,
                      nitems_perbuff + remain);
    batch_update(outk[i], outk[i]->start + nitems_perbuff);
    batch_update(outv[i], outv[i]->start + nitems_perbuff);
    BATCH_KILL(ink_tmp, 0);
    BATCH_KILL(inv_tmp, 0);
    return 0;
}
