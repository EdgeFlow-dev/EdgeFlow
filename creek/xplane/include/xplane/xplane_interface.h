#ifndef XPLANE_INTERFACE_H
#define XPLANE_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "type.h"
#include "common-types.h"

//#define NUM_ARGS	4

#if 0
int32_t xfunc_uarray_debug(uint32_t type, x_arg p[NUM_ARGS]);
int32_t xfunc_create_uarray(uint32_t type, x_arg p[NUM_ARGS]);
int32_t xfunc_create_uarray_rand(uint32_t type, x_arg p[NUM_ARGS]);
int32_t xfunc_create_uarray_nsbuf(uint32_t type, x_arg p[NUM_ARGS]);
int32_t xfunc_uarray_to_nsbuf(uint32_t type, x_arg p[NUM_ARGS]);
int32_t xfunc_retire_uarray(uint32_t type, x_arg p[NUM_ARGS]);
int32_t xfunc_pseudo_source(uint32_t type, x_arg p[NUM_ARGS]);
int32_t xfunc_sumcount(uint32_t type, x_arg p[NUM_ARGS]);
int32_t xfunc_median(uint32_t type, x_arg p[NUM_ARGS]);
int32_t xfunc_concatenate(uint32_t type, x_arg p[NUM_ARGS]);
int32_t xfunc_misc(uint32_t type, x_arg p[NUM_ARGS]);

int32_t xfunc_sort(uint32_t type, x_arg p[NUM_ARGS]);
int32_t xfunc_merge(uint32_t type, x_arg p[NUM_ARGS]);
int32_t xfunc_join(uint32_t type, x_arg p[NUM_ARGS]);
int32_t xfunc_filter(uint32_t type, x_arg p[NUM_ARGS]);
int32_t xfunc_segment(uint32_t type, x_arg p[NUM_ARGS]);
#endif

int32_t invoke_command(uint32_t nCommandID, x_arg args[NUM_ARGS]);

#endif


#ifdef __cplusplus
}
#endif
