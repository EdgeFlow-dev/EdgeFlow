#ifndef XPLANE_FILTER_H
#define XPLANE_FILTER_H

#include <kernel/thread.h>
#include <kernel/vfp.h>
#include <mm/batch.h>

#include "type.h"
#include "xplane_common.h"

unsigned long all_filtered(uint32x4_t v32);
uint32_t xplane_scalar_filter_band(batch_t **dests, batch_t *src,
			x_params *params);
uint32_t xplane_neon_filter_band(batch_t **dests, batch_t *src, \
			x_params *params);
TEE_Result xfunc_filter(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]);

#endif
