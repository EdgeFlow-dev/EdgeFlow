/* Author: Hongyu Miao @ Purdue ECE 
 * Date: Nov 9th, 2017 
 * Description: This file provides sort/merge functions for xplan interface 
 * */
#ifndef XPLAN_SORT_MERGE_H
#define XPLAN_SORT_MERGE_H 

#include "x2_sort_merge.h"
//#include "xplane-types.h"
#include "mm/batch.h"
/* List of Functions */
int32_t sort(x_addr *dests, uint32_t n_outputs, x_addr src, idx_t keypos, idx_t reclen);
int32_t merge(x_addr *dests, uint32_t n_outputs, x_addr srca, x_addr srcb, idx_t keypos, idx_t reclen);
/* @vpos: alrady sorted pos */
int32_t sort_stable(x_addr *dests, uint32_t n_outputs, x_addr src, idx_t keypos, idx_t vpos, idx_t reclen);
#endif /* XPLAN_SORT_MERGE_H */
