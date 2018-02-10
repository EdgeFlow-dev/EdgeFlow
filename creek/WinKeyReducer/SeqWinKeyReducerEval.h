//
// Created by xzl on 3/17/17.
//

#ifndef SEQ_WINDOWED_KEY_RED_EVAL_H
#define SEQ_WINDOWED_KEY_RED_EVAL_H

#define K2_NO_MEASUREMENT 1

extern "C" {
#include "measure.h"
}

#include "core/EvaluationBundleContext.h"
#include "core/SingleInputTransformEvaluator.h"
#include "WinKeyReducer/SeqWinKeyReducer.h"

/* support different output bundle formats. if @RecordBundle, the Window ts is encoded in the 1st item of the
 * value pair
 */
template <class KVIn,
		template<class> class InputWinKeyFragT,
		/* can be specialized based on key/val distribution */
		template<class> class InternalWinKeyFragT,
		class KVOut,
		template<class> class OutputBundleT_   /* WindowsBundle or RecordBundle */
>
class SeqWinKeyReducerEval
		: public SingleInputTransformEvaluator<
				SeqWinKeyReducer<KVIn, InputWinKeyFragT, InternalWinKeyFragT, KVOut, OutputBundleT_>,
				WindowsKeyedBundle<KVIn, InputWinKeyFragT>,
				OutputBundleT_<KVOut>			/* reduce results. often small */
		>
{

	using KVPair = KVIn;
	using KVPairOut = KVOut;
	using TransformT = SeqWinKeyReducer<KVPair, InputWinKeyFragT, InternalWinKeyFragT, KVOut, OutputBundleT_>;
	using InputBundleT = WindowsKeyedBundle<KVPair, InputWinKeyFragT>;
	// the output should also be indexed by window.
	using OutputBundleT = OutputBundleT_<KVOut>;

public:
	SeqWinKeyReducerEval(int node)
			: SingleInputTransformEvaluator<TransformT,InputBundleT,OutputBundleT>(node) { }

#ifndef NDEBUG
	ptime last_punc_ts;
	bool once= true;
#endif

private:
	/* converter: result record --> output record.
	 * default: result reord == output record
	 * can be specialized based on the need for output (e.g. assemble a record where first == window, etc.)*/
	Record<KVOut> const makeRecord(KVPair const & value, Window const & win) {
		return Record<KVOut>(value, win.window_end());
	}

	void ReduceSerial(TransformT* trans,
										typename TransformT::AggResultT const & winmap,
										vector<shared_ptr<OutputBundleT>>* output_bundles)
	{

	}

	void ReduceTopKSerial(TransformT* trans,
												typename TransformT::AggResultT const & winmap,
												vector<shared_ptr<OutputBundleT>>* output_bundles)
	{

	}

private:
	/* task_id: zero based.
	 * return: <start, cnt> */
	static pair<int,int> get_range(int num_items, int num_tasks, int task_id) {

		/* not impl yet */
		xzl_bug_on(num_items == 0);

		xzl_bug_on(task_id > num_tasks - 1);

		int items_per_task  = num_items / num_tasks;

		/* give first @num_items each 1 item */
		if (num_items < num_tasks) {
			if (task_id <= num_items - 1)
				return make_pair(task_id, 1);
			else
				return make_pair(0, 0);
		}

		/* task 0..n-2 has items_per_task items. */
		if (task_id != num_tasks - 1)
			return make_pair(task_id * items_per_task, items_per_task);

		if (task_id == num_tasks - 1) {
			int nitems = num_items - task_id * items_per_task;
			xzl_bug_on(nitems < 0);
			return make_pair(task_id * items_per_task, nitems);
		}

		xzl_bug("bug. never reach here");
		return make_pair(-1, -1);
	}

	void ReduceTopKParallel(TransformT* trans,
													typename TransformT::AggResultT const & winmap,
													vector<shared_ptr<OutputBundleT>>* output_bundles,
													EvaluationBundleContext* c, bool do_topK = true);


	/* Flush internal state (partial aggregation results) given the external
	 * wm.
	 *
	 * @up_wm: the external wm.
	 * return: the local wm to be updated after the flushing.
	 *
	 * Ref: WindowedSumEvaluator1::flushState()
	 *
	 * To be specialized
	 */
public:
	ptime flushState(TransformT* trans, const ptime up_wm,
									 vector<shared_ptr<OutputBundleT>>* output_bundles,
									 EvaluationBundleContext* c, bool purge = true) override
	{
		assert(output_bundles);

		k2_measure("flush_state");

		//hym: XXX how to know the min_ts??? XXX
		//hym: XXX shall we just flush state according to up_wm? I think so.

		// all closed windows are copied to winmap
		typename TransformT::AggResultT winmap;
		ptime in_min_ts = trans->RetrieveState(&winmap, up_wm, up_wm);

		// no window is being closed.
		if (winmap.size() == 0) {
			k2_measure_flush();
			if (in_min_ts == max_date_time) /* internal state empty */
				return up_wm;
			else /* nothing flushed but internal state exists */
				return min(in_min_ts, up_wm);
		}

		/* -- some windows are closed -- */
		ReduceTopKParallel(trans, winmap, output_bundles, c, true);

		return min(in_min_ts, up_wm);

	}//end flushState

	/* eval does some local computation
	 * (sort keys, aggregation of v containers, reduction ...) then add to the
	 * transform's state.
	 */
	bool evaluateSingleInputReduce (TransformT* trans,
																	shared_ptr<InputBundleT> input_bundle,
																	shared_ptr<OutputBundleT> output_bundle)
	{
		/* NB: local and intput_bundle only have to have same vcontainer */
		typename TransformT::LocalAggResultT local; /* contain multi windows */

		/* XXX should do single thread reduction?
		 *
		 * instead of do seq buffer of kv, should do KVInter
		 * */
		for (auto && w : input_bundle->vals) {
			auto && win = w.first;
			auto && frag = w.second;
//			TransformT::local_aggregate_init(&local[win]);
			local[win] = frag;  // simply copy the ptr over
		}

		// multi windows
		trans->AddAggregatedResult(local);

#if 0
		/* debugging */
		int total_keys = 0; /* may out of multiple windows */
		for (auto && w : local) {   // partial_results_
			auto && frag = w.second;
			total_keys += frag->vals.size();
		}
		EE("total_keys %d", total_keys);
#endif

		return false;
	}

	/* the data bundle path */
	bool evaluateSingleInput (TransformT* trans,
														shared_ptr<InputBundleT> input_bundle,
														shared_ptr<OutputBundleT> output_bundle)  override
	{
		return evaluateSingleInputReduce(trans, input_bundle, output_bundle);
	}

}; //end SeqWinKeyReducerEval

#endif // SEQ_WINDOWED_KEY_RED_EVAL_H
