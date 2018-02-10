#ifndef XPLANE_MISC_H
#define XPLANE_MISC_H

#include "type.h"
#include "xplane_common.h"


enum func_misc {
	unique_key_t,
};  


/* before calling, src should be sorted */
int32_t unique_key(x_addr *dest, batch_t *src, x_params *params);

TEE_Result xfunc_misc(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]);

#endif
