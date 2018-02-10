#ifndef QSORT_CMP_H
#define QSORT_CMP_H 
#include "type.h"
/* xzl: XXX: simd_t - simd_t may overflow an int */

/************* For qsort ******************/
inline __attribute((always_inline))
static int cmpfunc_0(const void *a, const void *b){
	return ((simd_t *)a)[0] - ((simd_t *)b)[0];
}

inline __attribute((always_inline))
static int cmpfunc_1(const void *a, const void *b){
	return ((simd_t *)a)[1] - ((simd_t *)b)[1];
}

inline __attribute((always_inline))
static int cmpfunc_2(const void *a, const void *b){
	return ((simd_t *)a)[2] - ((simd_t *)b)[2];
}


/************* x3 stable qsort ******************/
/* keypos for comparison: 0 */
inline __attribute((always_inline))
static int sort0_sorted1(const void *a, const void *b){
	int ret = ((simd_t *)a)[0] - ((simd_t *)b)[0];
	return ret != 0 ? ret : ((simd_t *)a)[1] - ((simd_t *)b)[1];
}

inline __attribute((always_inline))
static int sort0_sorted2(const void *a, const void *b){
	int ret = ((simd_t *)a)[0] - ((simd_t *)b)[0];
	return ret != 0 ? ret : ((simd_t *)a)[2] - ((simd_t *)b)[2];
}

inline __attribute((always_inline))
static int sort0_sorted3(const void *a, const void *b){
	int ret = ((simd_t *)a)[0] - ((simd_t *)b)[0];
	return ret != 0 ? ret : ((simd_t *)a)[3] - ((simd_t *)b)[3];
}

/* keypos for comparison: 1 */
inline __attribute((always_inline))
static int sort1_sorted0(const void *a, const void *b){
	int ret = ((simd_t *)a)[1] - ((simd_t *)b)[1];
	return ret != 0 ? ret : ((simd_t *)a)[0] - ((simd_t *)b)[0];
}

inline __attribute((always_inline))
static int sort1_sorted2(const void *a, const void *b){
	int ret = ((simd_t *)a)[1] - ((simd_t *)b)[1];
	return ret != 0 ? ret : ((simd_t *)a)[2] - ((simd_t *)b)[2];
}

inline __attribute((always_inline))
static int sort1_sorted3(const void *a, const void *b){
	int ret = ((simd_t *)a)[1] - ((simd_t *)b)[1];
	return ret != 0 ? ret : ((simd_t *)a)[3] - ((simd_t *)b)[3];
}

/* keypos for comparison: 2 */
inline __attribute((always_inline))
static int sort2_sorted0(const void *a, const void *b){
	int ret = ((simd_t *)a)[2] - ((simd_t *)b)[2];
	return ret != 0 ? ret : ((simd_t *)a)[0] - ((simd_t *)b)[0];
}

inline __attribute((always_inline))
static int sort2_sorted1(const void *a, const void *b){
	int ret = ((simd_t *)a)[2] - ((simd_t *)b)[2];
//	return ret != 0 ? ret : (long)((simd_t *)a - (simd_t *)b);
	return ret != 0 ? ret : ((simd_t *)a)[1] - ((simd_t *)b)[1];
}

inline __attribute((always_inline))
static int sort2_sorted3(const void *a, const void *b){
	int ret = ((simd_t *)a)[2] - ((simd_t *)b)[2];
	return ret != 0 ? ret : ((simd_t *)a)[3] - ((simd_t *)b)[3];
}
/******************************************/

/* keypos for comparison: 3 */
inline __attribute((always_inline))
static int sort3_sorted0(const void *a, const void *b){
	int ret = ((simd_t *)a)[3] - ((simd_t *)b)[3];
	return ret != 0 ? ret : ((simd_t *)a)[0] - ((simd_t *)b)[0];
}

inline __attribute((always_inline))
static int sort3_sorted1(const void *a, const void *b){
	int ret = ((simd_t *)a)[3] - ((simd_t *)b)[3];
	return ret != 0 ? ret : ((simd_t *)a)[1] - ((simd_t *)b)[1];
}

inline __attribute((always_inline))
static int sort3_sorted2(const void *a, const void *b){
	int ret = ((simd_t *)a)[3] - ((simd_t *)b)[3];
	return ret != 0 ? ret : ((simd_t *)a)[2] - ((simd_t *)b)[2];
}
/******************************************/
#endif /* QSORT_CMP_H */
