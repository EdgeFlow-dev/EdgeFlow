/* Author: Hongyu Miao @ Purdue ECE
 * Date: Nov 9th, 2017 
 * Description: This file provides test code for functions in x2_sort_merge.h
 * Compile: g++ -O3 -std=c++11 -mavx512f -mavx512pf -mavx512er -mavx512cd x2_bitonic_sort_merge_core.cpp x2_sort_merge.cpp test_x2_sort_merge.cpp -o test_x2_sort_merge
 * */
#include "x2_sort_merge.h"
#include <iostream>
#include <sys/time.h>  
#include <climits>
void test_x2_qsort(){
	int64_t len = 15;
	int64_t * in_k = (int64_t *)malloc(sizeof(int64_t)* len);
	int64_t * in_ptr = (int64_t *)malloc(sizeof(int64_t)* len);
	for(int64_t i = 0; i < len; i++){
		in_k[i] = 100 - i;
		in_ptr[i] = 100 - i;
	}
	std::cout << "befor x2_qsort: " << std::endl;
	for(int64_t i = 0; i < len; i++){
		std::cout << "<" << in_k[i] << "," << in_ptr[i] << ">,";
	}
	std::cout << std::endl;
	
	x2_qsort(in_k, in_ptr, len);
	
	std::cout << "after x2_qsort: " << std::endl;
	for(int64_t i = 0; i < len; i++){
		std::cout << "<" << in_k[i] << "," << in_ptr[i] << ">,";
	}
	std::cout << std::endl;
}

void test_x2_sort_every_8(){
	int64_t len = 32;
	int64_t * in_k = (int64_t *)malloc(sizeof(int64_t)* len);
	int64_t * in_ptr = (int64_t *)malloc(sizeof(int64_t)* len);
	for(int64_t i = 0; i < len; i++){
		in_k[i] = 100 - i;
		in_ptr[i] = 100 - i;
	}
	std::cout << "befor sort_every_8(): " << std::endl;
	for(int64_t i = 0; i < len; i++){
		std::cout << "<" << in_k[i] << "," << in_ptr[i] << ">,";
	}
	std::cout << std::endl;
	
	x2_sort_every_8(in_k, in_ptr, len);
	
	std::cout << "after sort_every_8(): " << std::endl;
	for(int64_t i = 0; i < len; i++){
		std::cout << "<" << in_k[i] << "," << in_ptr[i] << ">,";
		if(i%8 == 7){
			std::cout << std::endl;
		}
	}
	std::cout << std::endl;
}

void test_x2_merge_8_aligned(){
	int64_t len1 = 16;
	int64_t len2 = 24;
	int64_t * in1_k = (int64_t *)malloc(sizeof(int64_t)* len1);
	int64_t * in1_ptr = (int64_t *)malloc(sizeof(int64_t)* len1);
	int64_t * in2_k = (int64_t *)malloc(sizeof(int64_t)* len2);
	int64_t * in2_ptr = (int64_t *)malloc(sizeof(int64_t)* len2);
	int64_t * out_k = (int64_t *)malloc(sizeof(int64_t)* (len1 + len2));
	int64_t * out_ptr = (int64_t *)malloc(sizeof(int64_t)* (len1 + len2));
	
	for(int64_t i = 0; i < len1; i++){
		in1_k[i] = i;
		in1_ptr[i] = i;
	}
	
	for(int64_t i = 0; i < len2; i++){
		in2_k[i] = i;
		in2_ptr[i] = i;
	}

	x2_merge_8_aligned(in1_k, in1_ptr, in2_k, in2_ptr, out_k, out_ptr, len1,len2);

	std::cout << "after x2_merge_8_aligned(): " << std::endl;
	for(int64_t i = 0; i < len1+len2; i++){
		std::cout << "<" << out_k[i] << "," << out_ptr[i] << ">,";
	}
	std::cout << std::endl;
}

void test_x2_merge_8_unaligned(){
	int64_t len1 = 25;
	int64_t len2 = 19;
	int64_t * in1_k = (int64_t *)malloc(sizeof(int64_t)* len1);
	int64_t * in1_ptr = (int64_t *)malloc(sizeof(int64_t)* len1);
	int64_t * in2_k = (int64_t *)malloc(sizeof(int64_t)* len2);
	int64_t * in2_ptr = (int64_t *)malloc(sizeof(int64_t)* len2);
	int64_t * out_k = (int64_t *)malloc(sizeof(int64_t)* (len1 + len2));
	int64_t * out_ptr = (int64_t *)malloc(sizeof(int64_t)* (len1 + len2));
	
	for(int64_t i = 0; i < len1; i++){
		in1_k[i] = i;
		in1_ptr[i] = i;
	}
	
	for(int64_t i = 0; i < len2; i++){
		in2_k[i] = i;
		in2_ptr[i] = i;
	}

	x2_merge_8_unaligned(in1_k, in1_ptr, in2_k, in2_ptr, out_k, out_ptr, len1,len2);

	std::cout << "after x2_merge_8_unaligned(): " << std::endl;
	for(int64_t i = 0; i < len1+len2; i++){
		std::cout << "<" << out_k[i] << "," << out_ptr[i] << ">,";
	}
	std::cout << std::endl;
}


void test_x2_sort(){
	//test different len
	for(int64_t len = 0; len < 20; len++){
		//int64_t len = 101;
		int64_t * in_k = (int64_t *)malloc(sizeof(int64_t)* len);
		int64_t * in_ptr = (int64_t *)malloc(sizeof(int64_t)* len);
		for(int64_t i = 0; i < len; i++){
			in_k[i] = rand()%100;
			in_ptr[i] = in_k[i];;
		}
#if 0	
		std::cout << "befor x2_sort: " << std::endl;
		for(int64_t i = 0; i < len; i++){
			std::cout << "<" << in_k[i] << "," << in_ptr[i] << ">,";
		}
		std::cout << std::endl << std::endl;
#endif	
		x2_sort(in_k, in_ptr, len);
#if 0	
		std::cout << "after x2_sort: " << std::endl;
		for(int64_t i = 0; i < len; i++){
			std::cout << "<" << in_k[i] << "," << in_ptr[i] << ">,";
		}
		std::cout << std::endl;
#endif
		//if(check_sort_k(in_k, len)){
		if(check_sort_kptr(in_k, in_ptr, len)){
			std::cout << "len is " << len << ": Sort is correct!" << std::endl;
			free(in_k);
			free(in_ptr);
		}else{
			std::cout << "len is " << len << ": Sort is wrong!" << std::endl;
			free(in_k);
			free(in_ptr);
			abort();
		}
	}
}

void test_x2_sort_out(){
	//test different len
	for(int64_t len = 0; len < 20; len++){
		//int64_t len = 101;
		int64_t * in_k = (int64_t *)malloc(sizeof(int64_t)* len);
		int64_t * in_ptr = (int64_t *)malloc(sizeof(int64_t)* len);
		int64_t * out_k = (int64_t *)malloc(sizeof(int64_t)* len);
		int64_t * out_ptr = (int64_t *)malloc(sizeof(int64_t)* len);
		
		for(int64_t i = 0; i < len; i++){
			in_k[i] = rand()%100;
			in_ptr[i] = in_k[i];;
		}
#if 0	
		std::cout << "befor x2_sort: " << std::endl;
		for(int64_t i = 0; i < len; i++){
			std::cout << "<" << in_k[i] << "," << in_ptr[i] << ">,";
		}
		std::cout << std::endl << std::endl;
#endif	
		x2_sort(in_k, in_ptr, out_k, out_ptr, len);
#if 0	
		std::cout << "after x2_sort: " << std::endl;
		for(int64_t i = 0; i < len; i++){
			std::cout << "<" << in_k[i] << "," << in_ptr[i] << ">,";
		}
		std::cout << std::endl;
#endif
		//if(check_sort_k(in_k, len)){
		if(check_sort_kptr(out_k, out_ptr, len)){
			std::cout << "len is " << len << ": Sort is correct!" << std::endl;
		}else{
			std::cout << "len is " << len << ": Sort is wrong!" << std::endl;
			abort();
		}
		free(in_k);
		free(in_ptr);
		free(out_k);
		free(out_ptr);
	}
}

void test_x2_sort_out_speed(){
		struct timeval t1, t2;
		double elapsedTime;
		int len = 1000000;
		
		int64_t * in_k = (int64_t *)malloc(sizeof(int64_t)* len);
		int64_t * in_ptr = (int64_t *)malloc(sizeof(int64_t)* len);
		int64_t * out_k = (int64_t *)malloc(sizeof(int64_t)* len);
		int64_t * out_ptr = (int64_t *)malloc(sizeof(int64_t)* len);
		
		for(int64_t i = 0; i < len; i++){
			in_k[i] = rand()%INT_MAX;
			in_ptr[i] = in_k[i];;
		}
		
		gettimeofday(&t1, NULL);
		x2_sort(in_k, in_ptr, out_k, out_ptr, len);
		gettimeofday(&t2, NULL);
		
		elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;	 // sec to ms
		elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0;   // us to ms
	
		std::cout << "sort 1000000 kv: " << elapsedTime << " ms" << std::endl;
/*		
		if(check_sort_kptr(out_k, out_ptr, len)){
			std::cout << "len is " << len << ": Sort is correct!" << std::endl;
		}else{
			std::cout << "len is " << len << ": Sort is wrong!" << std::endl;
			abort();
		}
*/
		free(in_k);
		free(in_ptr);
		free(out_k);
		free(out_ptr);
}
int main(){
/*
	std::cout << "------------------------------------" << std::endl;
	test_x2_qsort();
	
	std::cout << "------------------------------------" << std::endl;
	test_x2_sort_every_8();
	
	std::cout << "------------------------------------" << std::endl;
	test_x2_merge_8_aligned();
	
	std::cout << "------------------------------------" << std::endl;
	test_x2_merge_8_unaligned();
	
	std::cout << "------------------------------------" << std::endl;
	test_x2_sort();
	
	std::cout << "------------------------------------" << std::endl;
	test_x2_sort_out();
*/	
	test_x2_sort_out_speed();
	return 0;
}

