#include <iostream>
#include <iomanip>
#include <cstring>
#include "utils.h"
using namespace std;

void dump_arr_hex(const char name[], uint64_t* arr) {
    cout << "--------------------------" << endl;
    cout << "Array " << name << ":" << endl;
    cout << hex <<  arr[0] << " " << arr[1] << " " << arr[2] << " " << arr[3] << endl;
}

void dump_arr_double(const char name[], double* arr) {
    cout << "--------------------------" << endl;
    cout << "Array " << name << ":" << endl;
    cout << arr[0] << " " << arr[1] << " " << arr[2] << " " << arr[3] << endl;
}

void dump_arr_int64(const char name[], int64_t* arr, int len) {
    int i, j;
    cout << "--------------------------" << endl;
    cout << "Array " << name << ":" << endl;
    for (i = 0; i < len - 16; i += 16) {
        for (j = 0; j < 16; ++j)
            cout << setw(6) << arr[i + j] << " ";
        cout << endl;
    }
    while(i < len) {
        cout << setw(6) << arr[i++] << " ";
    }
    cout << endl;
}

void dump_arr_rec(const char name[], int64_t* arr, unsigned char reclen,
                    unsigned int len) {
    unsigned int i, j;
    cout << "--------------------------" << endl;
    cout << "Array " << name << ":" << endl;
    for (i = 0; i < len; i += reclen) {
        for (j = 0; j < reclen; ++j)
            cout << setw(6) << arr[i + j] << " ";
        cout << endl;
    }
}

/*
   @in: input array of records
   @key: output array of keys
   @val: output array of ptrs to each record
   @keypos: key position in each record
   @reclen: record length in # of simd_t
   @nitems: # of records to be converted

   Convert records to <key, ptr> format
*/
void kv_split(int64_t *in, int64_t *key, int64_t *val, idx_t keypos,
              idx_t reclen, uint32_t nitems)
{
    uint32_t i;
    for(i = 0; i < nitems; ++i) {
        *key++ = in[i * reclen + keypos];
        *val++ = (int64_t)&in[i * reclen];
    }
}

/*
   @out: output array
   @ptr: ptr array
   @reclen: record length in # of simd_t
   @nitems: # of ptrs to be converted

   Convert <key, ptr> to record format
*/
void kv_merge(int64_t *out, int64_t *ptr, idx_t reclen,
              uint32_t nitems)
{
    uint32_t i;
    for(i = 0; i < nitems; ++i) {
        memcpy(&out[i * reclen], (simd_t*)ptr[i],
               reclen * sizeof(simd_t));
    }
}
