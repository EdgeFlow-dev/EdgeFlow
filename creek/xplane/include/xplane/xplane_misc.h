#ifndef XPLANE_MISC_H
#define XPLANE_MISC_H

#include "common-types.h"
#include "xfunc.h"


enum func_misc {
	unique_key_t,
};


/* before calling, src should be sorted */
int32_t x_unique_key(x_addr *dest, batch_t *src, x_params *params);

TEE_Result xfunc_misc(TEE_Param p[TEE_NUM_PARAMS]);

#endif
