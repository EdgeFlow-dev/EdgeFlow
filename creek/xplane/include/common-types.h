#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <stdint.h>

#define NUM_ARGS		4
#define TEE_NUM_PARAMS	4

#define TEE_SUCCESS 	1

typedef union {
	struct {
		void *buffer;
		uint32_t size;
	} memref;
	struct {
		uint32_t a;
		uint32_t b;
	} value;
} x_arg;

typedef x_arg TEE_Param;
typedef struct timespec TEE_Time;
typedef int32_t TEE_Result;

#define XMSG(MSG, ...)
#define X1(MSG, ...)
#define X2(MSG, ...)
#define EMSG(MSG, ...)
#define DMSG(MSG, ...)

#endif
