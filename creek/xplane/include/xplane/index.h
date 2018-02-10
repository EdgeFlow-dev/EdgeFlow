#ifdef INDEX_H
#define INDEX_H

#include "xplane_lib.h"

int32_t extract_kp(x_addr *dest_k, x_addr *dest_p, x_addr src, idx_t keypos, idx_t reclen);
int32_t expand_p(x_addr *dest, x_addr src_p, idx_t reclen);

#endif
