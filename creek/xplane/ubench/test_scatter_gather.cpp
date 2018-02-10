/*
 * hym: example of using gather/scatter
 * compile: g++ -std=c++11 -mavx512f -mavx512pf -mavx512er -mavx512cd test_scatter_gather.cpp -o test_scatter_gather
 * date: Jan 2 2018
 * */

#include "immintrin.h"
#include <iostream>

#define N 16
void print(int64_t* data){
	std::cout << "print []: " << std::endl;
	for(int i = 0; i < N; i++){
		std::cout << data[i] << ","; 
	}
	std::cout << std::endl;
}

void test_scatter_gather(){
	int64_t * src_data = (int64_t *) malloc(sizeof(int64_t) * N);
	int64_t * dst_data = (int64_t *) malloc(sizeof(int64_t) * N);
	
	//init data
	for(int i = 0; i < N; i++){
		src_data[i] = i;
	}
	
	//gather data
	__m512i ld_idx = _mm512_set_epi64(0, 2, 4, 6, 8, 10, 12, 14);
	__m512d ld_data = _mm512_i64gather_pd(ld_idx, src_data, 8);

	//scatter data
	__m512i st_idx = _mm512_set_epi64(0, 1, 2, 3, 4, 5, 6, 7);
	_mm512_i64scatter_pd(dst_data, st_idx, ld_data, 8);		
	
	print(dst_data);
}

int main(){
	test_scatter_gather();
	return 0;
}

