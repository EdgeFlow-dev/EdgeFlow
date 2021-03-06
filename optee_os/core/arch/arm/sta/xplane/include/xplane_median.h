#ifndef XPLANE_MEDIAN_H
#define XPLANE_MEDIAN_H

#include "type.h"
#include "xplane_common.h"

enum func_median {
	med_bykey_t,
	med_all_t,
	med_all_s_t,
	topk_bykey_t,
	kpercentile_bykey_t,
	kpercentile_all_t
};

int32_t med_bykey(batch_t **dests, batch_t *src, x_params *params);
int32_t topk_bykey(batch_t **dests, batch_t *src, x_params *params);
int32_t kpercentile_bykey(batch_t **dests, batch_t *src, x_params *params);
int32_t kpercentile_all(batch_t **dests, batch_t *src, x_params *params);

/* -- med all TBD -- */
int32_t med_all(batch_t *dest, batch_t *src, x_params *params);
//int32_t med_all_s(xscalar_t *dest, batch_t *src, x_params *params);

TEE_Result xfunc_median(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]);

#endif
