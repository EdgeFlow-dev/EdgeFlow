#include "xplane/xplane_interface.h"
#include "xplane/xfunc.h"

extern int32_t xfunc_uarray_debug(x_arg p[NUM_ARGS]);
extern int32_t xfunc_create_uarray(x_arg p[NUM_ARGS]);
extern int32_t xfunc_create_uarray_rand(x_arg p[NUM_ARGS]);
extern int32_t xfunc_create_uarray_nsbuf(x_arg p[NUM_ARGS]);
extern int32_t xfunc_uarray_to_nsbuf(x_arg p[NUM_ARGS]);
extern int32_t xfunc_retire_uarray(x_arg p[NUM_ARGS]);
extern int32_t xfunc_pseudo_source(x_arg p[NUM_ARGS]);
extern int32_t xfunc_concatenate(x_arg p[NUM_ARGS]);

#ifdef ARM_NEON
extern int32_t xfunc_sort(x_arg args[NUM_ARGS]);
extern int32_t xfunc_merge(x_arg args[NUM_ARGS]);
#else
#define xfunc_sort(arg)		0
#define xfunc_merge(arg)	0
#endif

extern int32_t xfunc_join(x_arg p[NUM_ARGS]);
extern int32_t xfunc_filter(x_arg p[NUM_ARGS]);
extern int32_t xfunc_sumcount(x_arg p[NUM_ARGS]);
extern int32_t xfunc_median(x_arg p[NUM_ARGS]);
extern int32_t xfunc_misc(x_arg p[NUM_ARGS]);
extern int32_t xfunc_segment(x_arg p[NUM_ARGS]);

int32_t invoke_command(uint32_t nCommandID,
		x_arg args[NUM_ARGS]){

	switch (nCommandID) {
#if 0
		case X_CMD_MAP:
			sz_populate_map();
			return TEE_SUCCESS;

#endif
		/********************** X_CMD_UARRAY **************************/
		case X_CMD_UA_RAND :
			return xfunc_create_uarray_rand(args);
		case X_CMD_UA_NSBUF :
			return xfunc_create_uarray_nsbuf(args);
		case X_CMD_UA_CREATE :
			return xfunc_create_uarray(args);
		case X_CMD_UA_RETIRE :
			return xfunc_retire_uarray(args);
		case X_CMD_UA_PSEUDO :
			return xfunc_pseudo_source(args);
		case X_CMD_UA_TO_NS :
			return xfunc_uarray_to_nsbuf(args);
		case X_CMD_UA_DEBUG :
			return xfunc_uarray_debug(args);
/*
		case X_CMD_SHOW_SBUFF :
			sz_show_resource();
			return TEE_SUCCESS;
*/
		/*********************************** **************************/
		case X_CMD_SORT:
			return xfunc_sort(args);
		case X_CMD_MERGE:
			return xfunc_merge(args);
		case X_CMD_SEGMENT:
			return xfunc_segment(args);
		case X_CMD_JOIN:
			return xfunc_join(args);
		case X_CMD_FILTER:
			return xfunc_filter(args);
		case X_CMD_MEDIAN:
			return xfunc_median(args);
		case X_CMD_SUMCOUNT:
			return xfunc_sumcount(args);
		case X_CMD_CONCATENATE:
			return xfunc_concatenate(args);
		case X_CMD_MISC:
			return xfunc_misc(args);
#if 0
		case X_CMD_KILL:
			sz_destroy_map();
			return TEE_SUCCESS;
#endif
		default:
			break;
	}
	return TEE_SUCCESS;
}

