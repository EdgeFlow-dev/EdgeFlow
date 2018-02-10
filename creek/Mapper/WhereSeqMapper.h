//
// Created by xzl on 7/11/17.
//
// implement the basic WHERE operator
// filter records based on thersholds (low, high)
//
// Contains full def of ExecEvaluator() to avoid circular dep

#ifndef CREEK_WINDOWEDNUMMAPPER2_H
#define CREEK_WINDOWEDNUMMAPPER2_H

// cf: SimpleMapper.h
// stateless

#include "Mapper/WhereSeqMapper0.h"
#include "Mapper/WhereSeqMapperEval.h"

template <typename InputT, typename OutputT, int keypos, int vpos>
void WhereSeqMapper<InputT, OutputT, keypos, vpos>::ExecEvaluator(int nodeid,
																	 EvaluationBundleContext *c, shared_ptr<BundleBase> bundle)
{
	/* instantiate an evaluator */
	WhereSeqMapperEval<InputT, OutputT, keypos, vpos> eval(nodeid);
	eval.evaluate(this, c, bundle);
}


#endif //CREEK_WINDOWEDNUMMAPPER2_H
