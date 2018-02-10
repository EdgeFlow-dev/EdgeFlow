#ifndef XPLANE_MERGE_H
#define XPLANE_MERGE_H

#include "common-types.h"
#include "xfunc.h"


int32_t xplane_merge(x_addr *outbuf, x_params *params);
//uint32_t xplane_merge(batch_t **dest, batch_t *srcA, batch_t *srcB);
TEE_Result xfunc_merge(x_arg p[NUM_ARGS]);
#endif
