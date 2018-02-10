#ifndef XPLANE_FILTER_H
#define XPLANE_FILTER_H

#include "xfunc.h"
#include "common-types.h"

//#include <arm_neon.h>

uint32_t xplane_scalar_filter_band(batch_t **dests, batch_t *src,
			x_params *params);
/********************** for neon filter **********************\
unsigned long all_filtered(uint32x4_t v32);
uint32_t xplane_neon_filter_band(batch_t **dests, batch_t *src, \
			x_params *params);
*************************************************************/
TEE_Result xfunc_filter(TEE_Param p[TEE_NUM_PARAMS]);

#endif
