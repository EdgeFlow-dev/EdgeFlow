//
// Created by xzl on 8/25/17.
//

#define K2_NO_DEBUG 1

#include "WinKeySeqBundleSink.h"
#include "WinKeySeqBundleSinkEval.h"

//void WinKeySeqBundleSink<creek::kvpair>::ExecEvaluator
//		(int nodeid, EvaluationBundleContext *c, shared_ptr<BundleBase> bundle_ptr)
//{
//	if(this->get_side_info() == SIDE_INFO_JD) {
//#ifdef WORKAROUND_JOIN_JDD
//		WinKeySeqBundleSinkEval eval(nodeid); /* construct a normal eval */
//#else
//		WinKeySeqBundleSinkEvalJD eval(nodeid); /* side info JD -- the right way. TBD */
//#endif
//		eval.evaluate(this, c, bundle_ptr);
//	} else {
//		xzl_assert(this->get_side_info() == SIDE_INFO_JDD
//							 || this->get_side_info() == SIDE_INFO_NONE);
//		WinKeySeqBundleSinkEval eval(nodeid); /* default side info */
//		eval.evaluate(this, c, bundle_ptr);
//	}
//}
