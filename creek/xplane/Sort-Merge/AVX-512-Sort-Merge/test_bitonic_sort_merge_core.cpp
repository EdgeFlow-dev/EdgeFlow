/* Author: Hongyu Miao @ Purdue ECE
 * Date: Nov 9th, 2017 
 * Description: This file provides test code for functions in bitonix_sort_merge_core.h 
 * Compile: g++ -std=c++11 -O3 -o test_bitonic_sort_merge_core test_bitonic_sort_merge_core.cpp bitonic_sort_merge_core.cpp -mavx512f -mavx512cd -mavx512er -mavx512pf 
 * */

#include <iostream>
#include "bitonic_sort_merge_core.h"

void test_bitonic_sort_avx512_1x8(){
	int64_t input[8];
	int64_t output[8];
        std::cout << "test_bitonic_sort_avx512_1x8: " << std::endl;	
	std::cout << "before sort: " << std::endl;
	for(int i = 0; i < 8; i++){
		input[i] = rand() % 100;;
		std::cout << input[i] << ", ";
	}
	std::cout << std::endl;
	
	bitonic_sort_avx512_1x8(input, output);
	
	std::cout << "after sort: " << std::endl;
	for(int i = 0; i < 8; i++){
		std::cout << output[i] << ", ";
	}
	std::cout << std::endl;
}

void test_bitonic_sort_avx512_8x8(){
	int64_t input[64];
	int64_t output[64];
	
	std::cout << "test_bitonic_sort_avx512_8x8: " << std::endl;

	std::cout << "before sort: " << std::endl;
	for(int i = 0; i < 64; i++){
		input[i] = rand() % 100;;
		std::cout << input[i] << ", ";
	}
	std::cout << std::endl;
	
	bitonic_sort_avx512_8x8(input, output);

	std::cout << "after sort: get 8 sorted blocks, each block has 8 values " << std::endl;
	for(int i = 0; i < 8; i++){
		for(int j =0; j < 8; j++){
			std::cout << output[i*8 + j] << ", ";
		}
		std::cout << std::endl;
	}
}

void test_bitonic_merge_avx512_2x8(){
	int64_t input1[8];
	int64_t input2[8];
	int64_t output[16];
	
	std::cout << "test_bitonic_merge_avx512_2x8: " << std::endl;

	std::cout << "input1: " << std::endl;
	for(int i = 0; i < 8; i++){
		input1[i] = i * 1;
		std::cout << input1[i] << ", ";
	}
	std::cout << std::endl;
	
	std::cout << "input2: " << std::endl;
	for(int i = 0; i < 8; i++){
		input2[i] = i * 2;
		std::cout << input2[i] << ", ";
	}
	std::cout << std::endl;
	
	bitonic_merge_avx512_2x8(input1, input2, output, output+8);

	std::cout << "merged output: " << std::endl;
	for(int i = 0; i < 16; i++){
		std::cout << output[i] << ", ";
	}
	std::cout << std::endl;
}

int verify(int64_t *mat) {
    int i,j;
    int error = 0;
    for(i=0; i<8; i++) {
      for(j=0; j<8; j++) {
        if(mat[j*8+i] != 1.0f*i*8+j) error++;
      }
    }
    return error;
}

void print_mat(int64_t *mat) {
    int i,j;
    for(i=0; i<8; i++) {
      for(j=0; j<8; j++) 
      	std::cout << mat[i*8+j] << ",";
      std::cout << std::endl;
    }
}

void test_transpose_8x8(){
    int i,j, rep;
    int64_t mat[64];
    int64_t matT[64];
    double dtime;
    std::cout << "test_transpose_8x8: " << std::endl; 
    rep = 10000000;
    for(i=0; i<64; i++) mat[i] = i;
    print_mat(mat);

    transpose_8x8(mat,matT);
    std::cout << "after transpose: error " << verify(matT) << std::endl;
    print_mat(matT);
}

int main(){
	std::cout << "---------------------------------------------" << std::endl;
	test_bitonic_sort_avx512_1x8();
	std::cout << "---------------------------------------------" << std::endl;
	test_bitonic_sort_avx512_8x8();
	std::cout << "---------------------------------------------" << std::endl;
	test_bitonic_merge_avx512_2x8();
	std::cout << "---------------------------------------------" << std::endl;
	test_transpose_8x8();
	return 0;
}
