//
// Created by xzl on 8/27/17.
//
// cf WinSumEval.h

#ifndef CREEK_WINSUMSEQEVAL_H_H
#define CREEK_WINSUMSEQEVAL_H_H

#include "creek-types.h"
#include "core/EvaluationBundleContext.h"
#include "core/SingleInputTransformEvaluator.h"
#include "WinSumSeq.h"

using namespace creek;

template <class InputT, class OutputT, int keypos, int vpos, unsigned N /*parallelism*/>
class WinSumSeqEval
	: public SingleInputTransformEvaluator<WinSumSeq<InputT, OutputT, keypos, vpos, N>,
	WinSeqBundle<InputT, N>, WinSeqBundle<OutputT, 1>>
{
	using InputBundleT = WinSeqBundle<InputT, N>;
	using OutputBundleT = WinSeqBundle<OutputT, 1>;
	using TransformT = WinSumSeq<InputT, OutputT, keypos, vpos, N>;

public:
	WinSumSeqEval(int node)
			: SingleInputTransformEvaluator<TransformT,InputBundleT,OutputBundleT>(node) { }

private:

	/* Will update @frag in place. Results in frag.seqbufs[0]
	 * cf: ReduceTopKParallel */
	WinKeySeqFrag<OutputT> agg_parallel(WinKeySeqFrag<OutputT,N> & frag, EvaluationBundleContext* c, TransformT* trans) {
//		using worker_res_t = SeqBuf<OutputT>;
		unsigned int total_tasks = N; // including *this* thread
		atomic<int> flag2(0);
//		vector <std::future < worker_res_t >> futures;
		vector <std::future<void>> futures;

		xzl_bug_on_msg(total_tasks > c->num_workers, "no enough worker threads. increases cores=N");

		xzl_bug_on(total_tasks != frag.seqbufs.size());

		/* this thread has task_id = 0; extra workers are 1..total_task-1 */

		W("reduce starts");

		k2_measure("reduce_start");
		for (unsigned int task_id = 1; task_id < total_tasks; task_id++) {
			futures.push_back( // store a future
					c->executor_.push(TASK_QUEUE_EVAL,
							[trans, &frag, task_id, &flag2](int id)
							/* ----- lambda  ---- */
							{
									int expected = 0;
									int first = 0;
									if (flag2.compare_exchange_strong(expected, 1)) {
										k2_measure("1st_exec");
										first = 1;
									}

//									frag.template SumByKeyInplace<keypos, vpos>(task_id); /* in place */
									trans->sum_internal_one(&frag, task_id);

									if (first)
										k2_measure("1st_done");
							}
							/* ---- end of lambda ---- */
					)
			);
		}

		k2_measure("task_cr_done");

		W("total %lu futures...", futures.size());

		/* the main thread does its share of work */
//		frag.template SumByKeyInplace<keypos, vpos>(0);
		trans->sum_internal_one(&frag, 0);
		W("task0: sum ends. join starts...");

		for (auto && f : futures) {
			f.get();
		}
		k2_measure("join_done");
		W("join done");
		futures.clear();

		/* single thread. unlocked.
		 * XXX call combine() instead XXX
		 * XXX overkill for SUM_ALL XXX
		 * XXX for med, need a specific Merge
		 * */
		frag.template MergedByKey<keypos>(); /* merge all partitions into part 0  */

		W("final merge ends; final agg starts");

//		frag.template SumByKeyInplace<keypos, vpos>(0);
		trans->sum_internal_final(&frag);

		WinKeySeqFrag<OutputT> ret;
		ret.w = frag.w;
		ret.seqbufs[0] = frag.seqbufs[0]; /* copy pbuf+sorted over */
		k2_measure("final_agg_end");
		W("final agg end");

		/* todo: topK */
		k2_measure_flush();

		// it's possible we're unsorted (e.g. AVG_ALL)
//		xzl_bug_on(ret.seqbufs[0].sorted_ == -1);

		return ret;
	}

	/* Flush internal state (partial aggregation results) given the external
		* wm.
		*
		* @up_wm: the external wm.
		*
		* return: the local wm to be updated after the flushing.
		*/
	etime flushStateFixed(TransformT* trans, const etime up_wm,
												vector<shared_ptr<OutputBundleT>>* output_bundles,
												EvaluationBundleContext* c, bool purge = true) {

		xzl_assert(output_bundles);

		W("enters");

		typename TransformT::AggResultT winmap;
		etime in_min_ts = trans->RetrieveState(&winmap, purge, up_wm);

		// no window is being closed.
		if (winmap.size() == 0) {
			/* internal state empty, and we just observed @up_wm: the transform's local
			 * wm should be updated to @up_wm.*/
			if (in_min_ts == creek::etime_max)
				return up_wm;
			else /* nothing flushed but internal state exists */
				return min(in_min_ts, up_wm);
		}

		/*  some windows are closed */
		{
			auto output_bundle = make_shared<OutputBundleT>(this->_node);

			// Iterate windows, each of which is associated with an aggregation
			// result OutputT.
			unsigned long totalrec = 0;

			for (auto it = winmap.begin(); it != winmap.end(); it++) {
				auto & win = it->first;
				auto & frag = it->second;

				W("flush a win, start %u %u", win.start, frag.w.start);
				xzl_assert(win.start == frag.w.start);

				/* final aggregation
				 * each window state encloses multiple seqbufs. parallel execution */
//				frag.Sumed();
				auto frag_1seqbuf = agg_parallel(frag, c, trans);
				output_bundle->vals[win] = frag_1seqbuf;

//				if (trans->mode == AVG_ALL)
//					EE("avg_all output one bundle. sz %lu", frag_1seqbuf.seqbufs[0].Size());

				totalrec += frag_1seqbuf.seqbufs[0].Size();
			}

			/* update stat */
			trans->record_counter_.fetch_add(totalrec, std::memory_order_relaxed);

			if (totalrec) /* only emit when >0 records */
				output_bundles->push_back(output_bundle);
		}

		W("done");

		return min(in_min_ts, up_wm);
	}

	etime flushStateSliding(TransformT* trans, const etime up_wm,
													vector<shared_ptr<OutputBundleT>>* output_bundles,
													EvaluationBundleContext* c)
	{
		assert(output_bundles);
		assert(trans->multi > 1);

		typename TransformT::AggResultT winmap;
		etime in_min_ts = trans->RetrieveStateSliding(&winmap, up_wm, trans->multi);

		// no window is being closed.
		if (winmap.size() == 0) {
			/* internal state empty, and we just observed @up_wm: the transform's local
			 * wm should be updated to @up_wm.*/
			if (in_min_ts == max_date_time)
				return up_wm;
			else /* nothing flushed but internal state exists */
				return min(in_min_ts, up_wm);
		}

		/*  some windows are closed & returned.
		 *
		 *  XXX handle the cases where some windows are missing at the end of the
		 *  returned window range (can't miss in the middle due to growth on
		 *  demand.
		 * */
		{
			// each output record also must carry a ts.
			auto output_bundle = make_shared<OutputBundleT>(
					winmap.size() * 4, /* just a guess of the # of records in the output bundle */
					this->_node);

			unsigned long totalrec = 0;

			int n = winmap.size() - trans->multi;
			assert(n > 0); /* XXX what no sufficient deltas are returned? */

			for (int i = 0; i < n; i++) {
				// Iterate sliding windows, each of which encompasses @trans->multi deltas;
				// each delta has a window state (result OutputT).
				Window outwin = std::next(winmap.begin(), i + trans->multi - 1)->first;
				WinKeySeqFrag<OutputT,N> frag(outwin);

				trans->aggregate_init(&frag);

				/* Combine all deltas
				 * We can be smarter: caching intermediate results etc */
				for (auto it = std::next(winmap.begin(), i);
						 it != std::next(winmap.begin(), i + trans->multi); it++) {
					dynamic_cast<TransformT *>(this)->combine(frag, it->second);
				}

				/* final aggregation -- parallel */
				auto frag_1seqbuf = agg_parallel(frag, c, trans);

				output_bundle->vals[outwin] = frag_1seqbuf;

				totalrec += frag_1seqbuf.seqbufs[0].Size();
			}

			/* update stat */
			trans->record_counter_.fetch_add(totalrec, std::memory_order_relaxed);

			if (totalrec) /* only emit when >0 records */
				output_bundles->push_back(output_bundle);
		}

		return min(in_min_ts, up_wm);
	}

public:
	etime flushState(TransformT* trans, const etime up_wm,
									 vector<shared_ptr<OutputBundleT>>* output_bundles,
									 EvaluationBundleContext* c, bool purge = true) override
	{
		if (trans->multi == 1)
			return flushStateFixed(trans, up_wm, output_bundles, c, purge);
		else
			return flushStateSliding(trans, up_wm, output_bundles, c);
	}

	/* do local aggregation and then combine to the transform.
		* XXX the use of AggResultT (MT safe) may be an overkill, since we don't need
		* MT safe here. */
	bool evaluateSingleInput (TransformT* trans,
														shared_ptr<InputBundleT> input_bundle,
														shared_ptr<OutputBundleT> output_bundle) override {

		using AggResultT = typename TransformT::AggResultT;

		AggResultT aggres;

		/* -- heavy lifting is done w/o locking the transform -- */

		for (auto it = input_bundle->vals.begin(); it != input_bundle->vals.end();
				 it ++)  /* for each window in input bundle */
		{
			auto && win = it->first;
			auto && pfrag = it->second;
			assert (aggres.count(win) == 0 && "bug? duplicate windows in same bundle");
			/* we assume the input WinKeyFrag only encloses a single seqbuf */

			aggres[win] = trans->sum_incoming_one(pfrag);
		}

		/* -- combine -- */
		trans->AddAggregatedResult(aggres); /* will lock and call into combine() for each window */

		VV("WinSum core %d: processed an incoming bundle", sched_getcpu());

		return false; // no output bundle
	}

};
#endif //CREEK_WINSUMSEQEVAL_H_H
