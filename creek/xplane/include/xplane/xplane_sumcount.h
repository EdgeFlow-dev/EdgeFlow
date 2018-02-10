#ifndef XPLANE_SUMCOUNT_H
#define XPLANE_SUMCOUNT_H

#include "xfunc.h"
#include "common-types.h"

enum func_sumcount {
	sumcount1_perkey_t,
	sumcount_perkey_t,
	sum_perkey_t,
	avg_perkey_t,
	sumcount1_all_t,
	sumcount_all_t,
	avg_all_t,
};


/* --- sumcount/avg per key --- */
int32_t x_sum_perkey(x_addr *dests, simd_t *src_data, simd_t *src_end, x_params *params);
int32_t x_sumcount1_perkey(x_addr *dests, simd_t *src_data, simd_t *src_end, x_params *params);
int32_t x_sumcount_perkey(x_addr *dests, simd_t *src_data, simd_t *src_end, x_params *params);
int32_t x_avg_perkey(x_addr *dests, simd_t *src, simd_t *src_end,  x_params *params);

/* --- sumcount/avg all. result: one record  --- */
int32_t x_sumcount1_all(x_addr *dest, batch_t *src, x_params *params);
int32_t x_sumcount_all(x_addr *dest, batch_t *src, x_params *params);
int32_t x_avg_all(x_addr *dest, batch_t *src, x_params *params);

TEE_Result xfunc_sumcount(TEE_Param p[TEE_NUM_PARAMS]);

#endif
