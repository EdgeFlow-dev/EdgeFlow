//
// Created by xzl on 8/23/17.
//
// cf: WinGBK.h
//
// this impl using SeqBuf underneath

#ifndef CREEK_GBKSEQ_H
#define CREEK_GBKSEQ_H

#include "core/Transforms.h"

template <class T, unsigned N, int keypos, int k2pos>
class GBKSeqEval;

// input bundle: WinKeySeqBundle
// output bundle: WinKeySeqBundle
// N: # of partitions in each output window.
template <class KVPair, unsigned N, int keypos, int k2pos = -1>
class GBKSeq : public PTransform {
	using InputT = Record<KVPair>;

public:
	GBKSeq(string name, int sideinfo)
			: PTransform(name, sideinfo) { }

	void ExecEvaluator(int nodeid, EvaluationBundleContext *c,
										 shared_ptr<BundleBase> bundle_ptr) override {

		GBKSeqEval<KVPair, N, keypos, k2pos> eval(nodeid);

		/* sanity check */
		xzl_bug_on(keypos >= reclen<Record<KVPair>>() || keypos < 0);
		xzl_bug_on(k2pos >= reclen<Record<KVPair>>());

		eval.evaluate(this, c, bundle_ptr);
	}
};

#endif //CREEK_GBKSEQ_H
