/*
 * Copyright (c) 2014, STMicroelectronics International N.V.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <compiler.h>
#include <types_ext.h>
#include <kernel/static_ta.h>
#include <trace.h>
#include <string.h>  /* for memset */
#include <tee_api_types.h>
#include <tee_api_defines.h>

#include <kernel/misc.h>
#include <atomic.h>
#include <mm/core_mmu.h>
#include <mm/batch.h>	// hp

#include "core_self_tests.h"
#include "xfunc.h"

#define TA_NAME		"xplane"

#define STA_SELF_TEST_UUID \
		{ 0xd96a5b40, 0xc3e5, 0x21e3, \
			{ 0x87, 0x94, 0x10, 0x02, 0xa5, 0xd5, 0xc6, 0x1b } }


#define N_THREADS	8
#define N_PARTS		N_THREADS


extern TEE_Result xfunc_uarray_debug(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]);
extern TEE_Result xfunc_create_uarray(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]);
extern TEE_Result xfunc_create_uarray_rand(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]);
extern TEE_Result xfunc_create_uarray_nsbuf(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]);
extern TEE_Result xfunc_uarray_to_nsbuf(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]); 
extern TEE_Result xfunc_retire_uarray(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]);
extern TEE_Result xfunc_pseudo_source(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]);
extern TEE_Result xfunc_sumcount(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]);
extern TEE_Result xfunc_median(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]);
extern TEE_Result xfunc_concatenate(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]);
extern TEE_Result xfunc_misc(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]);

extern TEE_Result xfunc_sort(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]);
extern TEE_Result xfunc_merge(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]);
extern TEE_Result xfunc_join(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]);
extern TEE_Result xfunc_filter(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]);
extern TEE_Result xfunc_segment(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]);

/* xzl: shared memory across all threads */
volatile uint32_t active_count = 0;

#if 0
static int cmpfunc(const void *a, const void *b){
	return (*(int*)a - *(int*)b);
}

static TEE_Result test_x1_qsort(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]){
	batch_t *in_batch = PARAMS_SRC(p);

	if ((TEE_PARAM_TYPE_GET(type, 0) == TEE_PARAM_TYPE_VALUE_INOUT) &&
		(TEE_PARAM_TYPE_GET(type, 1) == TEE_PARAM_TYPE_MEMREF_INOUT)){
	}

	qsort(in_batch->start, in_batch->size, sizeof(int32_t), cmpfunc);

//	neonsort_tuples(int32_t **inputptr, int32_t **outputptr, uint32_t nitems)

	return TEE_SUCCESS;
}
#endif

__unused
static TEE_Result xzl_self_tests(uint32_t param_types __unused,
		TEE_Param pParams[TEE_NUM_PARAMS])
{
	int num_rounds = 1000;

	uint32_t req_param_types =
		TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
				TEE_PARAM_TYPE_VALUE_OUTPUT,
				TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE);

	uint32_t cnt = atomic_inc32(&active_count);

	pParams[1].value.a = num_rounds;

	if (param_types != req_param_types) {
		EMSG("got param_types 0x%x, expected 0x%x",
			param_types, req_param_types);
		return TEE_ERROR_BAD_PARAMETERS;
	}

	while (num_rounds) {
		volatile size_t n = 1000;

		while (n)
			n--;

		num_rounds--;
	}

	pParams[1].value.b = cnt;
	atomic_dec32(&active_count);
//	DMSG("xzl: core %zu active cnt = %u", get_core_pos(), cnt);

	return TEE_SUCCESS;
}

/*
 * Trusted Application Entry Points
 */

static TEE_Result create_ta(void)
{
	DMSG("create entry point for static ta \"%s\"", TA_NAME);
	return TEE_SUCCESS;
}

static void destroy_ta(void)
{
	DMSG("destroy entry point for static ta \"%s\"", TA_NAME);
}

static TEE_Result open_session(uint32_t nParamTypes __unused,
		TEE_Param pParams[TEE_NUM_PARAMS] __unused,
		void **ppSessionContext __unused)
{
	DMSG("open entry point for static ta \"%s\"", TA_NAME);
	return TEE_SUCCESS;
}

static void close_session(void *pSessionContext __unused)
{
	DMSG("close entry point for static ta \"%s\"", TA_NAME);
}

static TEE_Result invoke_command(void *pSessionContext __unused,
		uint32_t nCommandID, uint32_t nParamTypes,
		TEE_Param pParams[TEE_NUM_PARAMS])
{
	DMSG("command entry point for static ta \"%s\" nCommandID : %x", TA_NAME, nCommandID);

	if(!vfp_is_enabled())
		thread_kernel_enable_vfp();

	switch (nCommandID) {
	case X_CMD_MAP:	
		sz_populate_map();
		return TEE_SUCCESS;
/********************** X_CMD_UARRAY **************************/
	case X_CMD_UA_RAND :
		return xfunc_create_uarray_rand(nParamTypes, pParams);
	case X_CMD_UA_NSBUF :
		return xfunc_create_uarray_nsbuf(nParamTypes, pParams);
	case X_CMD_UA_CREATE :
		return xfunc_create_uarray(nParamTypes, pParams);
	case X_CMD_UA_RETIRE :
		return xfunc_retire_uarray(nParamTypes, pParams);
	case X_CMD_UA_PSEUDO :
		return xfunc_pseudo_source(nParamTypes, pParams);
	case X_CMD_UA_TO_NS :
		return xfunc_uarray_to_nsbuf(nParamTypes, pParams);
	case X_CMD_UA_DEBUG :
		return xfunc_uarray_debug(nParamTypes, pParams);
	case X_CMD_SHOW_SBUFF :
		sz_show_resource();
		return TEE_SUCCESS;
/*********************************** **************************/
	case X_CMD_SORT:
		return xfunc_sort(nParamTypes, pParams);
	case X_CMD_MERGE:
		return xfunc_merge(nParamTypes, pParams);
	case X_CMD_SEGMENT:
		return xfunc_segment(nParamTypes, pParams);
	case X_CMD_JOIN:
		return xfunc_join(nParamTypes, pParams);
	case X_CMD_FILTER:
		return xfunc_filter(nParamTypes, pParams);
	case X_CMD_MEDIAN:
		return xfunc_median(nParamTypes, pParams);
	case X_CMD_SUMCOUNT:
		return xfunc_sumcount(nParamTypes, pParams);
	case X_CMD_CONCATENATE:
		return xfunc_concatenate(nParamTypes, pParams);
	case X_CMD_MISC:
		return xfunc_misc(nParamTypes, pParams);
	case X_CMD_KILL:
		sz_destroy_map();
		return TEE_SUCCESS;
	default:
		break;
	}
	return TEE_ERROR_BAD_PARAMETERS;
}

static_ta_register(.uuid = STA_SELF_TEST_UUID, .name = TA_NAME,
		   .create_entry_point = create_ta,
		   .destroy_entry_point = destroy_ta,
		   .open_session_entry_point = open_session,
		   .close_session_entry_point = close_session,
		   .invoke_command_entry_point = invoke_command);
