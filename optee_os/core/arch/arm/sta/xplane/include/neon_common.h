#ifndef NEON_COMMON_H
#define NEON_COMMON_H

#include <arm_neon.h>

#if defined(__cplusplus)
#undef restrict
#define restrict __restrict__
#endif


typedef struct block4	{int32_t val[4];	} block4;
typedef struct block8	{int32_t val[8];	} block8;
typedef struct block12	{int32_t val[12];	} block12;
typedef struct block16	{int32_t val[16];	} block16;
typedef struct block48	{int32_t val[48];	} block48;

int32_t vrev128q_s32(int32_t* arr);

/** Load 2 NEON 128-bit registers from the given address **/
#define LOAD8(REGL, REGH, ADDR)						\
	do {											\
		REGL = vld1q_s32((int32_t const*) ADDR);					\
		REGH = vld1q_s32((int32_t const*) (((block4 *)ADDR) + 1));	\
	} while(0)

#define LOAD8U(REGL, REGH, ADDR)	\
	do {							\
		LOAD8(REGL, REGH, ADDR);	\
	}while(0)

#define LOAD16(REGL1, REGL2, REGH1, REGH2, ADDR)		\
	do{													\
		LOAD8(REGL1, REGL2, ADDR);						\
		LOAD8(REGH1, REGH2, (((block8 *) ADDR) + 1));	\
	}while(0)


#define STORE8(ADDR, REGL, REGH)					\
	do {											\
		vst1q_s32((int32_t*) ADDR, REGL);		\
		vst1q_s32((int32_t*) (((block4 *) ADDR) + 1), REGH);	\
	} while(0)

#define STORE8U(ADDR, REGL, REGH)					\
	do{												\
		STORE8(ADDR, REGL, REGH);					\
	} while(0)

#define STORE16(ADDR, REGL1, REGL2, REGH1, REGH2)	\
	do{												\
		STORE8(ADDR, REGL1, REGL2);					\
		STORE8((((block8 *) ADDR)+1), REGH1, REGH2);	\
	} while(0)

#define REVERSE(A)									\
	do{												\
		/* [a0, a1, a2, a3] => [a1, a0, a3, a2] */ 	\
		int32x4_t rev = vrev64q_s32(A);		\
		/* [a3, a2, a1, a0] */				\
		/* bring right 2 value in leftside vector
		 * bring left 2 value in rightside vector */\
		A = vextq_s32(rev, rev, 2);			\
	} while(0);								\

// NOTE B should be reversed before bitonic merge
#define BITONIC4(O1, O2, A, B)				\
	do {									\
		/* level-1 comparisons */			\
		int32x4_t l1 = vminq_s32(A, B);		\
		int32x4_t h1 = vmaxq_s32(A, B);		\
											\
		/* level-1 shuffles */				\
		/* [a0, b0, a1, b1]	*/				\
		/* [a2, b2, a3, b3]	*/				\
		int32x4_t l1p = vzip1q_s32(l1, h1);	\
		int32x4_t h1p = vzip2q_s32(l1, h1);	\
											\
		/* level-2 comparisons */			\
		int32x4_t l2 = vminq_s32(l1p, h1p);	\
		int32x4_t h2 = vmaxq_s32(l1p, h1p);	\
											\
		/* level-2 shuffles */				\
		/* [a0, a2, b0, b2]	*/				\
		/* [a1, a3, b1, b3] */				\
		int32x4_t l2p = vzip1q_s32(l2, h2);	\
		int32x4_t h2p = vzip2q_s32(l2, h2);	\
											\
		/* level-3 comparisons */			\
		int32x4_t l3 = vminq_s32(l2p, h2p);	\
		int32x4_t h3 = vmaxq_s32(h2p, l2p);	\
											\
		/* level-3 shuffles */				\
		/* [a0, a1, a2, a3]	*/				\
		/* [b0, b1, b2, b3] */				\
		O1 = vzip1q_s32(l3, h3);			\
		O2 = vzip2q_s32(l3, h3);			\
		} while(0)							\

#define BITONIC8(O1, O2, O3, O4, A1, A2, B1, B2)	\
	do{												\
		int32x4_t l11 = vminq_s32(A1, B1);			\
		int32x4_t l12 = vminq_s32(A2, B2);			\
		int32x4_t h11 = vmaxq_s32(A1, B1);			\
		int32x4_t h12 = vmaxq_s32(A2, B2);			\
													\
		BITONIC4(O1, O2, l11, l12);					\
		BITONIC4(O3, O4, h11, h12);					\
	} while(0)

/* Bitonic merge kernel for 2 x 4 elements */
#define BITONIC_MERGE4(O1, O2, A, B)		\
	do {									\
		/* reverse the order of input B */	\
		REVERSE(B);							\
		BITONIC4(O1, O2, A, B);				\
	} while(0)

/* Bitonic merge kernel for 2 x 8 elements */
#define BITONIC_MERGE8(O1, O2, O3, O4,		\
					   A1, A2, B1, B2)		\
	do{										\
		int32x4_t l11, l12, h11, h12;		\
		/* reverse the order of input B */	\
		REVERSE(B1); REVERSE(B2);			\
											\
		l11 = vminq_s32(A1, B2);	\
		l12 = vminq_s32(A2, B1);	\
		h11 = vmaxq_s32(A1, B2);	\
		h12 = vmaxq_s32(A2, B1);	\
											\
		BITONIC4(O1, O2, l11, l12);			\
		BITONIC4(O3, O4, h11, h12);			\
	} while(0)


/* Bitonic merge kernel for 2 x 16 elements */
/* TODO implement it */
#define BITONIC_MERGE16(O1, O2, O3, O4, O5, O6, O7, O8,	\
						A1, A2, A3, A4, B1, B2, B3, B4)	\
	do{													\
		int32x4_t l01, l02, l03, l04;	\
		int32x4_t h01, h02, h03, h04;	\
		REVERSE(B1); REVERSE(B2);			\
		REVERSE(B3); REVERSE(B4);			\
											\
		l01 = vminq_s32(A1, B4);	\
		l02 = vminq_s32(A2, B3);	\
		l03 = vminq_s32(A3, B2);	\
		l04 = vminq_s32(A4, B1);	\
											\
		h01 = vmaxq_s32(A1, B4);	\
		h02 = vmaxq_s32(A2, B3);	\
		h03 = vmaxq_s32(A3, B2);	\
		h04 = vmaxq_s32(A4, B1);	\
											\
		BITONIC8(O1, O2, O3, O4, l01, l02, l03, l04);	\
		BITONIC8(O5, O6, O7, O8, h01, h02, h03, h04);	\
	} while(0)

#endif
