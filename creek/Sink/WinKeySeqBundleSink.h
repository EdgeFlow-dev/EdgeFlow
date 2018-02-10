//
// Created by xzl on 8/25/17.
//

#ifndef CREEK_WINKEYSEQBUNDLESINK_H
#define CREEK_WINKEYSEQBUNDLESINK_H

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

/* @T: element. e.g. kvpair or k2v */
template<typename T>
class WinKeySeqBundleSinkEval; /* avoid circular dep */

template<typename T>
class WinKeySeqBundleSinkEvalJD; /* avoid circular dep */

/* @T: element type, eg kvpair.
 * NB: this defaults N == 1. otherwise bundle type mismatch
 * */
template<typename T>
class WinKeySeqBundleSink : public PTransform {
	using InputBundleT = WinKeySeqBundle<T>;

public:
	WinKeySeqBundleSink(string name, int sideinfo)  : PTransform(name, sideinfo) { }

	static void printBundle(const InputBundleT & input_bundle) {
		I("	got one bundle");

#if 0
		/* sample some bundles */
		static int cnt = 0;
		if (cnt ++ % 2 == 0) {
			for (auto & winfrag : input_bundle.vals) {
				auto &seqbuf = winfrag.second.seqbufs[0];
				seqbuf.Dump("sink");
			}
		}
#endif

	}

	void ExecEvaluator(int nodeid, EvaluationBundleContext *c,
										 shared_ptr<BundleBase> bundle_ptr) override {
		if(this->get_side_info() == SIDE_INFO_JD) {
	#ifdef WORKAROUND_JOIN_JDD
			WinKeySeqBundleSinkEval<T> eval(nodeid); /* construct a normal eval */
	#else
			WinKeySeqBundleSinkEvalJD<T> eval(nodeid); /* side info JD -- the right way. TBD */
	#endif
			eval.evaluate(this, c, bundle_ptr);
		} else {
			xzl_assert(this->get_side_info() == SIDE_INFO_JDD
								 || this->get_side_info() == SIDE_INFO_NONE);
			WinKeySeqBundleSinkEval<T> eval(nodeid); /* default side info */
			eval.evaluate(this, c, bundle_ptr);
		}
	}

};

#endif //CREEK_WINKEYSEQBUNDLESINK_H
