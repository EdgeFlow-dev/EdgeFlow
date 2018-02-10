/* Author: Hongyu Miao @ Purdue ECE
 * Date: Nov 9th, 2017 
 * Description: This file provides test code for functions in x2_bitonix_sort_merge_core.h
 * Compile: g++ -O3 -std=c++11 -mavx512f -mavx512pf -mavx512er -mavx512cd test_x2_bitonic_sort_merge_core.cpp -o test_x2_bitonic_sort_merge_core
 * */
#include "x2_bitonic_sort_merge_core.h"
#include <iostream>

void test_x2_cmp_pd(){
	__m512d k1;
	__m512d ptr1;
	__m512d k2;
	__m512d ptr2;
	__m512d min_k;
	__m512d min_ptr;
	__m512d max_k;
	__m512d max_ptr;

	int64_t input_k1[8] = {0, 3, 4, 7, 8, 11, 12, 15};
	int64_t input_ptr1[8] = {0, 3, 4, 7, 8, 11, 12, 15};
	int64_t input_k2[8] = {0, 3, 4, 6, 9, 10, 13, 14};
	int64_t input_ptr2[8] = {1, 2, 5, 6, 9, 10, 13, 14};
	
	int64_t output_min_k[8];
	int64_t output_min_ptr[8];
	int64_t output_max_k[8];
	int64_t output_max_ptr[8];

	
	std::cout << "test_x2_cmp_pd: " << std::endl;

	k1 = _mm512_loadu_pd(input_k1);
	ptr1 = _mm512_loadu_pd(input_ptr1);
	k2 = _mm512_loadu_pd(input_k2);
	ptr2 = _mm512_loadu_pd(input_ptr2);
	
	x2_cmp_pd(k1, ptr1, k2, ptr2, min_k, min_ptr, max_k, max_ptr);
	_mm512_storeu_pd(output_min_k, min_k);
	_mm512_storeu_pd(output_min_ptr, min_ptr);
	_mm512_storeu_pd(output_max_k, max_k);
	_mm512_storeu_pd(output_max_ptr, max_ptr);

	std::cout << "min_k: " << std::endl;
	for(int i = 0; i < 8; i++){
		std::cout << output_min_k[i] << ",";
	}
	std::cout << std::endl;

	std::cout << "min_ptr: " << std::endl;
	for(int i = 0; i < 8; i++){
		std::cout << output_min_ptr[i] << ",";
	}
	std::cout << std::endl;
	std::cout << std::endl;

	std::cout << "max_k: " << std::endl;
	for(int i = 0; i < 8; i++){
		std::cout << output_max_k[i] << ",";
	}
	std::cout << std::endl;
	
	std::cout << "max_ptr: " << std::endl;
	for(int i = 0; i < 8; i++){
		std::cout << output_max_ptr[i] << ",";
	}
	std::cout << std::endl;
	std::cout << std::endl;
}

void test_x2_bitonic_sort_avx512_1x8(){
	int64_t input_k[8] = {83, 86, 77, 15, 93, 35, 86, 92};
	int64_t input_ptr[8] = {49, 21,62, 27, 90, 59, 63, 26};

	std::cout << "test_x2_bitonic_sort_avx512_1x8: " << std::endl;

	std::cout << "input_k: " << std::endl;
	for(int i = 0; i < 8; i++){
		//input_k[i] = rand() % 100;
		std::cout << input_k[i] << ", ";
	}
	std::cout << std::endl;	
	
	std::cout << "input_ptr: " << std::endl;
	for(int i = 0; i < 8; i++){
		//input_ptr[i] = rand() % 100;
		std::cout << input_ptr[i] << ", ";
	}
	std::cout << std::endl;
	std::cout << std::endl;

	//CoreSmallSort_kptr(input_k, input_ptr);
	x2_bitonic_sort_avx512_1x8(input_k, input_ptr);

	std::cout << "sorted input_k: " << std::endl;
	for(int i = 0; i < 8; i++){
		std::cout << input_k[i] << ", ";
	}
	std::cout << std::endl;	
	
	std::cout << "corresponding input_ptr: " << std::endl;
	for(int i = 0; i < 8; i++){
		std::cout << input_ptr[i] << ", ";
	}
	std::cout << std::endl;
}


void test_x2_bitonic_merge_avx512_2x8(){
	int64_t in1_k[8] = {1, 3, 5, 7, 9, 11, 13, 15};
	//int64_t in1_k[8] = {1, 1, 1, 1, 1, 1, 1, 1};
	int64_t in1_ptr[8] = {1, 3, 5, 7, 9, 11, 13, 15};
	int64_t in2_k[8] = {0, 2, 4, 6, 8, 10, 12, 14};
	//int64_t in2_k[8] = {1, 1, 1, 1, 1, 1, 1, 1};
	int64_t in2_ptr[8] = {0, 2, 4, 6, 8, 10, 12, 14};
	
	std::cout << "test_x2_bitonic_merge_avx512_2x8: " << std::endl;

	std::cout << "in1_k: " << std::endl;
	for(int i = 0; i < 8; i++){
		std::cout << in1_k[i] << ", ";
	}
	std::cout << std::endl;

	std::cout << "in1_ptr: " << std::endl;
	for(int i = 0; i < 8; i++){
		std::cout << in1_ptr[i] << ", ";
	}
	std::cout << std::endl;
	std::cout << std::endl;
	
	std::cout << "in2_k: " << std::endl;
	for(int i = 0; i < 8; i++){
		std::cout << in2_k[i] << ", ";
	}
	std::cout << std::endl;

	std::cout << "in2_ptr: " << std::endl;
	for(int i = 0; i < 8; i++){
		std::cout << in2_ptr[i] << ", ";
	}
	std::cout << std::endl;
	std::cout << std::endl;
	
	x2_bitonic_merge_avx512_2x8(in1_k, in1_ptr, in2_k, in2_ptr, in1_k, in1_ptr, in2_k, in2_ptr);

	std::cout << "after merge smallest 8 items: " << std::endl;
	for(int i = 0; i < 8; i++){
		std::cout << in1_k[i] << ", ";
	}
	std::cout << std::endl;

	std::cout << "corresponsing ptr:: " << std::endl;
	for(int i = 0; i < 8; i++){
		std::cout << in1_ptr[i] << ", ";
	}
	std::cout << std::endl;
	std::cout << std::endl;
	
	std::cout << "after merge another 8 items: " << std::endl;
	for(int i = 0; i < 8; i++){
		std::cout << in2_k[i] << ", ";
	}
	std::cout << std::endl;

	std::cout << "corresponding ptr: " << std::endl;
	for(int i = 0; i < 8; i++){
		std::cout << in2_ptr[i] << ", ";
	}
	std::cout << std::endl;
}

void test_x2_bitonic_sort_avx512_8x8(){
	int64_t k[64];
	int64_t ptr[64];
	
	std::cout << "test_x2_bitonic_sort_avx512_8x8: " << std::endl;

	std::cout << "before sort <k,ptr>: " << std::endl;
	for(int i = 0; i < 64; i++){
		k[i] = rand() % 100;;
		ptr[i] = k[i];
		std::cout << "<" << k[i] << "," << ptr[i] << ">,";
	}
	std::cout << std::endl;
	
	x2_bitonic_sort_avx512_8x8(k, ptr);

	std::cout << std::endl << "after sort <k,prt>: sorted 8-aligned blocks" << std::endl;
	for(int i = 0; i < 8; i++){
		for(int j =0; j < 8; j++){
			std::cout << "<" << k[i*8+j] << "," << ptr[i*8+j] << ">,";
		}
		std::cout << std::endl;
	}
	std::cout << std::endl;
}

int main(){
	std::cout << "------------------------------------" << std::endl;
	test_x2_cmp_pd();
	
	std::cout << "------------------------------------" << std::endl;
	test_x2_bitonic_sort_avx512_1x8();
	
	std::cout << "------------------------------------" << std::endl;
	test_x2_bitonic_merge_avx512_2x8();
	
	std::cout << "------------------------------------" << std::endl;
	test_x2_bitonic_sort_avx512_8x8();
	
	return 0;
}


