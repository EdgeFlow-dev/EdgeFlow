/*
 * Description: This file provides test code for functions in sort_merge.h
 * Compile: g++ -O3 -std=c++11 -mavx512f -mavx512pf -mavx512er -mavx512cd bitonic_sort_merge_core.cpp sort_merge.cpp test_sort_merge.cpp -o test_sort_merge
 **/
//#include "sort_merge.h"
#include "bitonic_sort_merge_core.h"
#include <iostream>
#include <sys/time.h>  
#include <climits>
void test_sort_every_8(){
	int64_t len = 32;
	int64_t * in_k = (int64_t *)malloc(sizeof(int64_t)* len);
	int64_t * out_k = (int64_t *)malloc(sizeof(int64_t)* len);
	for(int64_t i = 0; i < len; i++){
		in_k[i] = 100 - i;
	}
	std::cout << "befor sort_every_8(): " << std::endl;
	for(int64_t i = 0; i < len; i++){
		std::cout << in_k[i] << ",";
	}
	std::cout << std::endl;
	
	sort_every_8(in_k, out_k, len);
	
	std::cout << "after sort_every_8(): " << std::endl;
	for(int64_t i = 0; i < len; i++){
		std::cout << out_k[i] << ",";
		if(i%8 == 7){
			std::cout << std::endl;
		}
	}
	std::cout << std::endl;
}

void test_merge_8_aligned(){
	int64_t len1 = 16;
	int64_t len2 = 24;
	int64_t * in1_k = (int64_t *)malloc(sizeof(int64_t)* len1);
	int64_t * in2_k = (int64_t *)malloc(sizeof(int64_t)* len2);
	int64_t * out_k = (int64_t *)malloc(sizeof(int64_t)* (len1 + len2));
	
	for(int64_t i = 0; i < len1; i++){
		in1_k[i] = i;
	}
	
	for(int64_t i = 0; i < len2; i++){
		in2_k[i] = i;
	}

	merge_8_aligned(in1_k, in2_k, out_k, len1, len2);

	std::cout << "after merge_8_aligned(): " << std::endl;
	for(int64_t i = 0; i < len1+len2; i++){
		std::cout << out_k[i] << ",";
	}
	std::cout << std::endl;
}

void test_x2_merge_8_unaligned(){
	int64_t len1 = 25;
	int64_t len2 = 19;
	int64_t * in1_k = (int64_t *)malloc(sizeof(int64_t)* len1);
	int64_t * in2_k = (int64_t *)malloc(sizeof(int64_t)* len2);
	int64_t * out_k = (int64_t *)malloc(sizeof(int64_t)* (len1 + len2));
	int64_t * out_ptr = (int64_t *)malloc(sizeof(int64_t)* (len1 + len2));
	
	for(int64_t i = 0; i < len1; i++){
		in1_k[i] = i;
	}
	
	for(int64_t i = 0; i < len2; i++){
		in2_k[i] = i;
	}

	merge_8_unaligned(in1_k, in2_k, out_k, len1, len2);

	std::cout << "after x2_merge_8_unaligned(): " << std::endl;
	for(int64_t i = 0; i < len1+len2; i++){
		std::cout << out_k[i] << ",";
	}
	std::cout << std::endl;
}

bool check_sort(int64_t *k, uint64_t len){
	for(int i = 1; i < len; i++){
		if(k[i] < k[i-1]){
			return false;
		}
	}
	return true;
}

void test_sort(){
	//test different len
	for(int64_t len = 0; len < 20; len++){
		//int64_t len = 101;
		int64_t * in_k = (int64_t *)malloc(sizeof(int64_t)* len);
		int64_t * out_k = (int64_t *)malloc(sizeof(int64_t)* len);
		for(int64_t i = 0; i < len; i++){
			in_k[i] = rand()%100;
		}
		
		sort(in_k, out_k, len);
		
		//if(check_sort_k(in_k, len)){
		if(check_sort(out_k, len)){
			std::cout << "len is " << len << ": Sort is correct!" << std::endl;
		}else{
			std::cout << "len is " << len << ": Sort is wrong!" << std::endl;
		}
		free(in_k);
		free(out_k);
	}
}
void test_sort_speed(){
		struct timeval t1, t2;
		double elapsedTime;
		int len = 1000000;
		
		int64_t * in_k = (int64_t *)malloc(sizeof(int64_t)* len);
		int64_t * out_k = (int64_t *)malloc(sizeof(int64_t)* len);
		for(int64_t i = 0; i < len; i++){
			in_k[i] = rand()%INT_MAX;
		}
		
		gettimeofday(&t1, NULL);
		sort(in_k, out_k, len);
		gettimeofday(&t2, NULL);

		elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;	 // sec to ms
		elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0;   // us to ms
	
		std::cout << "sort 1000000 k: " << elapsedTime << " ms" << std::endl;
/*
		if(check_sort(out_k, len)){
			std::cout << "len is " << len << ": Sort is correct!" << std::endl;
		}else{
			std::cout << "len is " << len << ": Sort is wrong!" << std::endl;
		}
*/
		free(in_k);
		free(out_k);
}
int main(){
/*
	std::cout << "------------------------------------" << std::endl;
	test_sort_every_8();
	test_merge_8_aligned();
	test_x2_merge_8_unaligned();
	test_sort();
*/
	test_sort_speed();
	return 0;
}
