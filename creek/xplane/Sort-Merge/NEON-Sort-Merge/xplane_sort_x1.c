#include "xplane/xplane_sort_core.h"
#include "xplane/params.h"

/* 1 elements vector inregister sorting */
//inline __attribute__((always_inline))
void x1_inregister_sort(int32_t *input, int32_t *output, __attribute__((unused)) uint32_t cmp){

	int32x4_t ra = vld1q_s32((int32_t const *)(input));
	int32x4_t rb = vld1q_s32((int32_t const *)(input + 4));
	int32x4_t rc = vld1q_s32((int32_t const *)(input + 8));
	int32x4_t rd = vld1q_s32((int32_t const *)(input + 12));

	int32x4_t ra1, rb1, rc1, rd1;
	int32x4_t ra2, rc2, rd2;
	int32x4_t rb3, rc3, ra3, rd3;
	int32x4_t ra4, rb4, rc4;
	int32x4_t rb5, rc5;

	/* odd-even sorting network begins */
	/* 1st level of comparisons */
	ra1 = vminq_s32(ra, rb);
	rb1 = vmaxq_s32(ra, rb);

	rc1 = vminq_s32(rc, rd);
	rd1 = vmaxq_s32(rc, rd);

	/* 2nd level of comparisons */
	rb = vminq_s32(rb1, rd1);
	rd = vmaxq_s32(rb1, rd1);

	/* 3rd level of comparisons */
	ra2 = vminq_s32(ra1, rc1);
	rc2 = vmaxq_s32(ra1, rc1);

	/* 4th level of comparisons */
	rb3 = vminq_s32(rb, rc2);
	rc3 = vmaxq_s32(rb, rc2);

	/* results are in ra2, rb3, rc3, rd */
	/* Initial data and transposed data looks like following:
	 * a2={ a1 b1 c1 d1 }                       a4={ a1 a2 a3 a4 }
	 * b3={ a2 b2 c2 d2 }  === transpose ==>    b5={ b1 b2 b3 b4 }
	 * c3={ a3 b3 c3 d3 }                       c5={ c1 c2 c3 c4 }
	 *  d={ a4 b4 c4 d4 }                       d4={ d1 d2 d3 d4 }
	 */

	/* shuffle phase */

	/* a3={ a1 a3 b1 b3 }
	 * b4={ c1 c3 d1 d3 } */
	ra3 = vzip1q_s32(ra2, rc3);
	rb4 = vzip2q_s32(ra2, rc3);

	/* c4={ a2 a4 b2 b4 }
	 * d2={ c2 c4 d2 d4 } */
	rc4 = vzip1q_s32(rb3, rd);
	rd2 = vzip2q_s32(rb3, rd);

	/* a4={ a1 a2 a3 a4 }
	 * b5={ b1 b2 b3 b4 } */
	ra4 = vzip1q_s32(ra3, rc4);
	rb5 = vzip2q_s32(ra3, rc4);

	/* c5={ c1 c2 c3 c4 }
	 * d3={ d1 d2 d3 d4 } */
	rc5 = vzip1q_s32(rb4, rd2);
	rd3 = vzip2q_s32(rb4, rd2);

	/* store eavh value to correspobnding registers */
	vst1q_s32((int32_t *) output, ra4);
	vst1q_s32((int32_t *) (output + 4), rb5);
	vst1q_s32((int32_t *) (output + 8), rc5);
	vst1q_s32((int32_t *) (output + 12), rd3);
}

/* recursively merge 4 items with x1 */
//inline __attribute__((always_inline))
void x1_merge4_eqlen_aligned(int32_t * const inpA, int32_t * const inpB,
		int32_t * const out, const uint32_t lenA, const uint32_t lenB, __attribute__((unused)) uint32_t cmp)
{
	register x1x4_t *inA    = (x1x4_t *) inpA;
	register x1x4_t *inB    = (x1x4_t *) inpB;
	x1x4_t * const endA  = (x1x4_t *) (inpA + lenA);
	x1x4_t * const endB  = (x1x4_t *) (inpB + lenB);

	x1x4_t *outp = (x1x4_t *) out;

	register x1x4_t *next = inB;

	register int32x4_t outreg1;
	register int32x4_t outreg2;

	register int32x4_t regA = vld1q_s32((int32_t const *) inA);
	register int32x4_t regB = vld1q_s32((int32_t const *) next);

	inA ++;
	inB ++;

	BITONIC_MERGE4(outreg1, outreg2, regA, regB);
	/* store outreg1 */
	vst1q_s32((int32_t *) outp, outreg1);
	outp++;

	while (inA < endA && inB < endB){
		/* hj: need to check */
		/* Options : normal-if, cmove-with-assembly, sw-predication */
		/* IFELSECONDMOVE(next, inA, inB, 32); */
		/* hj: use code as follows instead of IFSECONDMOVE */
		if(*((int32_t *)inA) < *((int32_t *)inB)){
			next = inA;
			inA++;
		} else{
			next = inB;
			inB++;
		}

		regA = outreg2;
		regB = vld1q_s32((int32_t const *) next);

		BITONIC_MERGE4(outreg1, outreg2, regA, regB);

		/* storeing outreg1 */
		vst1q_s32((int32_t *) outp, outreg1);
		outp++;
	}


	/* handle remaining items */
	while (inA < endA) {
		regA = vld1q_s32((int32_t const *) inA);
		regB = outreg2;

		BITONIC_MERGE4(outreg1, outreg2, regA, regB);

		vst1q_s32((int32_t *) outp, outreg1);
		inA++;
		outp++;
	}

	while (inB < endB) {
		regA = outreg2;
		regB = vld1q_s32((int32_t const *) inB);

		BITONIC_MERGE4(outreg1, outreg2, regA, regB);

		vst1q_s32((int32_t *) outp, outreg1);
		inB++;
		outp++;
	}

	vst1q_s32((int32_t *) outp, outreg2);
}



/** Merge two sorted arrays to a final output using 8-way neon bitonic merge.
 *
 * @param inpA input array A
 * @param inpB input array B
 * @param Out output array
 * @param lenA size of A
 * @param lenB size of B
 **/

//inline __attribute__((always_inline))
void x1_merge8_varlen_aligned(int32_t * restrict inpA, int32_t * restrict inpB, int32_t * restrict Out,
		const uint32_t lenA, const uint32_t lenB, __attribute__((unused)) uint32_t cmp)
{
	uint32_t lenA8 = lenA & ~0x7, lenB8 = lenB & ~0x7;
	uint32_t ai = 0, bi = 0;

	int32_t *out = Out;

	int32_t hireg[4];       // hj: problem was solved

	if(lenA8 > 8 && lenB8 > 8){
		register block8 *inA    = (block8*) inpA;
		register block8 *inB    = (block8*) inpB;
		block8 * const  endA    = (block8*) (inpA + lenA) - 1;
		block8 * const  endB    = (block8*) (inpB + lenB) - 1;

		block8 *outp = (block8*) out;

		register block8 *next = inB;

		register int32x4_t outreg1l, outreg1h;
		register int32x4_t outreg2l, outreg2h;

		register int32x4_t regAl, regAh;
		register int32x4_t regBl, regBh;

		LOAD8(regAl, regAh, inA);
		LOAD8(regBl, regBh, next);

		//      XMSG("[start] inA : %p inB : %p", (void *) inA, (void *) inB);
		inA ++;
		inB ++;

		BITONIC_MERGE8(outreg1l, outreg1h, outreg2l, outreg2h,
				regAl, regAh, regBl, regBh);


		/* store outreg1 */
		STORE8(outp, outreg1l, outreg1h);
		outp++;
		while (inA < endA && inB < endB){
			/* 3 Options : normal-if, cmove-with-assembly, sw-predication */
			if(*((int32_t *)inA) < *((int32_t *)inB)){
				next = inA;
				inA++;
			} else{
				next = inB;
				inB++;
				//          if(inB > (block8 *) 0x600401ff00)
				//              XMSG("[aaaa] add or inA : %p inB : %p", (void *) inA, (void *) inB);
			}
			regAl = outreg2l;
			regAh = outreg2h;
			LOAD8(regBl, regBh, next);

			BITONIC_MERGE8(outreg1l, outreg1h, outreg2l, outreg2h,
					regAl, regAh, regBl, regBh);

			/* storeing outreg1 */
			STORE8(outp, outreg1l, outreg1h);
			outp++;

		}

		/* flush the register to one of the lists */
		//      int32x4_t* hireg;
		//      int32_t hireg[4];       // hj: problem was solved
		vst1q_s32(hireg, outreg2h);
	
		if(*((int32_t *)inA) >= *(int32_t *)(hireg+3)){
			inA --;
			STORE8(inA, outreg2l, outreg2h);
		} else {
			inB --;
			STORE8(inB, outreg2l, outreg2h);
		}

		ai = ((int32_t *)inA - inpA); /* # of sorted element */
		bi = ((int32_t *)inB - inpB);

		inpA = (int32_t *)inA;
		inpB = (int32_t *)inB;
		out  = (int32_t *)outp;

		//      XMSG("[b] inA : %p inB : %p", (void *) inA, (void *) inB);
	}

	/* serial-merge */  //TODO : performance tuning
	while(ai < lenA && bi < lenB){
		int32_t *in = inpB;
		uint32_t ch = (*inpA < *inpB);
		uint32_t notch = !ch;

		//      XMSG("Serial ai : %d, lenA : %d ||||| bi : %d lenB : %d", ai, lenA, bi, lenB);
		ai += ch;
		bi += notch;

		if(ch)
			in = inpA;
		*out = *in;
		out++;

		inpA += ch;
		inpB += notch;
		//      if(inpA > (int32_t *) 0x600003ff00)
		//          XMSG("[ccccc] ai : %d bi : %d inA : %p inB : %p", ai, bi, (void *) inpA, (void *) inpB);
	}

	if(ai < lenA) {
		//  accelarate memcopy with NEON but doesn't work due to alignement problem
		//  TODO: need to clarify NEON support unaligned memory access
		while(ai < lenA){
			*out = *inpA;
			ai++;
			out++;
			inpA++;
		}
	} else if(bi < lenB) {
		/* if B has any more items to be output */
		while(bi < lenB){
			*out = *inpB;
			bi++;
			out++;
			inpB++;
		}
	}
}
