#ifndef XPLANE_SORT_H
#define XPLANE_SORT_H

#include "type.h"
#include "xplane_common.h"

enum func_sort{
	x_sort_t,
	qsort_t,
	stable_sort_t
};

int32_t xplane_qsort(x_addr *dests, int32_t *src_data,
		int32_t *src_end, x_params *params);
int32_t xplane_sort(x_addr *dests, int32_t *src_data,
		int32_t *src_end, x_params *params);
TEE_Result xfunc_sort(uint32_t type, TEE_Param p[TEE_NUM_PARAMS]);


/************* For qsort ******************/
inline __attribute((always_inline))
static int cmpfunc_0(const void *a, const void *b){
	return ((int *)a)[0] - ((int *)b)[0];
//	int ret = ((int *)a)[0] - ((int *)b)[0];
//	return ret != 0 ? ret : ((int *)a)[1] - ((int *)b)[1];
}

inline __attribute((always_inline))
static int cmpfunc_1(const void *a, const void *b){
	int ret = ((int *)a)[1] - ((int *)b)[1];
	return ret != 0 ? ret : ((int *)a)[0] - ((int *)b)[0];
}
inline __attribute((always_inline))
static int cmpfunc_2(const void *a, const void *b){
	int ret = ((int *)a)[2] - ((int *)b)[2];
	return ret != 0 ? ret : ((int *)a)[1] - ((int *)b)[1];
}


/************* x3 stable qsort ******************/
/* keypos for comparison: 0 */
inline __attribute((always_inline))
static int sort0_sorted1(const void *a, const void *b){
	int ret = ((int *)a)[0] - ((int *)b)[0];
	return ret != 0 ? ret : ((int *)a)[1] - ((int *)b)[1];
}

inline __attribute((always_inline))
static int sort0_sorted2(const void *a, const void *b){
	int ret = ((int *)a)[0] - ((int *)b)[0];
	return ret != 0 ? ret : ((int *)a)[2] - ((int *)b)[2];
}

inline __attribute((always_inline))
static int sort0_sorted3(const void *a, const void *b){
	int ret = ((int *)a)[0] - ((int *)b)[0];
	return ret != 0 ? ret : ((int *)a)[3] - ((int *)b)[3];
}

/* keypos for comparison: 1 */
inline __attribute((always_inline))
static int sort1_sorted0(const void *a, const void *b){
	int ret = ((int *)a)[1] - ((int *)b)[1];
	return ret != 0 ? ret : ((int *)a)[0] - ((int *)b)[0];
}

inline __attribute((always_inline))
static int sort1_sorted2(const void *a, const void *b){
	int ret = ((int *)a)[1] - ((int *)b)[1];
	return ret != 0 ? ret : ((int *)a)[2] - ((int *)b)[2];
}

inline __attribute((always_inline))
static int sort1_sorted3(const void *a, const void *b){
	int ret = ((int *)a)[1] - ((int *)b)[1];
	return ret != 0 ? ret : ((int *)a)[3] - ((int *)b)[3];
}

/* keypos for comparison: 2 */
inline __attribute((always_inline))
static int sort2_sorted0(const void *a, const void *b){
	int ret = ((int *)a)[2] - ((int *)b)[2];
	return ret != 0 ? ret : ((int *)a)[0] - ((int *)b)[0];
}

inline __attribute((always_inline))
static int sort2_sorted1(const void *a, const void *b){
	int ret = ((int *)a)[2] - ((int *)b)[2];
//	return ret != 0 ? ret : (long)((int *)a - (int *)b);
	return ret != 0 ? ret : ((int *)a)[1] - ((int *)b)[1];
}

inline __attribute((always_inline))
static int sort2_sorted3(const void *a, const void *b){
	int ret = ((int *)a)[2] - ((int *)b)[2];
	return ret != 0 ? ret : ((int *)a)[3] - ((int *)b)[3];
}
/******************************************/

/* keypos for comparison: 3 */
inline __attribute((always_inline))
static int sort3_sorted0(const void *a, const void *b){
	int ret = ((int *)a)[3] - ((int *)b)[3];
	return ret != 0 ? ret : ((int *)a)[0] - ((int *)b)[0];
}

inline __attribute((always_inline))
static int sort3_sorted1(const void *a, const void *b){
	int ret = ((int *)a)[3] - ((int *)b)[3];
	return ret != 0 ? ret : ((int *)a)[1] - ((int *)b)[1];
}

inline __attribute((always_inline))
static int sort3_sorted2(const void *a, const void *b){
	int ret = ((int *)a)[3] - ((int *)b)[3];
	return ret != 0 ? ret : ((int *)a)[2] - ((int *)b)[2];
}
/******************************************/
#endif
