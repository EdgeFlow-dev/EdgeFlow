#ifndef XPLANE_MERGE_H
#define XPLANE_MERGE_H

#include "type.h"
#include "xplane_common.h"


int32_t xplane_merge(x_addr *outbuf, x_params *params);
//uint32_t xplane_merge(batch_t **dest, batch_t *srcA, batch_t *srcB);
TEE_Result xfunc_merge(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]);
#endif
