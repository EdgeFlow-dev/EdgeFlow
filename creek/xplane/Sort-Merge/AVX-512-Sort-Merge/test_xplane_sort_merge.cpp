/* Author: Hongyu Miao @ Purdue ECE
 * Date: Nov 9th, 2017 
 * Description: This file provides test code for functions in x2_sort_merge.h
 * Compile: g++ -O3 -std=c++11 -mavx512f -mavx512pf -mavx512er -mavx512cd test_xplan_sort_merge.cpp -o test_xplan_sort_merge
 **/

#include "xplane_sort_merge.h"
#include <iostream>

void test_sort(){
	std::cout << "test_sort(): " << std::endl;
	int reclen = 3;
	int record_num = 100;
	batch_t * src_batch = (batch_t *) malloc(sizeof(batch_t));

	src_batch->size = record_num * reclen;
	src_batch->start = (int64_t *) malloc(sizeof(int64_t) * src_batch->size);
	
	for(int i = 0; i < record_num; i++){	
		for(int j = 0; j < reclen; j++){
			src_batch->start[i*reclen +j] = 200 - i; 
		}
	}
	
	x_addr src = (x_addr) (src_batch);
	x_addr *dests;
	uint64_t n_outputs = 3;
	dests = (x_addr *) malloc(sizeof(x_addr) * n_outputs);
	
	sort(dests, n_outputs, src, 0, reclen);

	batch_t *dst_ptr;
	for(int i = 0; i < n_outputs; i++){
		std::cout << "sorted out " << i << std::endl;
		dst_ptr = (batch_t *) dests[i];
		std::cout << "dst_ptr->size/reclen is " << dst_ptr->size/reclen << std::endl;
		for(uint64_t j = 0; j < dst_ptr->size/reclen; j++){
			std::cout << "<" << dst_ptr->start[j*reclen] << "," << dst_ptr->start[j*reclen +1] << "," << dst_ptr->start[j*reclen +2] << ">,";	
		}
		std::cout << std::endl << std::endl;
	}
}

void test_merge(){
	std::cout << "test_merge(): " << std::endl;	
	int reclen = 3;
	int record_num_A = 10;
	int record_num_B = 50;
	batch_t * src_batch_A = (batch_t *) malloc(sizeof(batch_t));
	batch_t * src_batch_B = (batch_t *) malloc(sizeof(batch_t));

	src_batch_A->size = record_num_A * reclen;
	src_batch_A->start = (int64_t *) malloc(sizeof(int64_t) * src_batch_A->size);
	
	src_batch_B->size = record_num_B * reclen;
	src_batch_B->start = (int64_t *) malloc(sizeof(int64_t) * src_batch_B->size);
	
	for(int i = 0; i < record_num_A; i++){	
		for(int j = 0; j < reclen; j++){
			src_batch_A->start[i*reclen +j] = i; 
		}
	}
	for(int i = 0; i < record_num_B; i++){	
		for(int j = 0; j < reclen; j++){
			src_batch_B->start[i*reclen +j] = i; 
		}
	}
	
	x_addr src_A = (x_addr) (src_batch_A);
	x_addr src_B = (x_addr) (src_batch_B);
	
	x_addr *dests;
	uint64_t n_outputs = 3;
	int keypos = 0;
	dests = (x_addr *) malloc(sizeof(x_addr) * n_outputs);

	merge(dests, n_outputs, src_A, src_B, keypos, reclen);
		
	batch_t *dst_ptr;
	for(int i = 0; i < n_outputs; i++){
		std::cout << "merged out " << i << std::endl;
		dst_ptr = (batch_t *) dests[i];
		std::cout << "dst_ptr->size/reclen is " << dst_ptr->size/reclen << std::endl;
		for(uint64_t j = 0; j < dst_ptr->size/reclen; j++){
			std::cout << "<" << dst_ptr->start[j*reclen] << "," << dst_ptr->start[j*reclen +1] << "," << dst_ptr->start[j*reclen +2] << ">,";	
		}
		std::cout << std::endl << std::endl;
	}
}


int main(){
	std::cout << "------------------------------------" << std::endl;
	test_merge();
	
	std::cout << "------------------------------------" << std::endl;
	test_sort();
	return 0;
}
