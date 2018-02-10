#ifndef XPLANE_JOIN_H
#define XPLANE_JOIN_H

#include "common-types.h"
#include "xfunc.h"

enum func_join {
	join_bykey_t,
	join_byfilter_t
};

int32_t x_join_bykey(x_addr *dests, batch_t *srcA, batch_t *srcB, x_params *params);
int32_t x_join_byfilter(x_addr *dests, batch_t *srcA, batch_t*srcB, x_params *params);

TEE_Result xfunc_join(TEE_Param p[TEE_NUM_PARAMS]);
#endif
