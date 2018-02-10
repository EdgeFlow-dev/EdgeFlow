//
// Created by xzl on 8/23/17.
//
// cf: WinGBK.cpp

#define K2_NO_DEBUG 1

#include "Values.h"
#include "core/EvaluationBundleContext.h"
#include "WinGBKSeq.h"
#include "WinGBKSeqEval.h"

#if 0
template <class KVPair, int keypos>
void WinGBKSeq<KVPair, keypos>::ExecEvaluator(int nodeid,
	EvaluationBundleContext *c, shared_ptr<BundleBase> bundle_ptr)
{

#ifndef NDEBUG /* if evaluators get stuck ...*/
	static atomic<int> outstanding (0);
#endif
	/* instantiate an evaluator */
	WinGBKSeqEval<KVPair, keypos> eval(nodeid);
	//eval.evaluate(this, c);

#ifndef NDEBUG		// for debug
	outstanding ++;
#endif
	eval.evaluate(this, c, bundle_ptr);

#ifndef NDEBUG   // for debug
	outstanding --; I("end eval... outstanding = %d", outstanding);
#endif
}

/* template instantiation with concrete types.
 *
 * this is verbose but we don't have partial specialization...
 * */

template
void WinGBKSeq<creek::kvpair, 0>
::ExecEvaluator(int nodeid,
								EvaluationBundleContext *c, shared_ptr<BundleBase> bundle_ptr);

template
void WinGBKSeq<creek::kvpair, 1>
::ExecEvaluator(int nodeid,
								EvaluationBundleContext *c, shared_ptr<BundleBase> bundle_ptr);

template
void WinGBKSeq<creek::k2v, 0>
::ExecEvaluator(int nodeid,
								EvaluationBundleContext *c, shared_ptr<BundleBase> bundle_ptr);

template
void WinGBKSeq<creek::k2v, 1>
::ExecEvaluator(int nodeid,
								EvaluationBundleContext *c, shared_ptr<BundleBase> bundle_ptr);

/* todo: instantiate more types */
#endif


