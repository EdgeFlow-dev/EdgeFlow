#include "xplane_sort_core.h"
#include "params.h"

/* 3 elements vector inregister sorting */
inline void x3_inregister_sort(int32_t *input, int32_t *output, uint32_t t_idx){
	int32x4x3_t ra = vld3q_s32(input);
	int32x4x3_t rb = vld3q_s32(input + 12);
	int32x4x3_t rc = vld3q_s32(input + 24);
	int32x4x3_t rd = vld3q_s32(input + 36);

	uint32_t i;

	
	/* use t_idx in array for sorting */

	/* 1st level of comparisons */
	x3_cmp_s32x4(&ra, &rb, t_idx);
	x3_cmp_s32x4(&rc, &rd, t_idx);

	/* 2nd level of comparisons */	
	x3_cmp_s32x4(&ra, &rc, t_idx);
	x3_cmp_s32x4(&rb, &rd, t_idx);

	/* 3rd level of comparisons */
	x3_cmp_s32x4(&rb, &rc, t_idx);

	/* results are in ra2, rb3, rc3, rd2 */

	/* shuffle phase */
	for(i = 0; i < 3; i++)
		xplane_transpose4x4(&ra.val[i], &rb.val[i], &rc.val[i], &rd.val[i]);

	vst3q_s32(output, ra);
	vst3q_s32(output + 12, rb);
	vst3q_s32(output + 24, rc);
	vst3q_s32(output + 36, rd);
}

/* recursively merge 4 items with x3 */
void x3_merge4_aligned(int32_t * const inpA, int32_t * const inpB,
						int32_t * const out, const uint32_t lenA, const uint32_t lenB, uint32_t t_idx){
	/* hp: ELES_PER_ITEM * 4 */
	/* len is based on size of x3_t */
	x3x4_t *inA = (x3x4_t *) inpA;
	x3x4_t *inB = (x3x4_t *) inpB;

	x3x4_t * const endA = (x3x4_t *)(((x3_t *) inpA) + lenA);
	x3x4_t * const endB = (x3x4_t *)(((x3_t *) inpB) + lenB);

	x3x4_t *outp = (x3x4_t *) out;
	x3x4_t *next = (x3x4_t *) inB;

	int32x4x3_t regA = vld3q_s32((const int32_t *) inA);
	int32x4x3_t regB = vld3q_s32((const int32_t *) next);

	inA++; inB++;

	x3_bitonic_merge4(&regA, &regB, t_idx);

	vst3q_s32((int32_t *) outp, regA);
	outp++;

	while (inA < endA && inB < endB){
		if(*(((int32_t *)inA) + t_idx) < *(((int32_t *)inB) + t_idx)) {
			next = inA;
			inA ++;
		} else {
			next = inB;
			inB ++;
		}

		regA = regB;
		regB = vld3q_s32((const int32_t *) next);

		x3_bitonic_merge4(&regA, &regB, t_idx);

		vst3q_s32((int32_t *) outp, regA);
		outp ++;
	}


	/* handle reamining items */
	while (inA < endA){
		regA = vld3q_s32((const int32_t *) inA);
		regB = regB;	/*hp needed? */
		
		x3_bitonic_merge4(&regA, &regB, t_idx);
		vst3q_s32((int32_t *) outp, regA);
		inA++; outp++;
	}
	
	while (inB < endB){
		regA = regB;
		regB = vld3q_s32((const int32_t *) inB);
	
		x3_bitonic_merge4(&regA, &regB, t_idx);
		vst3q_s32((int32_t *) outp, regA);
		inB++; outp++;
	}

	vst3q_s32((int32_t *) outp, regB);
}

void x3_merge4_not_aligned(int32_t * restrict inpA, int32_t * restrict inpB, int32_t * restrict Out,
		const uint32_t lenA, const uint32_t lenB, uint32_t t_idx){
	uint32_t lenA4 = lenA & ~0x3, lenB4 = lenB & ~0x3;
	uint32_t ai = 0, bi = 0;

	int32_t *out = Out;

//	uint32_t i;

	if(lenA4 > 4 && lenB4 > 4){

		x3x4_t *inA = (x3x4_t *) inpA;
		x3x4_t *inB = (x3x4_t *) inpB;
		x3x4_t * const endA = ((x3x4_t *)(((x3_t *) inpA) + lenA) - 1);
		x3x4_t * const endB = ((x3x4_t *)(((x3_t *) inpB) + lenB) - 1);

		x3x4_t *outp = (x3x4_t *) out;
		x3x4_t *next = (x3x4_t *) inB;

		int32x4x3_t regA = vld3q_s32((const int32_t *) inA);
		int32x4x3_t regB = vld3q_s32((const int32_t *) next);


		inA++; inB++;

		x3_bitonic_merge4(&regA, &regB, t_idx);
		vst3q_s32((int32_t *) outp, regA);
		outp++;


		while (inA < endA && inB < endB){

			/* 3 Options : normal-if, cmove-with-assembly, sw-predication */
			if(*((int32_t *)inA + t_idx) < *((int32_t *)inB + t_idx)){
				next = inA;
				inA++;
			} else{
				next = inB;
				inB++;
			}

			regA = regB;
			regB = vld3q_s32((const int32_t *) next);

			x3_bitonic_merge4(&regA, &regB, t_idx);
			/* storeing regA (min) */
			vst3q_s32((int32_t *) outp, regA);
			outp++;

		}
		/* flush the register to one of the lists */
		/* first t_idx value of inA is larger than last t_idx value of register */
		if(*((int32_t *)inA + t_idx) >= regB.val[t_idx][3]){
			inA --;
			vst3q_s32((int32_t *) inA, regB);
		} else {
			inB --;
			vst3q_s32((int32_t *) inB, regB);
		}

		ai = ((x3_t *)inA - (x3_t *) inpA); /* # of sorted elements */
		bi = ((x3_t *)inB - (x3_t *) inpB); /* # of sorted elements */

		inpA = (int32_t *)inA;
		inpB = (int32_t *)inB;
		out  = (int32_t *)outp;

	}

	/* serial-merge */	//TODO : performance tuning
	while(ai < lenA && bi < lenB){
		int32_t *in = inpB;
		uint32_t ch = (*(inpA + t_idx) < *(inpB + t_idx));
		uint32_t notch = !ch;

		ai += ch;
		bi += notch;

		if(ch)
			in = inpA;

		/* hp : # of elements is 3 */
//		copy_item((x3_t *)out, (x3_t *)in);
		memcpy(out, in, sizeof(x3_t));

		out += 3;
		inpA += (ch * 3);
		inpB += (notch * 3);
	}

	if(ai < lenA) {

//	accelarate memcopy with NEON but doesn't work due to alignement problem
//	TODO: need to clarify NEON support unaligned memory access

		while(ai < lenA){
//		XMSG("[A] ai : %d, lenA : %d ||||| bi : %d lenB : %d", ai, lenA, bi, lenB);
//		XMSG("[A] out : %p inpA : %p", (void *) out, (void *) inpA);
//		copy_item((x3_t *)out, (x3_t *)inpA);
		memcpy(out, inpA, sizeof(x3_t));
		out += 3; inpA += 3;
		ai++;
		}
	} else if(bi < lenB) {
		/* if B has any more items to be output */

		while(bi < lenB){
			memcpy(out, inpB, sizeof(x3_t));
			out += 3; inpB += 3;
			bi++;
		}
	}

}

