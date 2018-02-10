#include "xplane_sort_core.h"
#include "params.h"

/* 4 elements vector inregister sorting */
inline void x4_inregister_sort(int32_t *input, int32_t *output, uint32_t t_idx){
	int32x4x4_t ra = vld4q_s32(input);
	int32x4x4_t rb = vld4q_s32(input + 16);
	int32x4x4_t rc = vld4q_s32(input + 32);
	int32x4x4_t rd = vld4q_s32(input + 48);

	uint32_t i;

	/* use cmp in array for sorting */

	/* 1st level of comparisons */
	x4_cmp_s32x4(&ra, &rb, t_idx);
	x4_cmp_s32x4(&rc, &rd, t_idx);

	/* 2nd level of comparisons */	
	x4_cmp_s32x4(&ra, &rc, t_idx);
	x4_cmp_s32x4(&rb, &rd, t_idx);

	/* 3rd level of comparisons */
	x4_cmp_s32x4(&rb, &rc, t_idx);

	/* results are in ra2, rb3, rc3, rd2 */

	/* shuffle phase */
	for(i = 0; i < 4; i++)
		xplane_transpose4x4(&ra.val[i], &rb.val[i], &rc.val[i], &rd.val[i]);
	
	vst4q_s32(output, ra);
	vst4q_s32(output + 16, rb);
	vst4q_s32(output + 32, rc);
	vst4q_s32(output + 48, rd);
}

/* recursively merge 4 items with x3 */
void x4_merge4_aligned(int32_t * const inpA, int32_t * const inpB,
							int32_t * const out, const uint32_t lenA, const uint32_t lenB, uint32_t t_idx){
	/* hp: ELES_PER_ITEM * 4 */
	/* len is based on size of x4_t */
	x4x4_t *inA = (x4x4_t *) inpA;
	x4x4_t *inB = (x4x4_t *) inpB;

	x4x4_t * const endA = (x4x4_t *)(((x4_t *) inpA) + lenA);
	x4x4_t * const endB = (x4x4_t *)(((x4_t *) inpB) + lenB);

	x4x4_t *outp = (x4x4_t *) out;
	x4x4_t *next = (x4x4_t *) inB;

	int32x4x4_t regA = vld4q_s32((const int32_t *) inA);
	int32x4x4_t regB = vld4q_s32((const int32_t *) next);

	inA++; inB++;

	x4_bitonic_merge4(&regA, &regB, t_idx);

	vst4q_s32((int32_t *) outp, regA);
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
		regB = vld4q_s32((const int32_t *) next);

		x4_bitonic_merge4(&regA, &regB, t_idx);

		vst4q_s32((int32_t *) outp, regA);
		outp ++;
	}

	/* handle reamining items */
	while (inA < endA){
		regA = vld4q_s32((const int32_t *) inA);
		regB = regB;	/*hp needed? */
		
		x4_bitonic_merge4(&regA, &regB, t_idx);
		vst4q_s32((int32_t *) outp, regA);
		inA++; outp++;
	}
	
	while (inB < endB){
		regA = regB;
		regB = vld4q_s32((const int32_t *) inB);
	
		x4_bitonic_merge4(&regA, &regB, t_idx);
		vst4q_s32((int32_t *) outp, regA);
		inB++; outp++;
	}


	vst4q_s32((int32_t *) outp, regB);

}

void x4_merge4_not_aligned(int32_t * restrict inpA, int32_t * restrict inpB, int32_t * restrict Out,
		const uint32_t lenA, const uint32_t lenB, uint32_t t_idx){
	uint32_t lenA4 = lenA & ~0x3, lenB4 = lenB & ~0x3;
	uint32_t ai = 0, bi = 0;

	int32_t *out = Out;

//	uint32_t i;
//	int32_t hireg[4];		// hj: problem was solved

	if(lenA4 > 4 && lenB4 > 4){

		x4x4_t *inA = (x4x4_t *) inpA;
		x4x4_t *inB = (x4x4_t *) inpB;
		x4x4_t * const endA = ((x4x4_t *)(((x4_t *) inpA) + lenA) - 1);
		x4x4_t * const endB = ((x4x4_t *)(((x4_t *) inpB) + lenB) - 1);

		x4x4_t *outp = (x4x4_t *) out;
		x4x4_t *next = (x4x4_t *) inB;

		int32x4x4_t regA = vld4q_s32((const int32_t *) inA);
		int32x4x4_t regB = vld4q_s32((const int32_t *) next);


		inA++; inB++;

		x4_bitonic_merge4(&regA, &regB, t_idx);
		vst4q_s32((int32_t *) outp, regA);
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
			regB = vld4q_s32((const int32_t *) next);

			x4_bitonic_merge4(&regA, &regB, t_idx);
			/* storeing regA (min) */
			vst4q_s32((int32_t *) outp, regA);
			outp++;

		}
		/* flush the register to one of the lists */
//		vst1q_s32(hireg, outreg2h);

		if(*((int32_t *)inA + t_idx) >= regB.val[t_idx][3]){
			inA --;
			vst4q_s32((int32_t *) inA, regB);
		} else {
			inB --;
			vst4q_s32((int32_t *) inB, regB);
		}

		ai = ((x4_t *)inA - (x4_t *) inpA); /* # of sorted elements */
		bi = ((x4_t *)inB - (x4_t *) inpB); /* # of sorted elements */

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
		
//		*(x4_t *)out++ = *(x4_t *)in++;	
//		copy_item((x4_t *)out, (x4_t *)in);
		memcpy(out, in, sizeof(x4_t));

		out += 4;
		inpA += (ch * 4);
		inpB += (notch * 4);
	}

	if(ai < lenA) {

//	accelarate memcopy with NEON but doesn't work due to alignement problem
//	TODO: need to clarify NEON support unaligned memory access
		while(ai < lenA){
//		copy_item((x4_t *)out, (x4_t *)inpA);
		memcpy(out, inpA, sizeof(x4_t));
		out += 4; inpA += 4;
		ai++;
		}
	} else if(bi < lenB) {
		/* if B has any more items to be output */

		while(bi < lenB){
			memcpy(out, inpB, sizeof(x4_t));
			out += 4; inpB += 4;
			bi++;
		}
	}

}

