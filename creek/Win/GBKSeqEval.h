//
// Created by xzl on 8/23/17.
//
// cf: WinGBKEvaluator.h

#ifndef CREEK_GBKSEQEVAL_H_H
#define CREEK_GBKSEQEVAL_H_H

#include "SeqBundle.h"
#include "Win/GBKSeq.h"
#include "WinKeySeqFrag.h"
#include "core/SingleInputTransformEvaluator.h"

/* FixedWindowInto + GroupByKey. Stateless */
template <class KVPair, unsigned N, int key, int k2pos>
class GBKSeqEval
		: public SingleInputTransformEvaluator<GBKSeq<KVPair, N, key, k2pos>,
				WinKeySeqBundle<KVPair, N>, WinKeySeqBundle<KVPair, N>>
{
	using TransformT = GBKSeq<KVPair, N, key, k2pos>;
	using InputBundleT = WinKeySeqBundle<KVPair, N>;
	using OutputBundleT = WinKeySeqBundle<KVPair, N>; /* this is fixed */

public:

	bool evaluateSingleInput(TransformT *trans,
													 shared_ptr<InputBundleT> input_bundle,
													 shared_ptr<OutputBundleT> output_bundle) override {
		unsigned long totalrec = 0; // stat

		std::map<Window, WinKeySeqFrag<KVPair, N>, Window> m;

		for (auto &win_frag : input_bundle->vals) {
			/* --  one window -- */
			auto & incoming_win = win_frag.first;
			auto & infrag = win_frag.second;
//			auto & incoming_seqbufs = infrag.seqbufs;
			unsigned int n = infrag.Size();
			xzl_bug_on(n != N);

			WinKeySeqFrag<KVPair, N> outfrag(infrag);		/* make a copy, which only copy ref underneath */

			if (k2pos >= 0) { /* need to sort on a secondary key */
				for (unsigned i = 0; i < n; i++)
					outfrag. template SortedMultiKey<key, k2pos>(i);
			} else { /* sort by primary key only */
				xzl_bug_on(k2pos != -1);
				for (unsigned i = 0; i < n; i++)
					outfrag. template Sorted2<key>(i);
			}
			m[incoming_win] = outfrag;
		}

		output_bundle->vals = m;
		totalrec += 1; /* be cheap */

		/* update stat */
		trans->record_counter_.fetch_add(totalrec, std::memory_order_relaxed);

		if (output_bundle->vals.size())
			return true;
		else
			return false;
	}

	GBKSeqEval(int node)
			: SingleInputTransformEvaluator<TransformT, InputBundleT, OutputBundleT>(node) {}
};

#endif //CREEK_WINGBKSEQEVAL_H_H
