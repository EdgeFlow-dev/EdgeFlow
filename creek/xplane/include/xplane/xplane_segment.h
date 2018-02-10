#ifndef OPERATIONS_H
#define OPERATIONS_H

#include "xfunc.h"
#include "common-types.h"

typedef struct {
	xscalar_t seg_base;
	x_addr seg_ref;
} seg_t;

typedef struct {
	/* output */
	uint32_t n_segs;

	/* input */
	x_addr src;

	uint32_t n_outputs;
	uint32_t base;
	uint32_t subrange;

	idx_t tspos;
	idx_t reclen;
} seg_arg;



int32_t xplane_segment(struct batch *dest, struct batch *src, int32_t max);
int32_t x1_xplane_segment(struct batch *dest, struct batch *src, int32_t size);
TEE_Result xfunc_segment(TEE_Param p[TEE_NUM_PARAMS]);

#endif
