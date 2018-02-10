#include <assert.h>
#include "avxsort_core.h"
#include "utils.h"
#include <xplane_lib.h>
#include <mm/batch.h>
#include <iostream>
#include <cstring>
#include <log.h>

using namespace std;

uint64_t avx_merge(int64_t * const inpA, int64_t * const inpAp,
                   int64_t * const inpB, int64_t * const inpBp,
                   int64_t * const out, int64_t * const outp,
                   const uint64_t lenA, const uint64_t lenB)
{
    /* is equal length? */

    /* TODO: There is a problem when using merge-eqlen variants, because the
    merge routine does not consider that other lists begin where one list ends
    and might be overwriting a few tuples. */
    // if(iseqlen) {
    //   merge16_eqlen(inpA, inpAp, inpB, inpBp, out, outp, lenA, true);
    // }
    // else {
        merge16_varlen(inpA, inpAp, inpB, inpBp, out, outp, lenA, lenB);
    // }

    return (lenA + lenB);
}
int32_t merge(x_addr *dests, uint32_t n_outputs, x_addr srca, x_addr srcb,
              idx_t keypos, idx_t reclen)
{
    uint32_t i, nitems_A, nitems_B, nitems_perA, nitems_perB, remainA, remainB;
    struct batch *inAk, *inAv, *inBk, *inBv, *outk, *outv,
        *srca_batch, *srcb_batch;
    struct batch **dests_batch = (struct batch **)dests;

    srca_batch = (struct batch*)srca;
    srcb_batch = (struct batch*)srcb;

    if (!srca_batch->size && !srcb_batch->size) {
        // Both input src are empty
        for (i = 0; i < n_outputs; ++i)
            dests_batch[i] = batch_new(0, 4);
        return 0;
    }
    else if (!srca_batch->size || !srcb_batch->size) {
        simd_t *src;
        uint32_t nitems_perbatch, remain;

        if (!srca_batch->size) {
            src = srcb_batch->start;
            nitems_perbatch = srcb_batch->size / reclen / n_outputs;
            remain = srcb_batch->size / reclen % n_outputs;
        }
        else {
            src = srca_batch->start;
            nitems_perbatch = srca_batch->size / reclen / n_outputs;
            remain = srca_batch->size / reclen % n_outputs;
        }

        for (i = 0; i < n_outputs - 1; ++i) {
            dests_batch[i] = batch_new(0, nitems_perbatch *
                                       reclen * sizeof(simd_t));
            memcpy(dests_batch[i]->start, src + i * nitems_perbatch * reclen,
                   nitems_perbatch * reclen * sizeof(simd_t));
        }
        dests_batch[i] = batch_new(0, (nitems_perbatch + remain) *
                                   reclen * sizeof(simd_t));
        memcpy(dests_batch[i]->start, src + i * nitems_perbatch * reclen,
               (nitems_perbatch + remain) * reclen * sizeof(simd_t));
        return 0;
    }

    nitems_A = srca_batch->size / reclen;
    nitems_B = srcb_batch->size / reclen;
    nitems_perA = nitems_A / n_outputs;
    nitems_perB = nitems_B / n_outputs;
    remainA = nitems_A % nitems_perA;
    remainB = nitems_B % nitems_perB;

    inAk = batch_new(0, (nitems_perA + remainA) * sizeof(simd_t));
    inAv = batch_new(0, (nitems_perA + remainA) * sizeof(simd_t));
    inBk = batch_new(0, (nitems_perB + remainB) * sizeof(simd_t));
    inBv = batch_new(0, (nitems_perB + remainB) * sizeof(simd_t));
    outk = batch_new(0, (nitems_perA + remainA + nitems_perB + remainB) *
                     sizeof(simd_t));
    outv = batch_new(0, (nitems_perA + remainA + nitems_perB + remainB) *
                     sizeof(simd_t));
    // dump_arr_rec("srcA", ((struct batch*)srca)->start, reclen, 512);
    // dump_arr_rec("srcB", ((struct batch*)srcb)->start, reclen, 512);

    for (i = 0; i < n_outputs - 1; ++i) {
        kv_split(srca_batch->start + i * nitems_perA * reclen,
                 inAk->start, inAv->start, keypos, reclen, nitems_perA);
        kv_split(srcb_batch->start + i * nitems_perA * reclen,
                 inBk->start, inBv->start, keypos, reclen, nitems_perB);
        avx_merge(inAk->start, inAv->start, inBk->start, inBv->start,
                  outk->start, outv->start, nitems_perA, nitems_perB);
        dests_batch[i] = batch_new(0, (nitems_perA + nitems_perB) *
                                   reclen * sizeof(simd_t));
        kv_merge(dests_batch[i]->start, outv->start, reclen,
                 nitems_perA + nitems_perB);
        // batch_check_overflow(dests_batch[i], &dests_batch[i]->start,
        //                      (nitems_perA + nitems_perB)* reclen);
        batch_update(dests_batch[i], dests_batch[i]->start +
                     (nitems_perA + nitems_perB) * reclen);
    }
    kv_split(srca_batch->start, inAk->start, inAv->start,
             keypos, reclen, nitems_perA + remainA);
    kv_split(srcb_batch->start, inBk->start, inBv->start,
             keypos, reclen, nitems_perB + remainB);
    avx_merge(inAk->start, inAv->start, inBk->start, inBv->start,
              outk->start, outv->start, nitems_perA + remainA,
              nitems_perB + remainB);
    dests_batch[i] = batch_new(0, (nitems_perA + remainA + nitems_perB + remainB)
                               * reclen * sizeof(simd_t));

    kv_merge(dests_batch[i]->start, outv->start, reclen,
             nitems_perA + nitems_perB + remainA + remainB);
    batch_update(dests_batch[i], dests_batch[i]->start +
                 (nitems_perA + nitems_perB + remainA + remainB) * reclen);
    // for (i = 0; i < n_outputs; i++)
    //     dump_arr_rec("ret", dests_batch[i]->start, reclen, 1024);
    BATCH_KILL(inAk, 0);
    BATCH_KILL(inAv, 0);
    BATCH_KILL(inBk, 0);
    BATCH_KILL(inBv, 0);
    BATCH_KILL(outk, 0);
    BATCH_KILL(outv, 0);
    return 0;
}

int32_t merge_kp(x_addr *dests_k, x_addr *dests_p, uint32_t n_outputs,
                 x_addr srcA_k, x_addr srcA_p, x_addr srcB_k, x_addr srcB_p)
{
    uint32_t i, nitems_A, nitems_B, nitems_perA, nitems_perB, remainA, remainB;
    struct batch *inAk, *inAv, *inBk, *inBv, **outk, **outv;

    inAk = (struct batch*)srcA_k;
    inAv = (struct batch*)srcA_p;
    inBk = (struct batch*)srcB_k;
    inBv = (struct batch*)srcB_p;
    outk = (struct batch**)dests_k;
    outv = (struct batch**)dests_p;

    if (!inAk->size && !inBk->size) {
        // Both input src are empty
        for (i = 0; i < n_outputs; ++i) {
            outk[i] = batch_new(0, 4);
            outv[i] = batch_new(0, 4);
        }
        return 0;
    }
    else if (!inAk->size || !inBk->size) {
        simd_t *srck, *srcv;
        uint32_t nitems_perbatch, remain;

        if (!inAk->size) {
            srck = inBk->start;
            srcv = inBv->start;
            nitems_perbatch = inBk->size / n_outputs;
            remain = inBk->size % n_outputs;
        }
        else {
            srck = inAk->start;
            srcv = inAv->start;
            nitems_perbatch = inAk->size / n_outputs;
            remain = inAk->size % n_outputs;
        }

        for (i = 0; i < n_outputs - 1; ++i) {
            outk[i] = batch_new(0, nitems_perbatch * sizeof(simd_t));
            outv[i] = batch_new(0, nitems_perbatch * sizeof(simd_t));
            memcpy(outk[i]->start, srck + i * nitems_perbatch,
                   nitems_perbatch * sizeof(simd_t));
            memcpy(outv[i]->start, srcv + i * nitems_perbatch,
                   nitems_perbatch * sizeof(simd_t));
        }
        outk[i] = batch_new(0, (nitems_perbatch + remain) * sizeof(simd_t));
        outv[i] = batch_new(0, (nitems_perbatch + remain) * sizeof(simd_t));
        memcpy(outk[i]->start, srck + i * nitems_perbatch,
               (nitems_perbatch + remain) * sizeof(simd_t));
        memcpy(outv[i]->start, srcv + i * nitems_perbatch,
               (nitems_perbatch + remain) * sizeof(simd_t));
        return 0;
    }

    nitems_A = inAk->size;
    nitems_B = inBk->size;
    nitems_perA = nitems_A / n_outputs;
    nitems_perB = nitems_B / n_outputs;
    remainA = nitems_A % nitems_perA;
    remainB = nitems_B % nitems_perB;

    for (i = 0; i < n_outputs - 1; ++i) {
        outk[i] = batch_new(0, (nitems_perA + nitems_perB) * sizeof(simd_t));
        outv[i] = batch_new(0, (nitems_perA + nitems_perB) * sizeof(simd_t));

        avx_merge(inAk->start + i * nitems_perA, inAv->start + i * nitems_perA,
                  inBk->start + i * nitems_perB, inBv->start + i * nitems_perB,
                  outk[i]->start, outv[i]->start, nitems_perA, nitems_perB);

        batch_update(outk[i], outk[i]->start + (nitems_perA + nitems_perB));
        batch_update(outv[i], outv[i]->start + (nitems_perA + nitems_perB));
    }
    outk[i] = batch_new(0, (nitems_perA + nitems_perB + remainA + remainB) *
                        sizeof(simd_t));
    outv[i] = batch_new(0, (nitems_perA + nitems_perB + remainA + remainB) *
                        sizeof(simd_t));

    avx_merge(inAk->start + i * nitems_perA, inAv->start + i * nitems_perA,
              inBk->start + i * nitems_perB, inBv->start + i * nitems_perB,
              outk[i]->start, outv[i]->start, nitems_perA + remainA,
              nitems_perB + remainB);

    batch_update(outk[i], outk[i]->start + (nitems_perA + nitems_perB +
                                            remainA + remainB));
    batch_update(outv[i], outv[i]->start + (nitems_perA + nitems_perB +
                                            remainA + remainB));

    return 0;

}
