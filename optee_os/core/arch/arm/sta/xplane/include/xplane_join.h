#ifndef XPLANE_JOIN_H
#define XPLANE_JOIN_H

#include "type.h"
#include "xplane_common.h"

enum func_join {   
	join_bykey_t,
	join_byfilter_t
};                     

int32_t join_bykey(x_addr *dests, batch_t *srcA, batch_t *srcB, x_params *params);
int32_t join_byfilter(x_addr *dests, batch_t *srcA, batch_t*srcB, x_params *params);

TEE_Result xfunc_join(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]);
#endif
