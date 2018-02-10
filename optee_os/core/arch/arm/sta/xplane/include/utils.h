#ifndef UTILS_H
#define UTILS_H

#include "type.h"
#include <kernel/tee_time.h>
#include <trace.h>


/********************************
 *		Useful utils			*
 ********************************/

uint32_t bitcnt(uint32_t n);
int32_t x_pow(int32_t p, int32_t n);
int32_t is_power_of_two(uint32_t x);
uint32_t my_log2(uint32_t x);
uint32_t ceil_log2(uint32_t x);

void arr_dump(int32_t *arr, int32_t len);

int32_t is_sorted(int32_t *arr, int32_t len);
int32_t x_is_sorted(int32_t *arr, int32_t len, uint32_t cmp, idx_t reclen);

int32_t generate_input(tuple_t * tuples, int32_t ntuples);

static unsigned long int r_next = 1;

static inline int32_t rand(void)	// RAND_MAX assumed to be 32767
{
	r_next = r_next * 1103515245 + 12345;
	return (unsigned int)(r_next/65536) % 32768;
}

static inline void srand(uint32_t seed)
{
	r_next = seed;
}

static inline unsigned int lfsr113_Bits (void)
{
	static unsigned int z1 = 12345, z2 = 23456, z3 = 34567, z4 = 45678;
	unsigned int b;
	b  = ((z1 << 6) ^ z1) >> 13;
	z1 = ((z1 & 4294967294U) << 18) ^ b;
	b  = ((z2 << 2) ^ z2) >> 27; 
	z2 = ((z2 & 4294967288U) << 2) ^ b;
	b  = ((z3 << 13) ^ z3) >> 21;
	z3 = ((z3 & 4294967280U) << 7) ^ b;
	b  = ((z4 << 3) ^ z4) >> 12;
	z4 = ((z4 & 4294967168U) << 13) ^ b;
	return (z1 ^ z2 ^ z3 ^ z4);
}


/********************************
 *		Time measurement		*
 ********************************/
//static inline uint32_t tee_time_to_ms(TEE_Time t);
//static inline uint32_t get_delta_time_in_ms(TEE_Time start, TEE_Time stop);


static inline uint32_t tee_time_to_ms(TEE_Time t)
{
	return t.seconds * 1000 + t.millis;
}

static inline uint32_t get_delta_time_in_ms(TEE_Time start, TEE_Time stop){
	return tee_time_to_ms(stop) - tee_time_to_ms(start);
}

#endif
