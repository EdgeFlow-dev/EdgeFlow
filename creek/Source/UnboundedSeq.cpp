//
// Created by xzl on 8/30/17.
//
// cf Unbounded.cpp

#include "Values.h"
#include "UnboundedSeq.h"
#include "UnboundedSeqEval.h"

/* no longer a template. so no "template" needed.
 * no explicit instantiation needed either */
void UnboundedInMem<creek::kvpair, RecordSeqBundle>::ExecEvaluator(int nodeid,
																																	 EvaluationBundleContext *c, shared_ptr<BundleBase> bundle_ptr) {
	/* instantiate an evaluator */
	UnboundedInMemEvaluator<creek::kvpair, RecordSeqBundle> eval(nodeid);
	eval.evaluate(this, c, bundle_ptr);
}