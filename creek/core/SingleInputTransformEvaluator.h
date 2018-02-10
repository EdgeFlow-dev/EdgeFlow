#ifndef SINGLE_INPUT_T_EVAL_H
#define SINGLE_INPUT_T_EVAL_H

#include "EvaluationBundleContext.h"
#include "TransformEvaluator.h"
#include "BundleContainer.h"

using namespace creek;

template<class TransformT, class InputBundleT, class OutputBundleT>
class SingleInputTransformEvaluator: public TransformEvaulator<TransformT> {

public:
	SingleInputTransformEvaluator(int node) {
		ASSERT_VALID_NUMA_NODE(node);
		this->_node = node;
	}

	// process one incoming bundle.
	// to be implemented by concrete evaluator.
	// @output_bundle: an allocated output bundle to deposit records into
	// @return: true if a consumer should be scheduled to consume
	// the output bundle

	/* Concrete transform should override at least one of the following */

	/* legacy */
	virtual bool evaluateSingleInput(TransformT* trans,
																	 shared_ptr<InputBundleT> input_bundle,
																	 shared_ptr<OutputBundleT> output_bundle) = 0;

	virtual bool evaluateSingleInput(TransformT* trans,
																	 shared_ptr<InputBundleT> input_bundle,
																	 shared_ptr<OutputBundleT> output_bundle,
																	 EvaluationBundleContext* c /* need this to access tp */) {

		return evaluateSingleInput(trans, input_bundle, output_bundle);

	}


	/*
	 * Given an "external" wm, attempt to flush the internal state.
	 *
	 * caller holds statelock.
	 *
	 * @up_wm: the wm considering received punc and inflight bundles.
	 * the function should attempt to flush based on @up_wm.
	 *
	 * @conlocked: whether the caller holds the conlock
	 * Often this func does not need this to update local wm
	 *
	 * @output_bundles: a local container to deposite output bundles
	 *
	 * return: the updated trans wm after flush.
	 * NB: this func should not update trans wm directly.
	 *
	 * for stateless transform, no flush is
	 * done and @up_wm is returned. for stateful transform, if the internal
	 * state cannot be flushed "up to" @up_wm, an earlier wm should be returned.
	 * This happens, e.g. when a window cannot be closed as of now.
	 *
	 * But the return @wm should not be later than @up_wm per definition.
	 *
	 */

	/* may grab the transform's internal lock */
	virtual etime flushState(TransformT* trans, const etime up_wm,
													 vector<shared_ptr<OutputBundleT>>* output_bundles,
													 EvaluationBundleContext* c, bool purge = true) {
		return up_wm;
	}

	/* Process a punc. Flush the internal state and assign a punc to downstream
	 * as needed.
	 *
	 * This incoming punc may be a partial punc (for JD). If so, update the joint wm as needed, and propagate the
	 * joint wm to downstream.
	 *
	 * The punc emitted to downstream may not be immediately retrieved by downstream.
	 */
#if 0 /* old impl */
	bool process_punc(TransformT* trans, EvaluationBundleContext* c,
			shared_ptr<Punc> punc, PValue *out)
	{
		bool out_punc = false;

#ifdef DEBUG /* extra check: punc must be monotonic */
		if (trans->punconce) {
			trans->punconce = false;
		} else {
			if (punc->min_ts < trans->last_punc_ts) {
				EE("%s (node %d) get punc: %s. last_punc_ts %d", trans->name.c_str(), this->_node,
					to_simple_string(punc->min_ts).c_str(), trans->last_punc_ts);
				xzl_bug("bug: punc regression.");
			}
		}
		trans->last_punc_ts = punc->min_ts;
#endif

		W("%s (node %d) get punc: %s. last_punc_ts %d", trans->name.c_str(), this->_node,
				to_simple_string(punc->min_ts).c_str(), trans->last_punc_ts);
		//assert(punc->min_ts > trans->GetWatermarkSafe());

#ifdef MEASURE_LATENCY
		punc->mark("retrieved by: " + trans->name);
#endif

		/* We retrieved a punc. This only happens after all bundles of the enclosing
		 * containers are consumed.
		 */

		/* --- flush internal state --- */
		vector<shared_ptr<OutputBundleT>> flushed_bundles;
		etime new_wm = flushState(trans, punc->min_ts, &flushed_bundles, c, true);

		/* XXX ugly XXX */
		vector<shared_ptr<BundleBase>> flushed_bundles_(flushed_bundles.begin(),
				flushed_bundles.end());

		if (out->consumer) {
			switch (trans->get_side_info()) {
				case SIDE_INFO_NONE:
					/* see comment in proces_punc above */
				case SIDE_INFO_L:
				case SIDE_INFO_R:
				case SIDE_INFO_JD:
				case SIDE_INFO_JDD:
					trans->depositBundlesDownstream(out->consumer, punc, flushed_bundles_);
					break;
				default:
					xzl_bug("unsupported mode");
					break;
			}
		}

		/* --- assign punc after data bundles --- */
		if (trans->SetWatermarkSafe(new_wm)) { /* local wm changed */
			if (out->consumer) {
				shared_ptr<Punc> new_punc = make_shared<Punc>(new_wm, punc->node);
#ifdef MEASURE_LATENCY
				new_punc->inherit_markers(*punc);
#endif
				switch (trans->get_side_info()) {
					case SIDE_INFO_NONE:
					/* j's upstream, for which depositOnePuncDownstream() will call L/R for it internally */
					case SIDE_INFO_L:
					case SIDE_INFO_R:
					case SIDE_INFO_JDD:
						trans->depositOnePuncDownstream(out->consumer, punc, new_punc, new_punc->node);
						break;
					case SIDE_INFO_JD:
						if(punc->get_list_info() == SIDE_INFO_L) {
							trans->left_wm = punc->min_ts;
						}else if(punc->get_list_info() == SIDE_INFO_R) {
							trans->right_wm = punc->min_ts;
						}else{
							std::cout << "This should never happen" << std::endl;
							abort();
							assert(false && "Punc has wrong side info in JD");
						}
						trans->depositOnePuncDownstream(out->consumer, punc, new_punc, new_punc->node);
						break;
					default:
						xzl_bug("unsupported mode");
						break;
				}

				out_punc = true;
			} else {  /* no downstreawm (sink?) print out something */
				W("		%s passthrough a punc: %s", trans->name.c_str(),
						to_simple_string(trans->GetWatermarkSafe()).c_str());
	#ifdef MEASURE_LATENCY
				punc->dump_markers();
	#endif
				c->OnSinkGetWm(punc->min_ts);
			}
		} else { /* local wm unchanged. */
			/*
			 * if the incoming wm is dropped (does not trigger wm output), we do
			 * not trace it for latency.
			 */
			if (out->consumer) {
				trans->cancelPuncDownstream(out->consumer, punc);
			} else {
				c->OnSinkGetWm(punc->min_ts);
				//bug("will this happen?");
			}
		}

		/* now the received punc is consumed and can be destroyed. This allows
		 * the enclosing container to be closed and the next punc to be retrieved
		 * by this tran.
		 */
//		auto t = punc->refcnt->fetch_sub(1);
//		t = t; assert(t == 1);

		long expected = PUNCREF_RETRIEVED;
		if (!punc->refcnt->compare_exchange_strong(expected, PUNCREF_CONSUMED)) {
			bug("bad punc's refcnt?");
		}

		/* --- spawning ---- */
		for (auto & b : flushed_bundles) {
			b = b; assert(b);
			c->SpawnConsumer();
		}

		if (out_punc)
			c->SpawnConsumer();

		return out_punc;
	}
#endif

	bool process_punc(TransformT* trans, EvaluationBundleContext* c,
										shared_ptr<Punc> punc, PValue *out)
	{
		bool out_punc = false;

#ifdef DEBUG /* extra check: punc must be monotonic */
		if (trans->punconce) {
			trans->punconce = false;
		} else {
			if (punc->min_ts < trans->last_punc_ts) {
				EE("%s (node %d) get punc: %s. last_punc_ts " ETIME_FMT, trans->name.c_str(), this->_node,
					 to_simple_string(punc->min_ts).c_str(), trans->last_punc_ts);
				xzl_bug("bug: punc regression.");
			}
		}
		trans->last_punc_ts = punc->min_ts;
#endif

		W("%s (node %d) get punc: %s listinfo %d. last_punc_ts " ETIME_FMT,
			trans->name.c_str(), this->_node,
			to_simple_string(punc->min_ts).c_str(), punc->get_list_info(),
			trans->last_punc_ts);

#ifdef MEASURE_LATENCY
		punc->mark("retrieved by: " + trans->name);
#endif

		/* We retrieved a punc. This only happens after all bundles of the enclosing
		 * containers are consumed.
		 */

		vector<shared_ptr<OutputBundleT>> flushed_bundles;
		etime new_wm = 0;
		auto sideinfo = trans->get_side_info();
		auto listinfo = punc->get_list_info();

		/* --- flush internal state --- */
		if (sideinfo == SIDE_INFO_JD) {
			/* punc is partial. compute a joint wm  & flush */
			xzl_bug_on(listinfo != LIST_L && listinfo != LIST_R);
			etime new_joint = trans->SetPartialWatermarkSafe(punc->min_ts, listinfo);
			if (new_joint > trans->GetWatermarkSafe()) {
				/* only flush when joint wm advances */
				new_wm = flushState(trans, new_joint, &flushed_bundles, c, true);
				xzl_bug_on(new_wm > new_joint);
			}
		} else { /* other type of trans (including JDD): wm is joint.
 							* flush based on punc */
			xzl_bug_on(listinfo != LIST_ORDERED);
			new_wm = flushState(trans, punc->min_ts, &flushed_bundles, c, true);
		}

		/* XXX ugly XXX */
		vector<shared_ptr<BundleBase>> flushed_bundles_(flushed_bundles.begin(),
																										flushed_bundles.end());

		/* deposit flushed bundles to downstream */
		if (!flushed_bundles_.empty() && out->consumer) {
			trans->depositBundlesDownstream(out->consumer, punc /* input */,
																			flushed_bundles_ /*output*/);
		}

		/* send new punc (must be joint wm) after data bundles. */
		if (new_wm && trans->SetWatermarkSafe(new_wm)) { /* there's a new wm and trans wm changed */
			W("%s: trans wm changed", trans->name.c_str());
			if (out->consumer) {
				shared_ptr<Punc> new_punc = make_shared<Punc>(new_wm, punc->node);
#ifdef MEASURE_LATENCY
				new_punc->inherit_markers(*punc);
#endif
				switch (trans->get_side_info()) {
					case SIDE_INFO_NONE:
						/* j's upstream, for which depositOnePuncDownstream() will call L/R for it internally */
					case SIDE_INFO_L:
					case SIDE_INFO_R:
					case SIDE_INFO_JD:
					case SIDE_INFO_JDD:
						new_punc->set_list_info(LIST_ORDERED);
						trans->depositOnePuncDownstream(out->consumer, punc, new_punc, new_punc->node);
						break;
					default: /* J, handled in JoinSeqEval */
						xzl_bug("unsupported mode");
						break;
				}
				out_punc = true;
			} else {  /* no downstreawm (sink?) print out something */
				W("		%s passthrough a punc: %s", trans->name.c_str(),
					to_simple_string(trans->GetWatermarkSafe()).c_str());
#ifdef MEASURE_LATENCY
				punc->dump_markers();
#endif
				c->OnSinkGetWm(punc->min_ts);
			}
		} else {
			/* trans wm unchanged, or only partial wm advances but joint wm does not.
			 * don't need to send the partial wm to dowmstream
			 * if the incoming wm is dropped (does not trigger wm output), we do
			 * not trace it for latency.
			 */
			W("%s: trans wm unchanged", trans->name.c_str());
			if (out->consumer) {
				if (!new_wm) /* if no new joint wm, this must be JD */
					xzl_bug_on(sideinfo != SIDE_INFO_JD);
				else /* otherwise, cancel punc */
					trans->cancelPuncDownstream(out->consumer, punc);
				out_punc = false;
			} else { /* no downstreawm (this is sink) print out something */
//				W("		%s passthrough a punc: %s", trans->name.c_str(),
//					to_simple_string(trans->GetWatermarkSafe()).c_str());
//#ifdef MEASURE_LATENCY
//				punc->dump_markers();
//#endif
				c->OnSinkGetWm(punc->min_ts);
			}
		}

		/* Now the old (received) punc is consumed and can be destroyed. This allows
		 * the enclosing container to be closed and the next punc to be retrieved
		 * by this tran.
		 */
		long expected = PUNCREF_RETRIEVED;
		if (!punc->refcnt->compare_exchange_strong(expected, PUNCREF_CONSUMED)) {
			bug("bad punc's refcnt?");
		}

		/* --- spawning tasks ---- */
		for (auto & b : flushed_bundles) {
			b = b; assert(b);
			c->SpawnConsumer();
		}

		if (out_punc)
			c->SpawnConsumer();

		return out_punc;
	}

	bool process_bundle(TransformT* trans, EvaluationBundleContext* c, shared_ptr<InputBundleT> input_bundle, PValue *out)
	{

		bool ret = false;
		xzl_assert(out);

		vector<shared_ptr<OutputBundleT>> output_bundles;
		/* contains the output bundle */
		shared_ptr<OutputBundleT> output_bundle = nullptr;

		int output_size = (input_bundle->size() >= 0 ? input_bundle->size() : 128);

		output_bundle = make_shared<OutputBundleT>(output_size,
																							 input_bundle->node); /* same node as the input bundle */

		if (evaluateSingleInput(trans, input_bundle, output_bundle, c))
			output_bundles.push_back(output_bundle);

		/* No extra action needed here. If the container owning the input bundle
		 * becomes empty because of the consumption here, next getOneBundle() will return the container's punc.
		 * At that time, eval can flush the state & emit bundles then.
		 */

		if (!output_bundles.empty()) {
			vector<shared_ptr<BundleBase>> output_bundles_ (output_bundles.begin(),
																											output_bundles.end());
			ret = true;
			switch (trans->get_side_info()) {
				case SIDE_INFO_NONE:
					/* see comment in proces_punc above */
				case SIDE_INFO_L:
				case SIDE_INFO_R:
				case SIDE_INFO_JD:
				case SIDE_INFO_JDD:
					trans->depositBundlesDownstream(out->consumer, input_bundle, output_bundles_);
					break;
				default: /* J, handled in its own eval */
					xzl_bug("unsupported mode");
					break;
			}
		}

		/* the input bundle is consumed */
		auto oldref = input_bundle->refcnt->fetch_sub(1);
		if (oldref <= 0) {
			EE("bug: %s: oldref is %ld <= 0. container %lx",
				 trans->name.c_str(), oldref,
				 (unsigned long)(input_bundle->container.load(memory_order::memory_order_relaxed)));
			trans->dump_containers("bug");
			xzl_bug("bug");
		}

		for (auto & b : output_bundles) {
			xzl_bug_on(!b);
			c->SpawnConsumer();
		}
		return ret;
	}

public:
	void evaluate(TransformT* trans, EvaluationBundleContext* c,
								shared_ptr<BundleBase> bundle_ptr = nullptr) override {

		assert(bundle_ptr && "must specify bundleptr. is old interface used?");

		assert(typeid(*trans) == typeid(TransformT));

		PValue* in1 = trans->getFirstInput();
		xzl_bug_on(!in1);
		auto out = trans->getFirstOutput();
		xzl_bug_on(!out);

		/* NB: Even sinks may have valid output, but that out has null consumer.
		 * See apply1() */

		/* at this time, the numa node of this eval must be determined. */
		ASSERT_VALID_NUMA_NODE(this->_node);

		vector<shared_ptr<OutputBundleT>> flushed_bundles;

#ifndef INPUT_ALWAYS_ON_NODE0
		/* this will fail if we don't have separate input queues and
		 * let evaluators to freely grab bundles from the queue...
		 */
		assert(this->_node == bundle_ptr->node);
#endif

		/* the bundle_ptr could be a data bundle or punc */
		auto input_bundle = dynamic_pointer_cast<InputBundleT>(bundle_ptr);

		if (!input_bundle) { /* punc path */
			auto punc = dynamic_pointer_cast<Punc>(bundle_ptr);
			/* neither bundle or punc, we don't know what to do */
			xzl_bug_on_msg(!punc, "bundle type mismatch -- are transforms compatible?");
			process_punc(trans, c, punc, out);
		} else { /* bundle path */
			process_bundle(trans, c, input_bundle, out);
		}
	}
};

#endif // SINGLE_INPUT_T_EVAL_H
