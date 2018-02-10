//
// Created by xzl on 9/1/17.
//
// cf Sink/WinKeySeqBundleSink.cpp

#if 0

#define K2_NO_DEBUG 1

#include "RecordSeqBundleSink.h"
#include "RecordSeqBundleSinkEval.h"

template<typename T>
void RecordSeqBundleSink<T>::ExecEvaluator
		(int nodeid, EvaluationBundleContext *c, shared_ptr<BundleBase> bundle_ptr)
{

	if(this->get_side_info() == SIDE_INFO_JD) {
		#if 0
#ifdef WORKAROUND_JOIN_JDD
		RecordSeqBundleSinkEval eval(nodeid); /* construct a normal eval */
#else
		WinKeySeqBundleSinkEvalJD eval(nodeid); /* side info JD -- the right way. TBD */
#endif
		eval.evaluate(this, c, bundle_ptr);
		#endif
		xzl_bug("TBD");
	} else {
		xzl_assert(this->get_side_info() == SIDE_INFO_JDD
							 || this->get_side_info() == SIDE_INFO_NONE);
		RecordSeqBundleSinkEval<T> eval(nodeid); /* default side info */
		eval.evaluate(this, c, bundle_ptr);
	}
}

template
void RecordSeqBundleSink<T>::ExecEvaluator
		(int nodeid, EvaluationBundleContext *c, shared_ptr<BundleBase> bundle_ptr);


#endif