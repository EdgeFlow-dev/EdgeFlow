#ifndef UTILS_H
#define UTILS_H

#include <inttypes.h>
#include <xplane_lib.h>

void dump_arr_hex(const char name[], uint64_t* arr);
void dump_arr_double(const char name[], double* arr);
void dump_arr_int64(const char name[], int64_t* arr, int len);
void dump_arr_rec(const char name[], int64_t* arr, unsigned char reclen,
                  unsigned int len);

void kv_split(int64_t *in, int64_t *key, int64_t *val, idx_t keypos,
              idx_t reclen, uint32_t nitems);
void kv_merge(int64_t *out, int64_t *key, idx_t reclen,
              uint32_t nitems);

#endif
