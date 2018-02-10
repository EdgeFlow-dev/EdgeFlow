//
// Created by xzl on 8/23/17.
//
// cf: WinGBKEvaluator.h

#ifndef CREEK_WINGBKSEQEVAL_H_H
#define CREEK_WINGBKSEQEVAL_H_H

#include "SeqBundle.h"
#include "Win/WinGBKSeq.h"
#include "WinKeySeqFrag.h"
#include "core/SingleInputTransformEvaluator.h"

/* FixedWindowInto + GroupByKey. Stateless */
template <class KVPair, int key, unsigned N, int k2pos>
class WinGBKSeqEval
		: public SingleInputTransformEvaluator<WinGBKSeq<KVPair, key, N, k2pos>,
				RecordSeqBundle<KVPair>, WinKeySeqBundle<KVPair, N>>
{
	using TransformT = WinGBKSeq<KVPair, key, N, k2pos>;
	using InputBundleT = RecordSeqBundle<KVPair>;
	using OutputBundleT = WinKeySeqBundle<KVPair, N>; /* this is fixed */

public:

#if 0
	void test()
	{
		using T = Record<KVPair>;
		Record<KVPair> r1(make_pair(30, 10), 10 /* ts */);
		Record<KVPair> r2(make_pair(10, 10), 15 /* ts */);
		Record<KVPair> r3(make_pair(40, 10), 20 /* ts */);
		Record<KVPair> r4(make_pair(50, 10), 21 /* ts */);
		Record<KVPair> r5(make_pair(110, 10), 22 /* ts */);

		Record<KVPair> raw_data[] = {r1, r2, r3, r4, r5};
		SeqBuf<Record<KVPair>> sb(raw_data, sizeof(raw_data) / sizeof(T));
		auto res1 = sb.Segment(etime(0), etime(10));
	}
#endif

	bool evaluateSingleInput(TransformT *trans,
													 shared_ptr<InputBundleT> input_bundle,
													 shared_ptr<OutputBundleT> output_bundle) override {
		Window ww;
		ww.duration = trans->window_size;
		unsigned long totalrec = 0; // stat

#if 0 /* old impl. does not support multi output */
//		W("going to call");
//		input_bundle->seqbuf.Dump("before gbk");

		/* directly return a map */
		output_bundle->vals =
				input_bundle->seqbuf.Segment(0, trans->window_size);

		DD("called. total windows %lu", output_bundle->vals.size());

		/* in addition, sort each window by key */
		for (auto &frag : output_bundle->vals) {
				DD("sort a window");
				/* for each underlying seqbuf, sort then replace.
				 * we probably don't need parallel execution */
				unsigned int n = frag.second.Size();
				xzl_bug_on(!n);

				for (unsigned int i = 0; i < n; i++) {
					if (trans->mode == WINGBK_BYKEY)
						frag.second.template Sorted2<key>(i);
					totalrec += frag.second.seqbufs[i].Size();
				}
		}
#endif

		auto seg_res = input_bundle->seqbuf.Segment(0 /*etime base */, trans->window_size);

		/* seg output: each window has one seqbuf, which has one partition only (limited by segment) */

		std::map<Window, WinKeySeqFrag<KVPair, N>, Window> m;

		for (auto &frag : seg_res) {
			/* --  one window -- */
			unsigned int n = frag.second.Size();
			xzl_bug_on_msg(n != 1, "tbd - seg only supports 1 output");
			auto & incoming_win = frag.first;
			auto & incoming_seqbuf = frag.second.seqbufs[0];

			if (trans->mode == WINGBK_WINONLY) { /* output winkeyfrag must have 1 part only. so move seqbufs[0] over */
				xzl_bug_on_msg(N != 1, "tbd - seg only supports 1 partition");
				WinKeySeqFrag<KVPair, N> outfrag(incoming_win, incoming_seqbuf);
				m[incoming_win] = outfrag;
			} else if (trans->mode == WINGBK_BYKEY) {
				vector<SeqBuf<Record<KVPair>>> out_seqbufs;
				if (k2pos >= 0) { /* need to sort on a secondary key */
//					auto seqbuf_vec1 = frag.second.seqbufs[0].template Sort<k2pos>(N); /* sort into N partitions by 2nd key*/
//					for (unsigned i = 0; i < N; i++)
//						seqbuf_vec.push_back(seqbuf_vec1[i].template Sort2<key>());
					out_seqbufs = incoming_seqbuf.template SortMultiKey<key, k2pos>(N); /* sort into N partitions */
				} else { /* sort by primary key only */
					xzl_bug_on(k2pos != -1);
					out_seqbufs = incoming_seqbuf.template Sort<key>(N); /* sort into N partitions */
				}
				WinKeySeqFrag<KVPair, N> outfrag(incoming_win, out_seqbufs);
				m[incoming_win] = outfrag;
			} else {
				xzl_bug("unsupported mode?");
			}
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

	WinGBKSeqEval(int node)
			: SingleInputTransformEvaluator<TransformT,
			InputBundleT, OutputBundleT>(node) {}
};

#endif //CREEK_WINGBKSEQEVAL_H_H
