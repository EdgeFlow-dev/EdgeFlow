#ifndef WINJOINEVAL_H
#define WINJOINEVAL_H

/*
 * xzl: join using sort. derived from JoinEvaluator1.h
 * 8/22/17
 *
 * Focus on supporting one type Record<kvpair>
 */

extern "C" {
#include "measure.h"
}

#include "core/EvaluationBundleContext.h"
#include "core/TransformEvaluator.h"
#include "JoinSeq.h"
#include "SeqBundle.h"
#include "BundleContainer.h"

template<class KVPair, int keypos, int vpos, int N>
class JoinSeqEval : public TransformEvaulator<JoinSeq<KVPair, keypos, vpos, N>> {
//	using K = decltype(KVPair::first);
//	using V = decltype(KVPair::second);
	using K = typename tuple_element<keypos, KVPair>::type;
	using TransformT = JoinSeq<KVPair, keypos, vpos, N>;
	using RecordKV = Record<KVPair>;
//	using RecordJoined = Record<pair<K, vector<V>>>;
	using InputBundleT = WinSeqBundle<KVPair, N>;
	using OutputBundleT = WinSeqBundle<KVPair, 1>;  /* make the type really simple */
public:
	JoinSeqEval(int node) {
		ASSERT_VALID_NUMA_NODE(node);
		this->_node = node;
	}

private:
	bool evaluateSortJoin(TransformT *trans,
												shared_ptr<InputBundleT> input_bundle,
												shared_ptr<OutputBundleT> output_bundle) {
		xzl_assert(trans);

		/* Sort the incoming bundle;
		 * Join with the other internal side (which is sorted); Emit;
		 * Merge with *this* side */

		bundle_container *upcon = trans->localBundleToContainer(input_bundle);

		//hym: left: list_info == 1
		//     right: list_info == 2
		//internal state left: 1 -1 = 0
		//internal state right: 2 - 1 =1
		int i = upcon->get_list_info() - 1;
		xzl_bug_on (!(i == SIDE_INFO_R - 1 || i == SIDE_INFO_L - 1));

		for (auto & x : input_bundle->vals) {

			Window const & w = x.first;
			auto & frag_in = x.second;

			/* Skip if this window has no record. otherwise JoinByFilter() will complain
			 *
			 * Empty window in bundle is possible. See StatefulTransform::RetrieveState()
			 * */
			 if (!frag_in.seqbufs[0].Size())
			 	continue;

			I("an incoming win: side %d"
			   "ts " ETIME_FMT " ts " ETIME_FMT, i, w.start, frag_in.w.start);

			/* combine to *this* side */
			trans->Combine(frag_in, i);

			/* join with *other* side */
			auto frag_out = trans->JoinEmit(frag_in, 1 - i);

			xzl_bug_on_msg(output_bundle->vals.count(w), "output already has duplicate window?");
//			EE("frag_out.seqbufs.size() = %lu", frag_out.seqbufs.size());
			xzl_bug_on(frag_out.seqbufs.size() != 1); /* should have 1 seqbuf */

			auto totalrec = frag_out.seqbufs[0].Size();

			if (frag_out.seqbufs.size() && totalrec) {
				output_bundle->vals[w] = frag_out;
				// update stat
				trans->record_counter_.fetch_add(totalrec, std::memory_order_relaxed);
			}
		}

		return true;
	}

	/* deposit bundle to Join's downstream directly */
	bool process_bundle(TransformT *trans, EvaluationBundleContext *c,
											shared_ptr<InputBundleT> input_bundle, PValue *out) {
		bool ret = false;

		vector<shared_ptr<OutputBundleT>> output_bundles;
		shared_ptr<OutputBundleT> output_bundle = nullptr;

		output_bundle = make_shared<OutputBundleT>();

		if (evaluateSortJoin(trans, input_bundle, output_bundle)) {
			output_bundles.push_back(output_bundle);
		}

		if (!output_bundles.empty()) {
			vector<shared_ptr<BundleBase>> output_bundles_(output_bundles.begin(), output_bundles.end());

			ret = true;
			trans->depositBundlesDownstream(out->consumer, input_bundle, output_bundles_);

			auto oldref = input_bundle->refcnt->fetch_sub(1);  /* input bundle consumed */
			if (oldref <= 0) {
				EE("bug: %s: oldref is %ld <= 0. container %lx",
					 trans->name.c_str(), oldref,
					 (unsigned long) (input_bundle->container.load(memory_order::memory_order_relaxed)));
				xzl_bug("bug");
				ret = false;
			}
		}
		return ret;
	}

	/*
	 *  When a punc arrives, drop internal state  (no actual output)
	 *
	 *     1. assing punc to a container
	 *     2. update left_wm and right_wm
	 *     3. call flushState() to flush internal state on one side
	 *     4. move some containers in Join's downstream from left_unordered_containers or 
	 *  	  right_unordered_containers to downstream ordered_containers..
	 */

	bool process_punc(TransformT *trans, EvaluationBundleContext *c,
										shared_ptr<Punc> punc, PValue *out) {
#ifdef MEASURE_LATENCY
		punc->mark("retrieved by: " + trans->name);
#endif

		bundle_container *upcon = trans->localBundleToContainer(punc);
		LIST_INFO list_info = upcon->get_list_info();

		etime nw_wm = punc->min_ts;
		shared_ptr<Punc> new_punc = make_shared<Punc>(nw_wm, punc->node);

		DD("list_info %d, ts " ETIME_FMT, list_info, nw_wm);

#ifdef MEASURE_LATENCY
		new_punc->inherit_markers(*punc);
#endif

		/* 1.update local internal wm
		 *
		 * (unconditionally) update local left/right wm.
		 * Note that Join does NOT have a local joint wm.
		 *
		 * Here, we assume upcon's list info is stable, i.e. won't be moved, so
		 * there's no race condition.
		 * hence, up trans can't be JD or JDD
		 */

		xzl_bug_on(list_info != LIST_L && list_info != LIST_R);
		trans->SetPartialWatermarkSafe(punc->min_ts, list_info);
		new_punc->set_list_info(list_info);

		//2. assign the punc to a container
		trans->depositOnePuncDownstream(out->consumer, punc, new_punc, new_punc->node);

#if 0 /* xzl: moved to the end of the func */
		// hym: don't set the punc-refcnt now, because we don't want to destroy this container now
		//      destroy this container until this container can be moved to 

		/* xzl: XXX problematic: consume wm before flushing state XXX */
		long expected = PUNCREF_RETRIEVED;
		if (!punc->refcnt->compare_exchange_strong(expected, PUNCREF_CONSUMED)) {
			bug("bad punc's refcnt?");
		}
#endif

/*
		//hym: purge is too expensive!!!
		//  if we purge first, then move contaienrs from l/r unordered to ordered in join's down(type4)
		//  the moving will be delayed, and the watermark will be delatyed(around 500ms)
		//  so the total latency is bad
		//  Solution: move containers before purge join's internal sate
	{
		boost::unique_lock<boost::shared_mutex> down_conlock(trans->mtx_container);
		k2_measure("before flush internal state");
		//3. flush internal state
		trans->watermark_update(punc->min_ts, side_info - 1);
		k2_measure("after flush internal state");
		k2_measure_flush();
	}
*/
		/* 4. move around downstream containers
		 * cf: JoinEvaluator1::process_punc()
		 *
		 * We lock container lists (mtx_container); however we cannot lock list_info of each lists.
		 * Thus, concurrent reader of list_info may have an inconsistent view
		 * */
		{
			boost::unique_lock<boost::shared_mutex> down_conlock(out->consumer->mtx_container);
			/* whether JD's left/right has sealed containers that should be moved */
			int left_sealed = 0;
			int right_sealed = 0;

			while (1) { /* iterate over all JD containers */

				left_sealed = 0;
				right_sealed = 0;

				auto & down_list_left = out->consumer->container_lists[LIST_L];
				auto & down_list_right = out->consumer->container_lists[LIST_R];
				auto & down_list_ordered = out->consumer->container_lists[LIST_ORDERED];
//				auto & down_list_unordered = out->consumer->container_lists[LIST_UNORDERED];

				// moved this out of the loop so we don't have to reacquire the lock from time to time.
				// writer lock for downstream trans (JD)
				//			boost::unique_lock<boost::shared_mutex> down_conlock(out->consumer->mtx_container);

				/* even if one side has no container, there's still a chance we can
				 * move containers.
				 * e.g. left_wm 12 (no containers)
				 * 		  right_wm 3. (a new container wm=5) --> can be moved  */
				if (down_list_left.empty() && down_list_right.empty())
					break;

				xzl_assert(out->consumer->get_side_info() == SIDE_INFO_JD);

				/* xzl: can't use reverse iterator due to splice() */
				auto it_l_oldest = down_list_left.end();
				if (it_l_oldest != down_list_left.begin()) {
					std::advance(it_l_oldest, -1);
					if (it_l_oldest->has_punc)
						left_sealed = 1;
				}

				auto it_r_oldest = down_list_right.end();
				if (it_r_oldest != down_list_right.begin()) {
					std::advance(it_r_oldest, -1);
					if (it_r_oldest->has_punc)
						right_sealed = 1;
				}

				/* which container to move to ordered part? */
				enum LIST_INFO to_move;
				char const * markmsg = nullptr;
				decltype(it_l_oldest) pv; /* the container at downstream to move */

				if (left_sealed == 1 && right_sealed == 1) {
					// Both sides have sealed containers
					// Compare the wm of the oldest containers, and move the smaller one to ordered

					if (it_l_oldest->punc->min_ts > it_r_oldest->punc->min_ts) {
						to_move = LIST_R;
						markmsg = "before splice: right";
						pv = it_r_oldest;
					} else {
						to_move = LIST_L;
						markmsg = "before splice: left";
						pv = it_l_oldest;
					}
				} else if (left_sealed == 1 && right_sealed == 0) {
					//hym: only left side has sealed container
					//     compare the container's punc with right_wm
					//     if punc <= right_wm, move the container to ordered
					if (it_l_oldest->punc->min_ts <= out->consumer->GetPartialWatermarkSafe(LIST_R)) {
						to_move = LIST_L;
						markmsg = "before splice: left";
						pv = it_l_oldest;
					} else {
						/* can't move the left sealed container. nothing much we can do */
						break;
					}
				} else if (left_sealed == 0 && right_sealed == 1) {
					//hym: only right side has sealed container
					//     compare container's wm with trans' left_wm
					//     if wm <= left_wm, move the container to ordered
					if (it_r_oldest->punc->min_ts <= out->consumer->GetPartialWatermarkSafe(LIST_L)) {
						to_move = LIST_R;
						markmsg = "before splice: right";
						pv = it_r_oldest;
					} else {
						/* can't move the sealed container. nothing much we can do */
						break;
					}
				} else {
					//both sides are not empty, but neither of them has sealed container
					// - nothing to move.
					break;
				}

				//					pv_r->punc->mark("before splice: right" + to_simple_string(boost::posix_time::microsec_clock::local_time()));
				pv->punc->mark(markmsg);
				pv->punc->dump_markers();
				down_list_ordered.splice(down_list_ordered.begin(),
																 out->consumer->container_lists[to_move], pv
				);

				/* xzl: we already hold the downt's container w lock */
				// rewrite the side_info as "ordered"
				// must be done after splice.
				pv->set_list_info(LIST_ORDERED);

				/* xzl: propagate changes to all downstreams
				 * (all JDD, all from unordered to ordered.)
				 */
				auto down_container = pv->downstream.load();
				auto down_trans = out->consumer->getFirstOutput()->consumer;
				while (down_trans && down_container) {
					xzl_assert(down_container->get_list_info() != LIST_ORDERED);
					/* XXX: xzl: perhaps need to lock all JDD at the same time (?) */
					EE("move JDD");
					boost::unique_lock<boost::shared_mutex> down_conlock1(down_trans->mtx_container);

					/* Find the iterator to downt container (from pointer)
					 * XXX unordered_containers can be made a set. no need to walk
					 * */
					auto it = down_trans->container_lists[LIST_UNORDERED].begin();
					while (it != down_trans->container_lists[LIST_UNORDERED].end()) {
						bundle_container *pt = &(*it);
						if (pt == down_container) {
							break;
						}
						it++;
					}

					down_trans->container_lists[LIST_ORDERED].splice(
							down_trans->container_lists[LIST_ORDERED].begin(),
							down_trans->container_lists[LIST_UNORDERED],
							it
					);

					/* rewrite side info --> moved to an ordered list */
					// must be done after splice.
					it->set_list_info(LIST_ORDERED);

					down_container = down_container->downstream.load();
					down_trans = down_trans->getFirstOutput()->consumer;
				}

				DD("done with all JDD");
			} //end while
		}

		DD("unlock JD");

		//hym: purge is very expensive, have to do purge after moving containers(after while loop)
		trans->flush_state(punc->min_ts, list_info - 1);

		long expected = PUNCREF_RETRIEVED;
		if (!punc->refcnt->compare_exchange_strong(expected, PUNCREF_CONSUMED)) {
			bug("bad punc's refcnt?");
		}

		return true;
	}

public:
	void evaluate(TransformT *trans, EvaluationBundleContext *c,
								shared_ptr<BundleBase> bundle_ptr = nullptr) override {
		xzl_bug_on_msg(!bundle_ptr, "must specify bundleptr. is old interface used?");
		xzl_assert(typeid(*trans) == typeid(TransformT));

		auto out = trans->getFirstOutput();
		xzl_assert(out);

		/* at this time, the numa node of this eval must be determined. */
		ASSERT_VALID_NUMA_NODE(this->_node);

		/* the bundle_ptr could be a data bundle or punc */
		auto input_bundle = dynamic_pointer_cast<InputBundleT>(bundle_ptr);

		if (!input_bundle) { /* punc path */
			auto punc = dynamic_pointer_cast<Punc>(bundle_ptr);
			xzl_bug_on(!punc); /* neither bundle or punc, we don't know what to do */
			process_punc(trans, c, punc, out);
		} else { //bundle path
			process_bundle(trans, c, input_bundle, out);
		}
	}
};

#endif /* WINJOINEVAL_H */
