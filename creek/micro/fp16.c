/* test ARM fp16 intrinsics (poorly documented
 *
 *
 * https://gcc.gnu.org/onlinedocs/gcc/Half-Precision.html
 *
 * xzl Sept 2017
 *
 * /shared/hikey/toolchains/aarch64/bin/aarch64-linux-gnu-gcc \-c fp16.c -march=armv8-a -o fp16
 *
 * calling into intrinsics will fail, which seem to expect "-mfpu=fp-armv8", but gcc aarch64 does not
 * support such an option?
 * */

#include <stdio.h>
#include <arm_fp16.h>

int main(int argc, char **argv)
{
	float16_t f1 = 999.0, f2 = 2.0, f3;
	unsigned int k = 1000;
	float x;

//	f3 = vaddh_f16(f1, f2);   // failed?
	f1 = (float16_t)k;

	f3 = f1 + f2;
	x = f3;
	printf("sizeof(float16_t) %u, x is %f\n", sizeof(float16_t), x);
}