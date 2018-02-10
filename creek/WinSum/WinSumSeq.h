//
// Created by xzl on 8/27/17.
//
// cf WinSumBase.h
//
//

#ifndef CREEK_WINSUMSEQ_H_H
#define CREEK_WINSUMSEQ_H_H


#include "config.h"

#include "Values.h"
#include "SeqBundle.h"
#include "core/StatefulTransform.h"

template <typename InputT, typename OutputT, int keypos, int vpos, unsigned N>
class WinSumSeqEval;

namespace creek {
	enum WinSumMode  {  // these are different from seqbuf mode
		SUM_BYKEY,
		SUM_ALL,
		AVG_ALL,
		/* by key */
		AVG_BYKEY,
		MEDV_BYKEY,
		TOPV_BYKEY,
		PERCENTILE_BYKEY,
		UNIQUE_BYKEY,
		TOPCOUNT_BYKEY, /* emit keys w/ highest occurrences. assume all input has v =1 */
	};
}

/* Summation over tuples. Support multiple modes:
 * 	SUM_PER_KEY
 * 	SUM_ALL -- in this mode, each worker collapses N tuples into one (most heavy lifting work). it seems to make little
 * 	sense to use N parallel workers
 *
 * InputT: input element type, e.g. int
 * @N: parallelism
 * */
template <typename InputT, typename OutputT, int keypos, int vpos, unsigned N>
class WinSumSeq : public StatefulTransform<
	WinSumSeq<InputT,OutputT,keypos,vpos,N>,
	InputT, /* input */
	WinKeySeqFrag<OutputT, N> /* window result. let's be generic (for kvpair) */
> {

public :
	/* multi: =1 for fixed window;
	 * >1 for sliding window (x factor for the sliding window size)
	 */

	WinSumSeq(string name, int sideinfo, int multi = 1, WinSumMode mode = SUM_BYKEY, float k = -1.0f)
	: StatefulTransform<WinSumSeq<InputT,OutputT,keypos,vpos,N>, InputT, WinKeySeqFrag<OutputT,N>>(name, sideinfo),
		multi(multi), mode(mode), k_(k)
	{
		xzl_bug_on((mode == TOPV_BYKEY || mode == PERCENTILE_BYKEY) && (k_ <= 0 || k_ >= 1));
	}

	/* multiplier for supporting sliding window.
	 * = 1 for tumbling window.  */
	const int multi;

	const WinSumMode mode;

	const float k_; /* for topK, <0 if un init'd */

	/* aggregate -- supposed to be hidden in sec world */
	// xzl: static imposes lots of prog constraints (e.g. can't pass transform's info)
	WinKeySeqFrag<OutputT,N> const & aggregate_init(WinKeySeqFrag<OutputT,N>* winres) {

		xzl_bug_on(!winres);
		for (auto & s : winres->seqbufs)
			s.AllocBuf();

		return *winres;
	}

#if 0 /* still useful? see below... */
	/* Execute summation on 1 incoming window (a winkeyfrag), output a winkeyfrag (a window) to be combined into
	 * the internal state.
	 * the output winkeyfrag is partitioned N way for parallelism */
	WinKeySeqFrag<OutputT,N> sum_incoming_one(WinKeySeqFrag<InputT, 1> const &infrag)
	{

		xzl_bug_on(infrag.seqbufs.size() != 1);

//		EE("incoming frag win %u", infrag.w.start);

		switch (mode) {
			case TOPV_BYKEY:
			case PERCENTILE_BYKEY:
			case MEDV_BYKEY: {
#if 0
				xzl_bug_on_msg(N != 1, "TBD");
				SeqBuf<Record<OutputT>> s1 = infrag.seqbufs[0].template Sort2<vpos>(); /* first by value */
				SeqBuf<Record<OutputT>> s2 = s1.template Sort2<keypos>(); /* first by value */
				return WinKeySeqFrag<OutputT, N>(infrag.w, s2);
#endif
#if 0
				/* sort w/partition; sort each partition.*/
				vector<SeqBuf<Record<OutputT>>> v1 = infrag.seqbufs[0].template SortMultiOut<vpos>(N);
				vector<SeqBuf<Record<OutputT>>> v2;
				for (auto & vv : v1) {
					v2.push_back(vv.template Sort2<keypos>());
				}
				return WinKeySeqFrag<OutputT, N>(infrag.w, v2);
#endif
#if 1
				vector<SeqBuf<Record<OutputT>>> win_res =
					infrag.seqbufs[0].template SortMultiKey<keypos,vpos>(N);
				return WinKeySeqFrag<OutputT, N>(infrag.w, win_res);
#endif
				break;
			}
			case SUM_BYKEY: {
				/* XXX: this should return array instead of vector */
				vector<SeqBuf<Record<OutputT>>> win_res = infrag.seqbufs[0].template SumByKey<keypos, vpos>(N);
				return WinKeySeqFrag<OutputT, N>(infrag.w, win_res);
				break;
			}
			case SUM_ALL: {
				xzl_bug_on_msg(N!=1, "sumall into multiple bufs?");
				SeqBuf<Record<OutputT>> win_res = infrag.seqbufs[0].template SumAllToOne<vpos>();
				return WinKeySeqFrag<OutputT, N>(infrag.w, win_res);
				break;
			}
			case AVG_BYKEY: {
				vector<SeqBuf<Record<OutputT>>> win_res = infrag.seqbufs[0].
							template SumCount<keypos,vpos>(N, SB_SUMCOUNT1_PERKEY);
				return WinKeySeqFrag<OutputT, N>(infrag.w, win_res);
				break;
			}
			case UNIQUE_BYKEY : {
				vector<SeqBuf<Record<OutputT>>> win_res = infrag.seqbufs[0].
						template SumCount<keypos,vpos>(N, SB_UNIQUE_PERKEY);
#if 0
				vector<SeqBuf<Record<OutputT>>> win_res = infrag.seqbufs[0].
						template SumCount<keypos,vpos>(N, SB_UNIQUE_PERKEY);
				return WinKeySeqFrag<OutputT, N>(infrag.w, win_res);
#endif
				break;
			}
			case AVG_ALL: {
				vector<SeqBuf<Record<OutputT>>> win_res = infrag.seqbufs[0].
						template SumCount<keypos,vpos>(N, SB_AVG_ALL);
				return WinKeySeqFrag<OutputT, N>(infrag.w, win_res);
				break;
			}
			default:
				xzl_bug("unknown mode");
		}
		xzl_bug("unknown mode");
		return WinKeySeqFrag<OutputT, N>(); /* never reach here */
	};
#endif

	/* similar to above. but the incoming WinKeySeqFrag is already N-partitioned */
	WinKeySeqFrag<OutputT,N> sum_incoming_one(WinKeySeqFrag<InputT, N> const &infrag)
	{

		xzl_bug_on(infrag.seqbufs.size() != N);

		switch (mode) {
			/* sorted by primary/secondary. can't do incremental agg */
			case TOPV_BYKEY:
			case PERCENTILE_BYKEY:
			case MEDV_BYKEY: {
#if 0
				xzl_bug_on_msg(N != 1, "TBD");
				SeqBuf<Record<OutputT>> s1 = infrag.seqbufs[0].template Sort2<vpos>(); /* first by value */
				SeqBuf<Record<OutputT>> s2 = s1.template Sort2<keypos>(); /* first by value */
				return WinKeySeqFrag<OutputT, N>(infrag.w, s2);
#endif
#if 0
				/* sort w/partition; sort each partition.*/
				vector<SeqBuf<Record<OutputT>>> v1 = infrag.seqbufs[0].template SortMultiOut<vpos>(N);
				vector<SeqBuf<Record<OutputT>>> v2;
				for (auto & vv : v1) {
					v2.push_back(vv.template Sort2<keypos>());
				}
				return WinKeySeqFrag<OutputT, N>(infrag.w, v2);
#endif
				/* we assume incoming is already sorted by 2 keys. nothing we can do now
				 * XXX drop lower-ranked records?? */
				return infrag;
				break;
			}
			/* incremental agg possible */
			case TOPCOUNT_BYKEY:
			case SUM_BYKEY: {
				/* XXX: this should return array instead of vector */
				vector<SeqBuf<Record<OutputT>>> win_res(N);
				for (unsigned int i = 0; i < N; i++) {
					win_res[i] = infrag.seqbufs[i].template SumCount_1_OutBatch<keypos, vpos>(SB_SUM_PERKEY);
				}
				return WinKeySeqFrag<OutputT, N>(infrag.w, win_res);
				break;
			}
			case SUM_ALL: {
				xzl_bug("tbd. might just call AVG_ALL, which won't be too much more expensive");
				break;
			}
			case AVG_BYKEY: {
				vector<SeqBuf<Record<OutputT>>> win_res(N);
				for (unsigned int i = 0; i < N; i++)
					win_res[i] = infrag.seqbufs[i].template SumCount_1_OutBatch<keypos,vpos>(SB_SUMCOUNT1_PERKEY);
				return WinKeySeqFrag<OutputT, N>(infrag.w, win_res);
				break;
			}
			case UNIQUE_BYKEY : {
				vector<SeqBuf<Record<OutputT>>> win_res(N);
				for (unsigned int i = 0; i < N; i++)
					win_res[i] = infrag.seqbufs[i].template SumCount_1_OutBatch<keypos,vpos>(SB_UNIQUE_PERKEY);
				return WinKeySeqFrag<OutputT, N>(infrag.w, win_res);
				break;
			}
			case AVG_ALL: {
				vector<SeqBuf<Record<OutputT>>> win_res(N);
				for (unsigned int i = 0; i < N; i++)
					win_res[i] = infrag.seqbufs[i].template SumCount_1_OutBatch<keypos,vpos>(SB_SUMCOUNT1_ALL);
				return WinKeySeqFrag<OutputT, N>(infrag.w, win_res);
				break;
			}
			default:
				xzl_bug("unknown mode");
		}
	};

	/* run summation on a winkeyfrag (a window). Can specify @seqbuf_id to only run summation on
	 * a particular partition.
	 *
	 * the result may be used in future summation passes.
	 */
	void sum_internal_one(WinKeySeqFrag<InputT, N>* frag, unsigned int seqbuf_id) {
		xzl_bug_on(!frag || frag->seqbufs.size() != N);
		xzl_bug_on(seqbuf_id >= frag->seqbufs.size());

		switch (mode) {
			case TOPV_BYKEY:
			case PERCENTILE_BYKEY:
			case MEDV_BYKEY:
				/* do nothing? since it is already sorted */
				break;
			case SUM_BYKEY:
			case TOPCOUNT_BYKEY:
				frag->template SumByKeyInplace<keypos, vpos>(seqbuf_id);
				break;
			case SUM_ALL:
				frag->template SumAllInplace<vpos>(seqbuf_id /* should be 0 */);
				break;
			case AVG_BYKEY:
				frag->template SumCountInplace<keypos, vpos>(seqbuf_id, SB_SUMCOUNT_PERKEY);
				break;
			case UNIQUE_BYKEY:
				frag->template SumCountInplace<keypos, vpos>(seqbuf_id, SB_UNIQUE_PERKEY);
				break;
			case AVG_ALL:
				frag->template SumCountInplace<keypos, vpos>(seqbuf_id, SB_SUMCOUNT_ALL);
				break;
			default:
				xzl_bug("unknown mode");
		}
	}

	/* run the final summation on a keyfrag (a window), on a given partition
	 * the result is ready for output (to downstream)
	 * often, this is invoked on part 0, after combing all partitions of a winkeyfrag to part 0.
	 */
	void sum_internal_final(WinKeySeqFrag<InputT, N>* frag, unsigned int part_id = 0) {
		xzl_bug_on(!frag || frag->seqbufs.size() != N);
		xzl_bug_on(part_id >= frag->seqbufs.size());

		switch (mode) {
			case MEDV_BYKEY:
				frag->template MedByKeyInplace<keypos, vpos>(part_id, SB_MED_PERKEY, 0.5);
				break;
			case TOPV_BYKEY:
				frag->template MedByKeyInplace<keypos, vpos>(part_id, SB_TOPK_PERKEY, k_);
				break;
			case PERCENTILE_BYKEY:
				frag->template MedByKeyInplace<keypos, vpos>(part_id, SB_PERCENTILE_PERKEY, k_);
				break;
			case SUM_BYKEY:
				frag->template SumByKeyInplace<keypos, vpos>(part_id);
				break;
			case TOPCOUNT_BYKEY:
				frag->template SumByKeyInplace<keypos, vpos>(part_id);
				frag->template Sorted2<vpos>(part_id); /* sort by value again */
				break;
			case SUM_ALL:
				frag->template SumAllInplace<vpos>(part_id /* should be 0 */);
				break;
			case AVG_BYKEY:
				frag->template SumCountInplace<keypos, vpos>(part_id, SB_AVG_PERKEY);
//				xzl_bug_on(frag->seqbufs[0].sorted_ == -1);
				xzl_bug_on(!frag->seqbufs[0].IsSorted());
				break;
			case UNIQUE_BYKEY:
				frag->template SumCountInplace<keypos, vpos>(part_id, SB_UNIQUE_PERKEY);
				xzl_bug_on(!frag->seqbufs[0].IsSorted());
				break;
			case AVG_ALL:
				frag->template SumCountInplace<keypos, vpos>(part_id, SB_AVG_ALL);
				break;
			default:
				xzl_bug("unknown mode");
		}
	}

	/* combine two winkeyfrags (of the same window), each may have N partitions.
	 *
	 * combine @others into @mine.
	 *
	 * This will replace the underlying buf for @mine
	 */
	void combine(WinKeySeqFrag<OutputT,N> & mine,
											WinKeySeqFrag<OutputT,N> const & others) {

		/* refrain from summation as follows. instead, hold until state flush */
		// 	mine.Sumed(); /* aggregation again across all seqbufs */

		xzl_bug_on_msg(mine.w.start != others.w.start, "bug: window mismatch");

		switch (mode) {
			case MEDV_BYKEY:
			case TOPV_BYKEY:
			case PERCENTILE_BYKEY:
				/* XXXX special merge. by key and then by value. right now it's incorrect */
				for (unsigned int i = 0; i < mine.seqbufs.size(); i++)
					mine.template Merged2Locked<keypos>(others, i);
				break;
			case SUM_BYKEY:
			case TOPCOUNT_BYKEY:
				for (unsigned int i = 0; i < mine.seqbufs.size(); i++)
					mine.template Merged2Locked<keypos>(others, i);
				break;
			case SUM_ALL:
				for (unsigned int i = 0; i < mine.seqbufs.size(); i++)
					mine.ConcatenatedLocked(others, i);
				break;
			case AVG_BYKEY:
			case UNIQUE_BYKEY:
				for (unsigned int i = 0; i < mine.seqbufs.size(); i++)
					mine.template Merged2Locked<keypos>(others, i);
				break;
			case AVG_ALL:
				for (unsigned int i = 0; i < mine.seqbufs.size(); i++)
					mine.ConcatenatedLocked(others, i);
				break;
			default:
				xzl_bug("unknown mode");
		}
	}

	void ExecEvaluator(int nodeid, EvaluationBundleContext *c,
										 std::shared_ptr<BundleBase> bundle) override {

		WinSumSeqEval<InputT, OutputT, keypos, vpos, N> eval(nodeid);
		eval.evaluate(this, c, bundle);
	}

};

#endif //CREEK_WINSUMSEQ_H_H
