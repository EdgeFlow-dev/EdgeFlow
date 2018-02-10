#ifndef XPLANE_UARRAY_H
#define XPLANE_UARRAY_H

#include <tee_api_types.h>
#include <tee_api_defines.h>

#include "type.h"
#include "xfunc.h"


enum NSBUF_STATUS{   
	BUF_NORMAL,
	BUF_START,
	BUF_CONTINUE,
	BUF_END,
};                   

enum func_ua_debug {
    uarray_size_t,   
};                  



/*
enum record_format { 
	record_format_x3,
	record_format_x4,
};
*/

typedef struct {
	uint32_t cmd;

	x_addr src;

	uint32_t src_offset;
	uint32_t count;

	idx_t reclen;
	idx_t tspos;

	int32_t ts_start;
	int32_t ts_delta;
} source_params;


TEE_Result xfunc_uarray_debug(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]);
int32_t get_uarray_size(x_params *params);

TEE_Result xfunc_create_uarray(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]);
TEE_Result xfunc_create_uarray_rand(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]);
TEE_Result xfunc_create_uarray_nsbuf(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]);
TEE_Result xfunc_uarray_to_nsbuf(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]);

TEE_Result xfunc_retire_uarray(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]);
TEE_Result xfunc_pseudo_source(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]);
TEE_Result xfunc_concatenate(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]);
#endif
