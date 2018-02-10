/*
 * Copyright (c) 2016, Linaro Limited
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
 *
 *
 * xzl: moved out of tee/xplane_lib/host/

 BUILD

 /shared/hikey/toolchains/aarch64/bin/aarch64-linux-gnu-gcc \
-c xplane_test.c \
-o xplane_test.o \
-I/home/xzl/tee/xplane_lib/host/include \
-I/home/xzl/tee/xplane_lib/ta/include \
-I/home/xzl/tee/optee_client/out/export/include

/shared/hikey/toolchains/aarch64/bin/aarch64-linux-gnu-gcc \
xplane_test.o -lxplane -lteec -lpthread \
-L/home/xzl/tee/xplane_lib/host \
-L/home/xzl/tee/optee_client/out/export/lib \
-o xplane_test

 */

#include <err.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* OP-TEE TEE client API (built by optee_client) */
#include <tee_client_api.h>

/* To the the UUID (found the the TA's h-file(s)) */
//#include <hello_world_ta.h>

#include "xplane_lib.h"

#define MAX_NUM_THREADS 8

int main(int argc, char *argv[])
{
	x_addr src1;
	uint32_t n_segs;
	uint32_t n_outputs;

	x_addr *dest1;
	x_addr *dest2;
	x_addr *dest3;

//	x_addr **sorted_ua;

	FILE *fp;

	/* randomly generated input which has 1000K items (composed of 3 elements) */
	fp = fopen("input_x3.txt","r");	/* input file for 3 elements in item */
	if (!fp){
		printf("file open failed errno : %p\n", fp);
		return 0;
	} 
	
	open_context();
	populate_map();


//	src1 = create_uarray_file(fp, 16 * 1024 * 1024 /* # of elements */  * 3 /* # of elements in item */);
//	src1 = create_uarray_file(fp, 8 * 1024 * 1024 /* # of elements */  * 3 /* # of elements in item */);
	src1 = create_uarray_file(fp, 8 * 1024 * 1024 /* # of elements */  * ELES_PER_ITEM /* # of elements in item */);
	printf("return uid : %lx\n", src1);

	sort(&dest2, 1, src1);
	
//	n_outputs = 1;
//	segment(&dest1, &n_segs, n_outputs, src1, 0 /* base */, 4 * 1024 * 1024 /* subrange */);
//	printf("n_segs = %d\n", n_segs);	/* n_segs is out */


#if 0
//	multi_sort(&sorted_ua, 1 /* n_outputs */, dest1, n_segs);
	for(int i = 0; i < n_segs * n_outputs; i++){
		sort(&dest2, 1, dest1[i]);
		for(int j = 0; j< 1; j++){
			filter_band(&dest3, 1, dest2[j], 0, 100);
		}
	}
#endif

	kill_map();	
	close_context();
}



