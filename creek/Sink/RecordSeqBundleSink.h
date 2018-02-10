//
// Created by xzl on 9/1/17.
//
// cf WinKeySeqBundleSink.h

#ifndef CREEK_SEQBUNDLESINK_H
#define CREEK_SEQBUNDLESINK_H

#include <iostream>
#include "config.h"

extern "C" {
#include "log.h"
#include "measure.h"
}

#include "core/Transforms.h"
#include "SeqBundle.h"
#include "BundleContainer.h"

using namespace std;

template<typename T>
class RecordSeqBundleSinkEval;

/* T: element type, e.g. kvpair */
template<typename T>
class RecordSeqBundleSink : public PTransform {
	using InputBundleT = RecordSeqBundle<T>;

public:
	RecordSeqBundleSink(string name, int sideinfo)  : PTransform(name, sideinfo) { }

	static void printBundle(const InputBundleT & input_bundle) {
		I("got one bundle");

#if 0
		/* sample some bundles */
		static int cnt = 0;
		if (cnt ++ % 2 == 0) {
			input_bundle.seqbuf.Dump("sink");
		}
#endif

	}

	void ExecEvaluator(int nodeid, EvaluationBundleContext *c,
										 shared_ptr<BundleBase> bundle_ptr) override
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

};

#endif //CREEK_SEQBUNDLESINK_H
