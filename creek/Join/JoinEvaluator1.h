#ifndef JOINEVALUATOR1_H
#define JOINEVALUATOR1_H

/*
 * xzl: code cleanup
 */

extern "C" {
#include "measure.h"
}

#include "core/EvaluationBundleContext.h"
#include "core/TransformEvaluator.h"
#include "Join.h"
#include "Values.h"

using namespace creek;

template<class KVPair,
		template<class> class InputBundleT_,
		template<class> class OutputBundleT_
>
class JoinEvaluator1 : public TransformEvaulator<Join<KVPair, InputBundleT_, OutputBundleT_>> {
	//using KVPair = decltype(pair<long,long>);
	using K = decltype(KVPair::first);
	using V = decltype(KVPair::second);
	using TransformT = Join<KVPair, InputBundleT_, OutputBundleT_>;
	using RecordKV = Record<KVPair>;
	using RecordJoined = Record<pair<K, vector<V>>>;
	using InputBundleT = InputBundleT_<KVPair>;
	using OutputBundleT = OutputBundleT_<pair<K, vector<V>>>;

public:

	JoinEvaluator1(int node) {
		ASSERT_VALID_NUMA_NODE(node);
		this->_node = node;
	}

	//hym: flush internal state on one side
	etime flushState(TransformT *trans, const etime up_wm,
									 vector<shared_ptr<OutputBundleT>> *output_bundles,
									 EvaluationBundleContext *c, bool purge = true) {
		//TODO

		return up_wm;
	}

private:
	bool evaluateJoin(TransformT *trans,
										shared_ptr<InputBundleT> input_bundle,
										shared_ptr<OutputBundleT> output_bundle) {

		//TODO
		//here trans should be Join it self
		assert(trans);
		RecordKV kv;

		bundle_container *upcon = trans->localBundleToContainer(input_bundle);

		//hym: left: side_Info is 1
		//     right: side_info is 2
		//internal state left: 1 -1 = 0
		//internal state right: 2 - 1 =1
		int i = upcon->get_side_info() - 1;
		xzl_bug_on (!(i == SIDE_INFO_R - 1 || i == SIDE_INFO_L - 1));

		//boost::shared_lock<boost::shared_mutex> reader_lock0(trans->_mutex[0]);	
		//boost::shared_lock<boost::shared_mutex> reader_lock1(trans->_mutex[1]);	
		/* the following two are hot */
		boost::shared_lock<boost::shared_mutex> reader_lock0(trans->win_containers[0].mtx_ctn);
		boost::shared_lock<boost::shared_mutex> reader_lock1(trans->win_containers[1].mtx_ctn);
		for (auto &&it = input_bundle->begin(); it != input_bundle->end(); ++it) {
			// search the other side. if match, emit the joined pair
			if (trans->search(*it, &kv, 1 - i)) {
				output_bundle->add_record(RecordJoined(
						make_pair(kv.data.first,
											vector<V>({(*it).data.second, kv.data.second})), kv.ts));
			}
			trans->add_record(*it, i);
		}
		return true;
	}

	//hym: new design, deposit bundle to Join's downstream directly, instead of Join's left/right_containers_out
	bool process_bundle(TransformT *trans, EvaluationBundleContext *c,
											shared_ptr<InputBundleT> input_bundle, PValue *out) {
		bool ret = false;
		ret = ret;

		vector<shared_ptr<OutputBundleT>> output_bundles;
		/* contains the output bundle */
		shared_ptr<OutputBundleT> output_bundle = nullptr;

		int output_size = (input_bundle->size() >= 0 ? input_bundle->size() : 128);
		output_bundle = make_shared<OutputBundleT>(output_size,
																							 input_bundle->node); /* same node as the input bundle */

		if (evaluateJoin(trans, input_bundle, output_bundle)) {
			output_bundles.push_back(output_bundle);
		}

		if (!output_bundles.empty()) {
			vector<shared_ptr<BundleBase>> output_bundles_(
					output_bundles.begin(),
					output_bundles.end());

			ret = true;
			//TODO depositBundleLR(): deposit to left_containers_out or
			//     right_containers_out
			trans->depositBundleDownstreamLR(out->consumer, input_bundle, output_bundles_);

			auto oldref = input_bundle->refcnt->fetch_sub(1);  /* input bundle consumed */
			if (oldref <= 0) {
				EE("bug: %s: oldref is %ld <= 0. container %lx",
					 trans->name.c_str(), oldref,
					 (unsigned long) (input_bundle->container));
				abort();
			}
			assert(oldref > 0);
		}
		return true;
	}//end process_bundle

	//hym: 1. assing the punc to a container
	//     2. update left_wm or right_wm
	//     3. call flushState() to flush internal state on one side
	//     4. deposit some containers from left_containers_out or
	//        right_containers_out to downstream's containers_
	/*
	 *  hym: new design
	 *     1. assing punc to a container
	 *     2. update left_wm and right_wm
	 *     3. call flushState() to flush internal state on one side
	 *     4. move some containers in Join's downstream from left_unordered_containers or 
	 *  	  right_unordered_containers to downstream ordered_containers..
	 * 	  TODO: move all containers(inherent from this container) to downstreams....(follow downstream pointer) 
	 */

	bool process_punc(TransformT *trans, EvaluationBundleContext *c,
										shared_ptr<Punc> punc, PValue *out) {
#ifdef MEASURE_LATENCY
		punc->mark("retrieved by: " + trans->name);
#endif

		bundle_container *upcon = trans->localBundleToContainer(punc);
		int side_info = upcon->get_side_info();

		etime nw_wm = punc->min_ts;
		shared_ptr<Punc> new_punc = make_shared<Punc>(nw_wm, punc->node);

#ifdef MEASURE_LATENCY
		new_punc->inherit_markers(*punc);
#endif

		//1.update local internal wm
		//  set new_wm's set_info, which will be used by type4 to update its left/right_wm
		if (side_info == 1) {//left
			trans->left_wm = punc->min_ts;
			new_punc->set_side_info(1); //left
		} else if (side_info == 2) { //right
			trans->right_wm = punc->min_ts;
			new_punc->set_side_info(2); //right
		}

		//2. assign the punc to a container
		trans->depositPuncDownstreamLR(out->consumer, punc, new_punc, new_punc->node);

		// hym: don't set the punc-refcnt now, because we don't want to destroy this container now
		//      destroy this container until this container can be moved to 

		/* xzl: XXX problematic: consume wm before flushing state XXX */
		long expected = PUNCREF_RETRIEVED;
		if (!punc->refcnt->compare_exchange_strong(expected, PUNCREF_CONSUMED)) {
			bug("bad punc's refcnt?");
		}
/*	
		//2. update local internal wm
		if(side_info == 1){//left
			trans->left_wm = punc->min_ts;
			new_punc->set_side_info(1); //left
		}else if(side_info == 2){ //right
			trans->right_wm = punc->min_ts;
			new_punc->set_side_info(2); //right
		}
*/
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
		/* 4. move around downstream containers  */
		while (1) {
			//int flag = 0;
			int left_flag = 0;
			int right_flag = 0;
			// writer lock for downstream trans (type 4)
			boost::unique_lock<boost::shared_mutex> down_conlock(out->consumer->mtx_container);
			/* even if one side has no container, there's still a chance we can
			 * move containers.
			 * e.g. left_wm 12 (no containers)
			 * 		  right_wm 3. (a new container wm=5) --> can be moved  */
			if (out->consumer->left_unordered_containers.empty()
					&& out->consumer->right_unordered_containers.empty()) {
				//std::cout << "all empty!!!!!!!!!!!!!!!!" << std::endl;
				//down_conlock.unlock();
				break;
			}

			xzl_assert(out->consumer->get_side_info() == SIDE_INFO_JD);

			//new design
			if (!(out->consumer->left_unordered_containers.empty())) {
				auto it_l = out->consumer->left_unordered_containers.end();
				auto pv_l = std::prev(it_l, 1);  /* left oldest */
				if (pv_l->has_punc) {
					left_flag = 1;
				}
			}

			if (!(out->consumer->right_unordered_containers.empty())) {
				auto it_r = out->consumer->right_unordered_containers.end();
				auto pv_r = std::prev(it_r, 1);  /* right oldest */
				if (pv_r->has_punc) {
					right_flag = 1;
				}
			}

			if (left_flag == 1 && right_flag == 1) {
				//TODO: Both sides have sealed containers
				//      Compare their wm, and move the smaller one to ordered 
				auto it_l = out->consumer->left_unordered_containers.end();
				auto pv_l = std::prev(it_l, 1);  /* left oldest */

				auto it_r = out->consumer->right_unordered_containers.end();
				auto pv_r = std::prev(it_r, 1);  /* right oldest */

				if (pv_l->punc->min_ts > pv_r->punc->min_ts) { /* right container moved to ordered */
					pv_r->punc->mark("before splice: right" + to_simple_string(boost::posix_time::microsec_clock::local_time()));
					pv_r->punc->dump_markers();
					/* todo: add "move" marker XXX */
					out->consumer->ordered_containers.splice(
							out->consumer->ordered_containers.begin(),
							out->consumer->right_unordered_containers,
							pv_r
					);

					// rewrite the side_info: ordered_containers in type 5 should be 0
					pv_r->set_side_info(0);
					//						c->SpawnConsumer(); /* xzl: needed? */

					/* xzl: propagate changes to all downstreams (all type 5)
					 */
					auto down_container = pv_r->downstream.load(); //in type 5
					auto down_trans = out->consumer->getFirstOutput()->consumer; // type5
					while (down_trans && down_container) {
						assert(down_container->get_side_info() != 0);
						/* XXX: xzl: perhaps need to lock all of them (?) */
						boost::unique_lock<boost::shared_mutex> down_conlock1(down_trans->mtx_container);

						/* xzl: XXX unordered_containers can be made a set. no need to walk
						 * has to walk here to find the iterator (from pointer)  */
						auto it = down_trans->unordered_containers.begin();
						while (it != down_trans->unordered_containers.end()) {
							bundle_container *pt = &(*it);
							if (pt == down_container) {
								break;
							}
							it++;
						}

						down_trans->ordered_containers.splice(
								down_trans->ordered_containers.begin(),
								down_trans->unordered_containers,
								it
						);

						down_container = down_container->downstream.load();
						down_trans = down_trans->getFirstOutput()->consumer;
					}
				} else { /* left container moved to ordered */
					// pv_l->punc->min_ts <= pv_r->punc->min_ts
					pv_l->punc->mark("before splice: left" + to_simple_string(boost::posix_time::microsec_clock::local_time()));
					pv_l->punc->dump_markers();
					out->consumer->ordered_containers.splice(
							out->consumer->ordered_containers.begin(),
							out->consumer->left_unordered_containers,
							pv_l
					);
					pv_l->set_side_info(0); // side_info of ordered_containers in type 5 should be 0
					// side_info of unordered_containers in type 5 should be 1 or 2

					//TODO: move punc->downstream->downstream...->downstream to ordered_containers
					auto down_container = pv_l->downstream.load(); //in type 5
					auto down_trans = out->consumer->getFirstOutput()->consumer; // type5
					while (down_trans && down_container) {
						assert(down_container->get_side_info() != 0);
						boost::unique_lock<boost::shared_mutex> down_conlock2(down_trans->mtx_container);

						auto it = down_trans->unordered_containers.begin();
						while (it != down_trans->unordered_containers.end()) {
							bundle_container *pt = &(*it);
							if (pt == down_container) {
								break;
							}
							it++;
						}

						down_trans->ordered_containers.splice(
								down_trans->ordered_containers.begin(),
								down_trans->unordered_containers,
								it
						);
						down_container = down_container->downstream.load();
						down_trans = down_trans->getFirstOutput()->consumer;
					}
				}

			} else if (left_flag == 1 && right_flag == 0) {
				//hym: only left side has sealed container
				//     compare the container's punc with right_wm
				//     if punc <= right_wm, move the container to ordered 
				auto it_l = out->consumer->left_unordered_containers.end();
				auto pv_l = std::prev(it_l, 1);  /* left oldest */
				if (pv_l->punc->min_ts <= out->consumer->right_wm) {
					//hym:  move left container to ordered
					pv_l->punc->mark("before splice: left" + to_simple_string(boost::posix_time::microsec_clock::local_time()));
					pv_l->punc->dump_markers();
					out->consumer->ordered_containers.splice(
							out->consumer->ordered_containers.begin(),
							out->consumer->left_unordered_containers,
							pv_l
					);
					pv_l->set_side_info(0); // side_info of ordered_containers in type 5 should be 0
					// side_info of unordered_containers in type 5 should be 1 or 2
					//TODO: move punc->downstream->downstream...->downstream to ordered_containers
					//					c->SpawnConsumer();
					auto down_container = pv_l->downstream.load(); //in type 5
					auto down_trans = out->consumer->getFirstOutput()->consumer; // type5
					while (down_trans && down_container) {
						assert(down_container->get_side_info() != 0);
						boost::unique_lock<boost::shared_mutex> down_conlock2(down_trans->mtx_container);

						auto it = down_trans->unordered_containers.begin();
						while (it != down_trans->unordered_containers.end()) {
							bundle_container *pt = &(*it);
							if (pt == down_container) {
								break;
							}
							it++;
						}

						down_trans->ordered_containers.splice(
								down_trans->ordered_containers.begin(),
								down_trans->unordered_containers,
								it
						);

						down_container = down_container->downstream.load();
						down_trans = down_trans->getFirstOutput()->consumer;
					}

				}

			} else if (left_flag == 0 && right_flag == 1) {
				//TODO: compaer punc on right with left_wm. move if smaller
				//hym: only right side has sealed container
				//     compare container's wm with trans' left_wm
				//     if wm <= left_wm, move the container to ordered 
				auto it_r = out->consumer->right_unordered_containers.end();
				auto pv_r = std::prev(it_r, 1);  /* right oldest */
				if (pv_r->punc->min_ts <= out->consumer->left_wm) {
					//hym: move right container to ordered
					/* todo: add "move" marker XXX */
					pv_r->punc->mark("before splice: right" + to_simple_string(boost::posix_time::microsec_clock::local_time()));
					pv_r->punc->dump_markers();
					out->consumer->ordered_containers.splice(
							out->consumer->ordered_containers.begin(),
							out->consumer->right_unordered_containers,
							pv_r
					);

					// rewrite the side_info: ordered_containers in type 5 should be 0
					pv_r->set_side_info(0);

					/* xzl: propagate changes to all downstreams (all type 5)
					 */
					auto down_container = pv_r->downstream.load(); //in type 5
					auto down_trans = out->consumer->getFirstOutput()->consumer; // type5
					while (down_trans && down_container) {
						assert(down_container->get_side_info() != 0);
						/* XXX: xzl: perhaps need to lock all of them (?) */
						boost::unique_lock<boost::shared_mutex> down_conlock1(down_trans->mtx_container);

						/* xzl: XXX unordered_containers can be made a set. no need to walk
						 * has to walk here to find the iterator (from pointer)  */
						auto it = down_trans->unordered_containers.begin();
						while (it != down_trans->unordered_containers.end()) {
							bundle_container *pt = &(*it);
							if (pt == down_container) {
								break;
							}
							it++;
						}

						down_trans->ordered_containers.splice(
								down_trans->ordered_containers.begin(),
								down_trans->unordered_containers,
								it
						);

						down_container = down_container->downstream.load();
						down_trans = down_trans->getFirstOutput()->consumer;
					}
				}
			} else {
				//both sides are not empty, but neither of them has sealed container
				break;
				//assert(false && "both of left_flag and right_flag are 0??");
			}

		} //end while

		//hym: purge is very expensive, have to do purge after moving containers(after while loop)
		//trans->watermark_update(punc->min_ts, side_info - 1);
		//hym: split watermark_update() to flush_state() and watermark_update()
		trans->watermark_update(punc->min_ts, side_info - 1);
		trans->flush_state(punc->min_ts, side_info - 1);
		return true;
	}

public:
	// hym: process bundle_ptr 
	void evaluate(TransformT *trans, EvaluationBundleContext *c,
								shared_ptr<BundleBase> bundle_ptr = nullptr) override {
		assert(bundle_ptr && "must specify bundleptr. is old interface used?");
		assert(typeid(*trans) == typeid(TransformT));

		auto out = trans->getFirstOutput();
		assert(out);

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

#endif /* JOINEVALUATOR1_H */
