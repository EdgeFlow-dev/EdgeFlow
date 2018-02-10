/*
 * transforms.cpp
 *
 *  Created on: Jun 18, 2016
 *      Author: xzl
 *
 * since all transforms will ultimately include this file, be cautious in
 * including any file here
 */

#ifndef TRANSFORMS_H
#define TRANSFORMS_H

#include <assert.h>
#include <typeinfo>
#include <string>
#include <set>
#include <map>
#include <list>
#include <tuple>
#include <unordered_set>
#include <unordered_map>
#include <utility>

#include "boost/thread/thread.hpp" // uses macro V()
#include "boost/date_time/posix_time/posix_time.hpp"

#include "config.h"
#include "Pipeline.h"
#include "PipelineVisitor.h"
#include "Values.h"
#include "PValues.h"
#include "util.h"

#include "log.h"

#include "tbb/concurrent_unordered_map.h"

using namespace std;
using namespace creek;

//class PValue;

class EvaluationBundleContext;

// xzl: avoid introducing template args here.
// in cpp, we can't bound the template args. and in Java, that seems to implement polymorphism?
//template <class DerivedTransform>
class PTransform {

#ifdef DEBUG
public:
	etime last_punc_ts = INT_MAX;
	bool punconce = true;
#endif

public:
	// hym: for dual input transforms, indicate which side the current transform is, L or R?
	// 0: downstream only has one input container stream, output bundles should be put into container_ directly
	// 1: downstream has 2 input container streams, 1 means output bundles should be put into left_containers_in
	// 2: downstream has 2 input container streams, 2 means output bundles should be put into right_containers_in
	// 3: current transform has 2 input container streams, and 2 output container streams
	//     			 SimpleMapper1: 1
	// UnbundedInMem: 0 ==>                   ==>  Join: 3 ==> Sink: 0
	//     			 SimpleMapper2: 2
	// XXX side_info should be assigned to containers
	const int side_info; // default: downstream is a single input transform

	int const & get_side_info() const {
		return side_info;
	}

protected:
	/* for join and JDD
	 * xzl: partial wms for left and right. must be accessed safely with lock
	 */
	etime left_wm = 0;
	etime right_wm = 0;
	// They are moved here from Join.h

public:
	/* upstream will directly deposit bundles to these container lists */

	/* oldest at back(). so push_front() appends a new container.
	 * a bundle may store a pointer to its enclosing container.
	 * so list helps here -- the element pointer should always be
	 * valid.
	 * */
	list<bundle_container> container_lists[LIST_MAX] ;

	const int cont_lists[SIDE_INFO_MAX][LIST_MAX] = {
			[SIDE_INFO_NONE] = {LIST_ORDERED, LIST_INVALID},
			[SIDE_INFO_L] = {LIST_ORDERED, LIST_INVALID}, /* side is L, but only has one list internally */
			[SIDE_INFO_R] = {LIST_ORDERED, LIST_INVALID},	/* ditto */
			[SIDE_INFO_J] = {LIST_L, LIST_R, LIST_INVALID},
			[SIDE_INFO_JD] = {LIST_L, LIST_R, LIST_ORDERED, LIST_INVALID},
			[SIDE_INFO_JDD] = {LIST_UNORDERED, LIST_ORDERED, LIST_INVALID},
	};

	//hym: apply containr to Join transform
	// Join has two input containers streams, and two ouput container streams
	// Upstream will deposit bundles to left_containers_in or right_containers_in
	// Join evaluator will get bundles from those input containers

	// type 4 trans
//	mutex left_mtx_unordered_containers;
//	mutex right_mtx_unordered_containers;
//	mutex mtx_ordered_containers;
//	mutex mtx_unordered_containers;

	// a unified implementation
	virtual PValue* apply(PValue* input) {
		return PCollection::createPrimitiveOutputInternal(input->getPipeline());
	}

	void validate(PValue* input) { }

	std::string getKindString() {
		return string(typeid(*this).name());
	}

	std::string getName() {
		return (!name.empty()) ? name : getKindString();
	}

	// we don't override anything (unlike Java)
	std::string toString() {
		if (name.empty()) {
			return getKindString();
		} else
			return getName() + " [" + getKindString() + "]";
	}

	virtual ~PTransform() { }

	// we want to hide the internal organization of inputs/outputs

	PValue* getFirstInput() {
		xzl_assert(!inputs.empty());
		return inputs[0];
	}

	PValue* getSecondInput() {
		xzl_assert(!inputs.empty());
		if (inputs.size() < 2)
			return nullptr;
		return inputs[1];
	}

	PValue* getFirstOutput() {
		xzl_assert(!outputs.empty());
		return outputs[0];
	}

	PValue* getSecondOutput() {
		xzl_assert(!outputs.empty());
		if (outputs.size() < 2)
			return nullptr;
		return outputs[1];
	}

	// visitor will directly push to it..
	vector<PValue *> inputs;
	vector<PValue *> outputs;

//  mutex	mtx_output;  /* keep order among output bundles/punc */
	mutex _mutex;  /* internal state */

	// upstream wm that has been observed lastly. need mtx_watermark
	etime upstream_wm;

//  /* tracking inflight bundles and received (but not yet emitted) punc */
//  /*  --- protected by mtx_container --- */
//  struct inflight_bundle_container {
//  	etime wm = etime_max; /* wm unassigned */
//  	unordered_set<shared_ptr<BundleBase>> bundles;
//  };
//  list<inflight_bundle_container> inflight_bundle_containers;

	// bundles being worked on by parallel evaluators
	// [ different from the bundles in the input PValue ]
	// XXX move this to SingleInputTransform
	unordered_set<shared_ptr<BundleBase>> inflight_bundles;
	unordered_set<shared_ptr<BundleBase>> dual_inflight_bundles[2];

	boost::shared_mutex mtx_container; /* protecting all containers */

	//hym: for Join
	// for left input and output
//  mutex left_mtx_container_in;
//  mutex left_mtx_container_out;
//  mutex right_mtx_container_in;
//  mutex right_mtx_container_out;

	/* a relaxed counter for bundles. for pressure feedback.
	 * NB: we count the bundles in container, i.e. not including inflight
	 * */
	atomic<unsigned long> bundle_counter_;

	/* helper */
	void dump_containers_legend() {
		cout << "legend for puncref:";
	}

	/* A container can be empty, e.g. after retrieving the last bundle */
	/* unsafe. need lock */
	void dump_containers(const char * msg = "") {
#if (CONFIG_KAGE_GLOBAL_DEBUG_LEVEL <= 50)  /* VV(...) */

		printf("dump_containers ('%s'): %s total %lu",
					 msg, this->name.c_str(),
					 this->container_lists[LIST_ORDERED].size());
		cout << endl;

		for (auto && cont : this->container_lists[LIST_ORDERED]) {
			/* snapshot */
			long refcnt = cont.refcnt.load();
			long punc_refcnt = cont.punc_refcnt.load();
//			auto s = cont.bundles.size();
			auto s = cont.peekBundleCount();

			printf("%lx: ", (unsigned long)(&cont));
			if (cont.downstream) {
				printf("down:%lx ", (unsigned long)(cont.downstream.load()));
			} else
				printf("down:NA ");

//			if (cont.punc)
//				cout << to_simplest_string1(cont.punc->min_ts).str() << "  ";
//			else
//				cout << "X";
//				cout << "(nullptr)";
//			cout << " puncrefcnt: " << punc_refcnt;
			cout << puncref_key(punc_refcnt) << " ";
//			cout << endl;

//			cout << "bundles: total " << refcnt;
//			cout << " unretrieved: " << s;
//			cout << " outstanding: " << (refcnt - s);
////			cout << endl;

			cout << "ref:" << refcnt;
			cout << "(in:" << s << ") ";
			cout << "|  ";
		}
		cout << endl;
#endif
	}

public:  /* xzl: XXX workaround, since these are needed by Join.h */


public:
	/* Maintain the (relaxed) bundle counter.
	 * return: the value before increment
	 * XXX combine the following two? */
	inline long IncBundleCounter(long delta = 1) {
		return bundle_counter_.fetch_add(delta, std::memory_order_relaxed);
	}

	inline long DecBundleCounter(long delta = 1) {
		return bundle_counter_.fetch_sub(delta, std::memory_order_relaxed);
	}

	/* return the # of *unretrieved* bundles (i.e. not including inflight ones)
	 * in this transform.
	 * Does not count punc.
	 * NB: this is frequently invoked by source to measure the pipeline pressure.
	 * So it has to be cheap.
	 * */
	inline long getNumBundles() {
		return bundle_counter_.load(std::memory_order_relaxed);
	}

#if 0 /* too expensive */
	long getNumBundles() {
		/* lock all containers in case some one suddenly go away */
  	unique_lock<mutex> conlock(mtx_container);
  	long ret = 0;
  	for (auto && cont : this->containers_) {
  		ret +=  cont.refcnt.load();
  	}
  	return ret;
	}
#endif

	/* For statistics: get the bundle count for *each* container and return as
	 * a vector. oldest at the back.
	 *
	 * Can be expensive */
	void getNumBundles(vector<long>* bundles) {
		/* lock all containers in case some one suddenly go away */
		boost::shared_lock<boost::shared_mutex> rlock(mtx_container);

		for (auto && cont : this->container_lists[LIST_ORDERED]) {
			/* w/o lock, the following two may be inconsistent */
			auto c = cont.peekBundleCount();
			if (c == 0) { /* encode perfcnt as neg */
				c = cont.punc_refcnt.load() - PUNCREF_MAX;
			}

			bundles->push_back(c);   /* in container only */
		}
	}

	struct cont_info {
		long bundle;
		etime punc;
		int side_info;
	};

	/* return # bundles and punc info for all containers */
	void getNumBundlesTs(vector<cont_info>* bundles) {
		/*hym:
		 *     Have already gotten lock before call this function
		 *     so should not get lock again here, otherwise deadlock will happen
		 */
		/* lock all containers in case some one suddenly go away */
		auto sideinfo = this->get_side_info();
		xzl_bug_on(sideinfo >= SIDE_INFO_MAX);
		auto & list_idx = cont_lists[sideinfo];
		int i;
		for (i = 0; i < LIST_MAX; i ++) {
			auto idx = list_idx[i];
			if (idx == LIST_INVALID)
				break;
			auto & conlist = this->container_lists[idx];
			for (auto && cont : conlist) {
				/* w/o lock, the following two may be inconsistent */
				cont_info info;

				info.bundle  = cont.peekBundleCount();
				if (info.bundle == 0) { /* encode perfcnt as neg */
					info.bundle = cont.punc_refcnt.load() - PUNCREF_MAX;
					xzl_assert(info.bundle < 0);
				}

				auto p = cont.getPuncSafe();
				if (p) {
					info.punc = p->min_ts;
					info.side_info = p->get_list_info();
				}

				bundles->push_back(info);   /* in container only */
			}
		}
		xzl_bug_on(i == LIST_MAX);
	}

	/* get text representation of container statistics */
	string getContainerInfo() {
		ostringstream oss;
		vector<cont_info> cnts;
		this->getNumBundlesTs(&cnts);
		//  	oss << "deposit to: " << this->name << "\n (bundles: ";
		oss << this->name << " (bundles: ";
		for (auto & cnt : cnts) {
			if (cnt.bundle >= 0)
				oss << cnt.bundle << " ";
			else {
				auto r = cnt.bundle + PUNCREF_MAX;
				oss << puncref_key(r) << " ";
				if (r >= 0) { /* punc ever assigned. show punc ts */
					oss << to_simple_string(cnt.punc) << " ";
				}
			}
		}
		oss << ")";
		return oss.str();
	}

	/* Used by source only. other transforms should use the container version.
	 * does not deal with cancelled punc.
	 *
	 * must be called after all prior bundles are deposited.
	 * (exception: flushed bundles can increase the refcnt first
	 * and emit the actual bundles later)
	 *
	 */
	void sourceDepositOnePuncToList(shared_ptr<Punc> punc, LIST_INFO info, int node = -1) {

#ifdef MEASURE_LATENCY
		ostringstream oss;
		vector<cont_info> cnts;
		this->getNumBundlesTs(&cnts);
		oss << "deposit to: " << this->name << "\n (bundles: ";
		for (auto & cnt : cnts) {
			if (cnt.bundle >= 0)
				oss << cnt.bundle << " ";
			else {
				auto r = cnt.bundle + PUNCREF_MAX;
				oss << puncref_key(r) << " ";
			}
			//  			if (r >= 0) { /* punc ever assigned. show punc ts */
			if (cnt.punc != etime_max) {
				oss << to_simple_string(cnt.punc) << "\t";
			}
		}
		oss << ")";
		punc->mark(oss.str());
//  	punc->mark("deposit to: " + this->name);
#endif
		/* fast path: rlock only, check whether containers need maintenance; if not,
									assign the punc.

			slow path: if maintenance needed, start over by grabbing upgrade_lock.
									pretend that we still can do fast path (we don't exclude
									other readers). if we really need do write, atomically
									upgrade the lock so other readers are excluded.
			come in with rlock. assuming fast path.

			see upgrade_locks()
		*/

		boost::shared_lock<boost::shared_mutex> rlock(mtx_container);
		boost::upgrade_lock<boost::shared_mutex> ulock(mtx_container, boost::defer_lock);
		unique_ptr<boost::upgrade_to_unique_lock<boost::shared_mutex>> pwlock
				= nullptr; /* will std::move later */

		auto &this_list = this->container_lists[info]; // won't change.
		punc->set_list_info(info);

		start:
		if (this_list.empty()) {
			/* Whole transform drained. we don't even have pending flushed
			 * bundles (otherwise there will be containers).
			 * Create a new container and seal it with @wm.
			 */

			if (!upgrade_locks(&rlock, &ulock, &pwlock))
				goto start;
			this_list.emplace_front(info);
			this_list.begin()->setPuncSafe(punc);
			return;
		}

		/* assign or update the wm of the most recent container */
		auto current = this_list.begin();
		auto listinfo = current->get_list_info();
		xzl_bug_on(listinfo != info);

		if (current->punc_refcnt == PUNCREF_RETRIEVED) {
			/* punc already emitted. start a new container. */

			if (!upgrade_locks(&rlock, &ulock, &pwlock))
				goto start;

			xzl_assert(current->getPuncSafe() && "must have a valid punc ptr");
			xzl_assert(!current->refcnt && "all bundles must be consumed");
			//   		containers_.push_front(bundle_container());
			this_list.emplace_front(info);
			this_list.begin()->setPuncSafe(punc);
			return;
		}

		/* clean up any dead container. */
		if (current->punc_refcnt == PUNCREF_CONSUMED) {

			if (!upgrade_locks(&rlock, &ulock, &pwlock))
				goto start;

			/* extensive sanity check... */
			xzl_assert(current->getPuncSafe() && "dead container but no valid punc?");
			xzl_assert(current->peekBundleCount() == 0);
			xzl_assert(current->refcnt == 0);

			current = this_list.erase(current);

			if (this_list.empty()) {
				/* after cleanup, no container left. start a new one. */
				this_list.emplace_front(info);
				this_list.begin()->setPuncSafe(punc);
				return;
			} else {
				/* current now points to the 2nd most recent container. */
				xzl_assert(current->punc_refcnt != 0
									 && "bug: can't have two dead containers");
			}
		}

		/* should have any type of lock */
		xzl_assert(rlock.owns_lock()
							 || ulock.owns_lock() || (pwlock && pwlock->owns_lock()));

		/* if we have wlock, we don't need to -Safe for individual container.
		 * but just be conservative...
		 */
		if (!current->getPuncSafe() || current->punc_refcnt == PUNCREF_ASSIGNED) {
			current->setPuncSafe(punc); /* assign or overwrite punc */
		} else {
			xzl_bug("bug?");
		}
	}

	/* deposit to *this* transform */
	void sourceDepositOnePunc(shared_ptr<Punc> punc, int node = -1) {
		auto const sideinfo = this->get_side_info();
		xzl_bug_on_msg(cont_lists[sideinfo][0] != LIST_ORDERED, "unsupported. source can only deposit to ordered list");
		sourceDepositOnePuncToList(punc, LIST_ORDERED, node);
	}

	/* Used by source only. other transforms should use the container version.
	 *
	 * deposit a data bundle to *this* transform. a later punc can only be deposited after this func returns.
	 *
	 * xzl: this is mostly called by source, which does not have container lists but directly manipulate downstream's
	 * container list.
	 *
	 * note this is different from depositBundleDownstream()
	 * */
public:
	void sourceDepositOneBundleToList(shared_ptr<BundleBase> bundle,
																		LIST_INFO info, int node = -1)
	{

		/* lock all containers until reliably getting the target container.
		 * (in case concurrent additions of containers) */

		boost::shared_lock<boost::shared_mutex> rlock(mtx_container);
		boost::upgrade_lock<boost::shared_mutex> ulock(mtx_container, boost::defer_lock);
		unique_ptr<boost::upgrade_to_unique_lock<boost::shared_mutex>> pwlock
				= nullptr; /* will std::move later */

		auto &this_list = this->container_lists[info]; // won't change.

		start:
		if (this_list.empty()) {
			if (!upgrade_locks(&rlock, &ulock, &pwlock))
				goto start;
			this_list.emplace_front(info); /* can't copy */
		}

		auto current_con = this_list.begin();

		auto transinfo = this->get_side_info();
		auto listinfo = current_con->get_list_info();

		xzl_bug_on(listinfo == LIST_INVALID);
		xzl_bug_on(listinfo != info);

		switch (transinfo) {
			/* cf: cont_info */
			case SIDE_INFO_L:
			case SIDE_INFO_R:
			case SIDE_INFO_NONE:
				xzl_bug_on(listinfo != LIST_ORDERED);
				break;
			default:
				xzl_bug("transinfo=??"); /* TBD, e.g. source directly deposits to J? */
				break;
		}

#if 0
		if (listinfo == LIST_INVALID) { /* unassigned? */
				/* ref: @cont_lists
				 * upgrade to wlock since we modify list info */
			if (!upgrade_locks(&rlock, &ulock, &pwlock))
				goto start;
			switch (transinfo) {
				case SIDE_INFO_L:
				case SIDE_INFO_R:
				case SIDE_INFO_NONE:
					listinfo = LIST_ORDERED;
					break;
				default:
					xzl_bug("transinfo=??"); /* TBD, e.g. source directly deposits to J */
					break;
			}
			current_con->set_list_info(listinfo);
		} else { /* list info assigned. check? */
			switch (transinfo) {
				case SIDE_INFO_L:
				case SIDE_INFO_R:
				case SIDE_INFO_NONE:
					xzl_bug_on(listinfo != LIST_ORDERED);
					break;
				default:
					xzl_bug("transinfo=??"); /* TBD, e.g. source directly deposits to J */
					break;
			}
		}
#endif

		if (current_con->getPuncSafe()) {

			/* the latest container already has a punc. open a new container */

			if (!upgrade_locks(&rlock, &ulock, &pwlock))
				goto start;

			xzl_assert(current_con->punc_refcnt != PUNCREF_UNDECIDED
								 && "invalid punc refcnt");

			/* The latest container is already dead: clean up.
			 * NB: even if we see punc_refcnt == 1 here, it may be dead
			 * (decremented to 0) beneath us any time. It will be cleaned by future
			 * calls.
			 */
			if (current_con->punc_refcnt == PUNCREF_CONSUMED) {
				/* extensive sanity check... */

				xzl_assert(current_con->getPuncSafe() && "dead container but no valid punc?");
#ifndef CONFIG_CONCURRENT_CONTAINER /* somehow failed this check -- because of relaxed counters? */
				xzl_assert(current_con->peekBundleCount() == 0);
#endif
				xzl_assert(current_con->refcnt == 0);

				current_con = this_list.erase(current_con);

				if (this_list.empty()) {
					/* after cleanup, no container left. start a new one. */
					this_list.emplace_front(listinfo);
					current_con = this_list.begin();
					goto open_container;
				} else {
					/* current_con now points to the 2nd most recent container.
					 * sanity check it */
					xzl_assert(current_con->punc_refcnt != PUNCREF_CONSUMED
										 && "bug: can't have two dead containers");
				}
			}

			/* careful when reading from an atomic/volatile value ... */
			auto prefcnt = current_con->punc_refcnt.load();
			xzl_assert(prefcnt == PUNCREF_CONSUMED   /* latest container dies just now */
								 || prefcnt == PUNCREF_RETRIEVED    /* outstanding */
								 || prefcnt == PUNCREF_ASSIGNED    /* unretrieved yet */
			);

			this_list.emplace_front(listinfo);
			current_con--; /* pointing to the last open container */
		}
#if 0
		/* now @current_con points to a valid, non-dead container */

	if (current_con->punc) {
		/* latest container already has a punc.
		 * It is sealed. create a new container. */
		xzl_assert(current_con->punc_refcnt != -1 && "must have a valid refcnt");

		/* current_con->punc_refcnt == 0 may happen: it was 1 last time we checked
		 * it; and it wad decremented underneath us.
		 * It seems okay that we leave it here and let future
		 */
		xzl_assert(current_con->punc_refcnt != 0 && "XXX clean up dead container?");

		containers_.emplace_front();
		current_con --;
	}
#endif

		/* Now @current_con points to an open container. Insert the bundle.
		 *
		 * @current_con won't go away since the punc won't be emitted
		 * concurrently.
		 */
		open_container:
		/* should have any type of lock */
		xzl_assert(rlock.owns_lock()
							 || ulock.owns_lock() || (pwlock && pwlock->owns_lock()));

		current_con->putBundleSafe(bundle);
		this->IncBundleCounter();

		//#ifdef MEASURE_LATENCY
		//  	bundle->mark("deposited: " + this->name);
		//#endif

	}

public:
	/* deposit to *this* transform */
	void sourceDepositOneBundle(shared_ptr<BundleBase> bundle, int node = -1) {
		auto const sideinfo = this->get_side_info();
		xzl_bug_on_msg(cont_lists[sideinfo][0] != LIST_ORDERED, "unsupported. source can only deposit to ordered list");
		sourceDepositOneBundleToList(bundle, LIST_ORDERED, node);
	}

//private:
public:  /* xzl: workaround XXX since it is needed by join */
	bundle_container* localBundleToContainer(
			shared_ptr<BundleBase> const & input_bundle)
	{
		bundle_container *upcon = input_bundle->container;
//XXX hym
#if 0 //not all contaier lists called_containers_, so here will be a bug if the container list's name is not containers_
		/* No need to lock. Just peek. */
#ifdef DEBUG
		bool flag = false;
		for (auto & c : containers_) {
			if (&c == upcon) {
				flag = true;
				break;
			}
		}
		xzl_assert(flag && "bug: enclosing container does not exist?");
#endif
#endif
		return upcon;
	}

private:
	/* return: # of downstream containers opened.
	 * -CREEKERR_CONTAINER_MOVED if container has been moved (up_info_expected does not hold). the caller may retry.
	 *
	 * NB: this is called with no lock at all
	 */
	int TryOpenDownstreamContainer(PTransform *downt, bundle_container *upcon)
	{
		int ret;
		auto sideinfo = this->get_side_info(); /* up trans's info */
		/* up container's info. since it might change under us, openDownstreamContainers() will validate this info
		 * again with lock */
		auto upconinfo = upcon->get_list_info();

		/* openDownStreamContainer() will update the list info, w/ locking */
		if (sideinfo == SIDE_INFO_NONE) {
//			if (upconinfo != LIST_ORDERED)
//				EE("upconinfo is %d. trans %s", upconinfo, this->name.c_str());
			xzl_bug_on(upconinfo != LIST_ORDERED);
			ret = openDownstreamContainers(upcon, downt);
			xzl_bug_on(ret < 0); /* should not fail, e.g. due to upcon move */
		} else if (sideinfo == SIDE_INFO_L) { // uptrans: Simplemapper 1, left
			// downt should be join, open a container in join's left_containers_in
			xzl_bug_on(upconinfo != LIST_ORDERED);
			ret = openDownstreamContainersOnList(upcon, downt, LIST_ORDERED, LIST_L);
//			if (ret < 0)  EE("now upconinfo %d", upcon->get_list_info());
			xzl_bug_on(ret < 0); /* should not fail, e.g. due to upcon move */
		} else if (sideinfo == SIDE_INFO_R) { // uptrans: Simplemapper 2, right
			//downt should be join, open a container in join's right_containers_in
			xzl_bug_on(upconinfo != LIST_ORDERED);
			ret = openDownstreamContainersOnList(upcon, downt, LIST_ORDERED, LIST_R);
//			if (ret < 0)
//				EE("now upconinfo %d", upcon->get_list_info());
			xzl_bug_on(ret < 0); /* should not fail, e.g. due to upcon move */
		} else if (sideinfo == SIDE_INFO_J) {
			xzl_bug_on(downt->get_side_info() != SIDE_INFO_JD);
			xzl_bug_on(upconinfo != LIST_L && upconinfo != LIST_R);
			ret = openDownstreamContainersOnList(upcon, downt, upconinfo, upconinfo); /* open a downt container on same side */
			xzl_bug_on(ret < 0); /* should not fail, e.g. due to upcon move */
		} else if (sideinfo == SIDE_INFO_JD) {
			xzl_bug_on(downt->get_side_info() != SIDE_INFO_JDD);
			switch (upconinfo) {
				case LIST_ORDERED:
					ret = openDownstreamContainersOnList(upcon, downt, LIST_ORDERED, LIST_ORDERED);
					xzl_bug_on(ret < 0); /* should not fail, e.g. due to upcon move */
					break;
				case LIST_L:
				case LIST_R:
					ret = openDownstreamContainersOnList(upcon, downt, upconinfo, LIST_UNORDERED);
					/* might fail due to upcon move */
					break;
				default:
					xzl_bug("JD list type = ?");
					break;
			}
		} else if (sideinfo == SIDE_INFO_JDD) {
			xzl_bug_on(downt->get_side_info() != SIDE_INFO_JDD);
			switch (upconinfo) {
				case LIST_UNORDERED:
					ret = openDownstreamContainersOnList(upcon, downt, LIST_UNORDERED, LIST_UNORDERED);
					/* might fail due to upcon move */
					break;
				case LIST_ORDERED:
					ret = openDownstreamContainersOnList(upcon, downt, LIST_ORDERED, LIST_ORDERED);
					xzl_bug_on(ret < 0); /* should not fail, e.g. due to upcon move */
					break;
				default:
					xzl_bug("JDD list type = ?");
					break;
			}
		} else {
			std::cout << "side info is " << this->get_side_info() << " " << this->getName() << " " << std::endl;
			xzl_bug(
					"unsupported side info. this side might be handled by other funcs in depositBundlesDownstreamXXX family");
		}

		return ret;
	}

public:
	/* deposit a bundle not to *this* transform, but to the given *downstream* transform @downt
	 *
	 * called without locking at all (for fast path: bundle directly goes to down container )
	 *
	 * XXX allow specifying @node for each bundle?
	 * */
	void depositBundlesDownstream(PTransform *downt,
																shared_ptr<BundleBase> input_bundle,
																vector<shared_ptr<BundleBase>> const &output_bundles, int node = -1)
	{
		bool try_open = false; /* have we tried open downstream containers? */
		int ret = 0;
		int failed = 0;
		xzl_bug_on(!downt);

		bundle_container *upcon = localBundleToContainer(input_bundle); /* enclosing container for @input_bundle */
		xzl_bug_on(!upcon);

		/* no lock if the downstream container is already linked */

		deposit:
		if (upcon->downstream) {
			/* fast path. NB @downstream is atomic w/ seq consistency so it's
			 * safe. */
			downt->depositBundlesToContainer(upcon->downstream, output_bundles, node);
			downt->IncBundleCounter(output_bundles.size());
			return;
		}

		/* slow path: try open container(s) downstream */
		xzl_bug_on_msg(try_open, "bug: already tried to open");

		ret = TryOpenDownstreamContainer(downt, upcon);

		if (ret == -CREEKERR_CONTAINER_MOVED) {
			if (failed < 3) {
				failed++;
				goto deposit;
			}
			xzl_bug("failed too many times due to CREEKERR_CONTAINER_MOVED");
		}

		if (!upcon->downstream) {
			EE("bug: failed to open downstream container. ret %d", ret);
			xzl_bug("!upcon->downstream");
		}
		xzl_bug_on(!upcon->downstream);
		try_open = true;
		goto deposit; /* go back to take the fast path */
	}

	/* see commment above */
	void depositOnePuncDownstream(PTransform *downt,
																shared_ptr<Punc> const &input_punc, shared_ptr<Punc> punc, int node = -1)
	{
		bool try_open = false;
		int ret = 0, failed = 0;

		xzl_assert(downt);
		bundle_container *upcon = localBundleToContainer(input_punc);
		xzl_assert(upcon); /* enclosing container */

		/* no lock if the downstream container is already linked */

		deposit:
		if (upcon->downstream) { /* fast path. NB @downstream is atomic, implying fence */
			downt->depositOnePuncToContainer(upcon->downstream, punc, node);
			upcon->downstream.load()->has_punc = 1;
			return;
		}

		xzl_assert(!try_open && "bug: already tried to open");

		ret = TryOpenDownstreamContainer(downt, upcon);

		if (ret == -CREEKERR_CONTAINER_MOVED) {
			if (failed < 3) {
				failed++;
				goto deposit;
			}
			xzl_bug("failed too many times due to CREEKERR_CONTAINER_MOVED");
		}

		xzl_assert(upcon->downstream);
		try_open = true;
		goto deposit;
	}

	/* *This* transform consumes a punc (and therefore closes a container) but
	 * does not emit flushed bundles or new punc.
	 *
	 * Since the up container will be gone soon, we need to mark the downstream
	 * container, indicating that i) it will never see a punc and ii) its (effective)
	 * punc should come from a later downstream container on the same line.
	 *
	 */
	void cancelPuncDownstream(PTransform* downt, shared_ptr<Punc> input_punc)
	{
		int ret = 0, failed = 0;
		bool try_open = false;

		xzl_assert(downt);
		bundle_container *upcon = localBundleToContainer(input_punc);
		xzl_assert(upcon); /* enclosing container */

		deposit:
		if (upcon->downstream) { /* fast path */
			long expected = PUNCREF_UNDECIDED;
			if (upcon->downstream.load()->punc_refcnt.compare_exchange_strong(
					expected, PUNCREF_CANCELED)) {
				return;
			} else {
				bug("invalid punc refcnt?");
			}
		}

		xzl_assert(!try_open && "bug: already tried to open");

		ret = TryOpenDownstreamContainer(downt, upcon);

		if (ret == -CREEKERR_CONTAINER_MOVED) {
			if (failed < 3) {
				failed++;
				goto deposit;
			}
			xzl_bug("failed too many times due to CREEKERR_CONTAINER_MOVED");
		}

		xzl_assert(upcon->downstream);
		try_open = true;
		goto deposit;
	}

protected:
	/* Core function for open downstream containers.
	 * will lock both transform's containers.
	 *
	 * @up_info_expected: the info of the upstream container list that has been observed by the caller.
	 * However, since the caller does not have the lock, the container may be moved to another list any time.
	 *
	 * return: # of downstream containers opened,
	 * -CREEKERR_CONTAINER_MOVED if container has been moved (up_info_expected does not hold)
	 */
	int openDownstreamContainersOnList(bundle_container *const upcon, PTransform *downt,
																		 LIST_INFO up_info_expected,
																		 LIST_INFO down_info)
	{
		xzl_assert(downt && upcon);

		/* slow path
		 *
		 * in *this* trans, walk from the oldest container until @upcon,
		 * open a downstream container if there isn't one.
		 *
		 * (X = no downstream container;  V = has downstream container)
		 * possible:
		 *
		 * X X V V (oldest)
		 *
		 * impossible:
		 * X V X V (oldest)
		 *
		 */

		/* reader lock for *this* trans */
		boost::shared_lock<boost::shared_mutex> conlock(mtx_container);
		/* writer lock for downstream trans */
		boost::unique_lock<boost::shared_mutex> down_conlock(downt->mtx_container);

		/* the up container may have been moved to another list before we grab the lock. so we need to get its info again */
		auto this_info = upcon->get_list_info();

		if (this_info != up_info_expected) /* up container has been moved. try again */
			return -CREEKERR_CONTAINER_MOVED;

		auto &this_container_list = this->container_lists[this_info];
		auto &down_container_list = downt->container_lists[down_info];

		if (upcon->downstream) /* check again */
			return 0;

		int cnt = 0;
		bool miss = false;    /* have we fixed a missing link? */

		for (auto it = this_container_list.rbegin();
				 it != this_container_list.rend(); /* by design we only have to walk to upcon. but let's check all */
				 it++) {

			/* also lock all downstream containers. good idea? */
//  		unique_lock<mutex> down_conlock(downt->mtx_container);

			if (!it->downstream) {
				miss = true;
				/* alloc & link downstream container. NB: it->downstream is an atomic
				 * pointer, so it implies a fence here. */
				down_container_list.emplace_front(down_info);
				it->downstream = &(down_container_list.front());

				//assign the side_info to this new container
				//it->downstream.load()->side_info = this->get_side_info(); //0->containrs_, 1->left_containers_in, 2->right_containers_in

				/* xzl: propagate the list info to downstream containers.
				 * Note that we can't copy over it->get_list_info(), e.g. if upcon is L, down con may be UNORDERED
				 * we took locks so there should not be race cond */
				it->downstream.load()->set_list_info(down_info);
				cnt++;
			} else { /* downstream link exists */
				xzl_bug_on_msg(miss,
											 "bug? an older container misses its downstream link while a new container has one.");
#ifdef DEBUG
				bool found = false;
				for (auto & con : down_container_list) {
					if (&con == it->downstream) {
						found = true;
						break;
					}
				}
				/* NB: once upstream assigns wm to the downstream (i.e. upstream
				 * is RETRIEVED or CONSUMED), the downstream may process the punc
				 * and be cleaned up prior to the upstream container.
				 * In this case, a dangling downstream pointer is possible.
				 *
				 * XXX other similar paths should do same thing. XXX
				 */
				auto puncref = it->punc_refcnt.load();
				if (!found
						&& puncref != PUNCREF_CONSUMED
						&& puncref != PUNCREF_RETRIEVED) {
					EE("bug: %s: downstream (%s) container does not exist",
						 this->name.c_str(), downt->name.c_str());
					cout << " -------------------------------\n";
					dump_containers("bug");
					downt->dump_containers("bug");
					cout << " -------------------------------\n\n";
					abort();
					xzl_bug("downstream container does not exist");
				}
#endif
			}
		}

#if 0 /* debug */
		cout << " -------------------------------\n";
		dump_containers("link containers");
		downt->dump_containers("link containers");
		cout << " -------------------------------\n\n";
#endif

		return cnt;
	}

private:
	virtual int openDownstreamContainers(bundle_container * const upcon, PTransform *downt) {
		auto ret = openDownstreamContainersOnList(upcon, downt, LIST_ORDERED, LIST_ORDERED);
		xzl_bug_on(ret < 0); /* should not fail, e.g. due to upcon move */
		return ret;
	}

//private:
public:
	/* Container set up and found.
	 * Deposit a data bundle to a @container of *this* transform.
	 * a later punc can only be deposited after this func returns.
	 *
	 * no need to lock containers_.
	 * IncBundleCounter() done by caller.
	 */
	void depositBundlesToContainer(bundle_container * const container,
																 vector<shared_ptr<BundleBase>> const & output_bundles, int node = -1)
	{
		/* sanity check */
		xzl_assert(container);

//XXX hym
#if 0 //not all container lists are named this->containers_, e.g. right_containers_in. so this check may fail
		#ifdef DEBUG
  	/* the given @container must exist in this trans */
  	{
//  		unique_lock<mutex> conlock(mtx_container);
			boost::shared_lock<boost::shared_mutex> rlock(mtx_container);

			for (auto & c : this->containers_) {
				if (&c == container)
					goto okay;
			}
  	}
  	xzl_assert(false && "bug? @container does not exist in this trans");
okay:
#endif
#endif

		xzl_assert(container->punc_refcnt == PUNCREF_UNDECIDED && "punc already assigned?");
		xzl_assert(container->verifyPuncSafe() && "punc already assigned?");

		for (auto & b : output_bundles) {
			xzl_assert(b != nullptr);
			container->putBundleSafe(b); /* see func comment above */
		}
		VV("%lu bundles to container %lx", output_bundles.size(),
			 (unsigned long)container);
	}

	/* See comment above.
	 * no need to lock containers_. */
	void depositOnePuncToContainer(bundle_container * const container,
																 shared_ptr<Punc> const & punc, int node = -1)
	{
		/* sanity check */
		xzl_assert(container);
		xzl_assert(punc);

#ifdef MEASURE_LATENCY
		ostringstream oss;
		vector<cont_info> cnts;
		this->getNumBundlesTs(&cnts);
		oss << "deposit to: " << this->name << "\n (bundles: ";
		for (auto &cnt : cnts) {
			if (cnt.bundle >= 0)
				oss << cnt.bundle << " ";
			else {
				auto r = cnt.bundle + PUNCREF_MAX;
				oss << puncref_key(r) << " ";
				if (r >= 0) { /* punc ever assigned. show punc ts */
					oss << to_simple_string(cnt.punc) << "\t";
				}
//  			oss << endl;
			}
		}
		oss << ")";
		punc->mark(oss.str());
#endif
//XXX hym
#if 0 // see comment above
		#ifdef DEBUG
  	{
  		//  		unique_lock<mutex> conlock(mtx_container);
			boost::shared_lock<boost::shared_mutex> rlock(mtx_container);

			/* make sure @container exists in this trans */
			for (auto & c : this->containers_) {
				if (&c == container)
					goto okay;
			}
  	}
  	xzl_assert(false && "bug? @container does not exist in this trans");
okay:
#endif
#endif

		xzl_assert(container->punc_refcnt == PUNCREF_UNDECIDED
							 && "punc already assigned?");
		xzl_assert(container->verifyPuncSafe() && "punc already assigned?");
		container->has_punc = 1;
		container->setPuncSafe(punc);
	}

public:

	/* Any container in this transform needs help?
	 *
	 * @has_early_punc: OUT. see comment below.
	 */
	bool hasWorkOlderThan(etime const & t, bool* has_early_punc) {
		/* 1. check tran wm, if more recent (>t), no work. (unemitted punc must>t)
		 *
		 * 2. if we've see an unemitted punc in upstream <t, we should do all
		 * containers in this trans regardless open/close. Since they'll
		 * block the punc from emitting from this trans.
		 *
		 * 3. if no punc <t observed in upstream, we check the oldest
		 * container in this transform. if open (punc unassigned), no work here.
		 * if its punc<=t assigned & unretrieved, has work & return punc & stop.
		 * [ what if punc outstanding? no work? look at next container?]
		 *
		 * report any discovered <t punc to caller.
		 */

		if (this->GetWatermarkSafe() > t)
			return false;
		return true;
	}

	/* Get a bundle/punc from a container whose punc is assigned and <t.
	 *
	 * Does not consider bundles from open container (should be done
	 * by getOneBundle())
	 *
	 * This also applies to new cascading container design, which may have
	 * multiple open containers towards the front of the list, e.g.
	 * O-open X-sealed C-(cancelled)
	 *
	 * possible:
	 * O O O X
	 * O O C X
	 * O X C X
	 *
	 * impossible:
	 * X O O X
	 * */

	virtual shared_ptr<BundleBase> getOneBundleOlderThanFromList(etime const & t,
																															 bool* has_pending_punc, LIST_INFO info, int node = -1) {

		*has_pending_punc = false;

		/* fastest path: if local wm more recent (>t), no work.
		 * (and any unemitted punc must >t) */

		if (this->GetWatermarkSafe() > t) {
			return nullptr;
		}

		auto & container_list = container_lists[info]; /* this won't go away. so we're safe */

		/* fast/slow path: there may be some work in this trans. */

		boost::shared_lock<boost::shared_mutex> rlock(mtx_container);
		boost::upgrade_lock<boost::shared_mutex> ulock(mtx_container, boost::defer_lock);
		unique_ptr<boost::upgrade_to_unique_lock<boost::shared_mutex>> pwlock
				= nullptr; /* will std::move later */

		shared_ptr<BundleBase> ret;

		/* if we upgrade rlock->ulock, we start over from here. this is
		 * because container_list may have been changed so much during lock upgrade
		 */
		start:

		if (container_list.empty()) {
//			dump_containers("no containers");
			return nullptr;
		}

		/* Start from oldest container (at back()) and look for one available
		 * bundle.
		 * For each container, we first check available data bundles and then
		 * any ready punc. If no luck, move to a newer container.
		 *
		 * When encountering an empty container, check its refcnt.
		 */

		auto it = container_list.rbegin();

		/* remember the 1st container that 1) has cancelled punc and 2) has
		 * some bundles */
		auto itt = container_list.rend();

		while (it != container_list.rend()) {

			if (it->punc_refcnt == PUNCREF_UNDECIDED) {
				/* we don't deal open container (and all later ones) */
				xzl_assert(it->verifyPuncSafe() && "bug?");
				return nullptr;
			}

			/* punc has been canceled, if we see a later container assigned with
			 * a punc, we may return a bundle from *this* container.
			 */
			if (it->punc_refcnt == PUNCREF_CANCELED) {
				if (it->refcnt == 0) { /* dead container, clean it up. */

					if (!upgrade_locks(&rlock, &ulock, &pwlock))
						goto start;

					/* because punc only canceled after bundles are deposited.*/
					xzl_assert(!it->peekBundleCount());
					it ++;
					it = list<bundle_container>::reverse_iterator(container_list.erase(it.base()));
				} else if (it->peekBundleCount() && itt == container_list.rend()) {
					/* bundles available + oldest container w/ punc canceled */
					itt = it;
					it ++;
				} else {
					/* else: could be no bundles in containers and some bundles are
						outstanding... */
					it ++;
				}
				continue;
			}

			/* punc has *ever* been assigned */

			if (it->getPuncSafe()->min_ts > t) {
				/* punc newer than wm. skip all remaining (later) containers */
				return nullptr;
			}

			/* now we see an assigned punc <=wm. */

			*has_pending_punc = true;

			/* Grab from the oldest container w/ canceled punc. However, when we
			 * go back, that container may or may not have bundles (other workers
			 * may have stolen them all). So we check all containers [itt, it) */
			if (itt != container_list.rend() && itt->peekBundleCount()) { /* try to avoid getBundle as much as possible */
				ret = itt->getBundleSafe();
				xzl_assert(ret);
				if (ret) {
					this->DecBundleCounter();
					return ret;
				}
			}

			/* non-empty container. grab & go */
			if (it->peekBundleCount() && (ret = it->getBundleSafe())) {  /* see above comment */
				this->DecBundleCounter();
				return ret;
			}

			/* an empty container... */

			/* some data bundles outstanding.
			 * don't wait. move to a newer container. */
			if (it->refcnt != 0) {
#if 0 /* can no longer verify the following due to concurrent readers. unless we lock the container */
				//				if (!(it->punc_refcnt != PUNCREF_RETRIEVED
//										&& it->punc_refcnt != PUNCREF_CONSUMED)) {
//					auto refcnt = it->punc_refcnt.load();
//					EE("bug: punc refcnt is %ld", refcnt);
//				}
				xzl_assert(it->punc_refcnt != PUNCREF_RETRIEVED
						&& it->punc_refcnt != PUNCREF_CONSUMED);
#endif
				it ++;
				continue;
			}

			/* an empty container, punc has been assigned (CANCELED case is handled
			 * above). all bundles already consumed.
			 *
			 * we take a peek of punc_refcnt and decide.
			 * */

			auto punc_refcnt = it->punc_refcnt.load();

			/* first, clean any dead container so that it won't block
			 * later punc.
			 */
			if (punc_refcnt == PUNCREF_CONSUMED) { /* bundles+punc consumed */
				/* has to be the oldest container since we can't have more
				 * than one outstanding punc (the second oldest container, whose
				 * punc is assigned, must still holds its punc).
				 */

				if (!upgrade_locks(&rlock, &ulock, &pwlock))
					goto start;  /* see comment at @start */

				xzl_assert(it == container_list.rbegin());
				/* erase w/ reverse iterator */
				it ++;
				it = list<bundle_container>::reverse_iterator(container_list.erase(it.base()));
				continue;
			} else if (punc_refcnt == PUNCREF_RETRIEVED) {
				/* punc outstanding. it may become 0 soon but we don't wait. */
				it ++;
				continue;
			} else if (punc_refcnt == PUNCREF_ASSIGNED) { /* punc not emitted */
				if (it == container_list.rbegin()) { /* oldest container. okay to emit */
					long expected = PUNCREF_ASSIGNED;
					if (!it->punc_refcnt.compare_exchange_strong(expected,
																											 PUNCREF_RETRIEVED)) {
						/* somebody just took & emitted it. move on to next container */
						it ++;
						continue;
					}

					/* XXX: opt: we may check the next newer container and
					 * coalesce punc as needed. benefit unclear though.
					 */
					return it->getPuncSafe();
				} else {
					/* not oldest container. can't emit punc before older ones. */
#if 0 /* not a good idea */
					/* opt: combine unemitted puncs.
					 *
					 * this is dangerous, since punc_refcnt may change under us.
					 * idea: temporarily decrements punc_refcnt to 1 so that
					 * other evaluators believe the punc is outstanding. after
					 * combing, increment to 2.  */
					auto & older = *(std::prev(it));
					if (older->punc_refcnt == 2) {
						older->wm = current->wm;
//						it = containers_.erase(it); /* erase & continue */
						it ++;
						it = list<bundle_container>::reverse_iterator(containers_.erase(it.base()));
					} else
#endif
					it ++;
					continue;
				}
			} else {
				EE("punc refcnt: %ld", punc_refcnt);
				xzl_assert(false && "illegal punc_perfcnt?");
			}

			xzl_assert(false && "bug?");
		} // while

//		dump_containers("no bundles");

		return nullptr;
	}

	virtual shared_ptr<BundleBase> getOneBundleOlderThan(etime const & t,
																											 bool* has_pending_punc, int node = -1)
	{

		auto sideinfo = this->get_side_info(); /* won't change. no lock needed */

		switch (sideinfo) {
			case SIDE_INFO_NONE:
			case SIDE_INFO_L:
			case SIDE_INFO_R:
				return getOneBundleOlderThanFromList(t, has_pending_punc,
																						 LIST_ORDERED, node);
				break;
			case SIDE_INFO_J:
				return getOneBundleOlderThan_J(t, has_pending_punc, node);
				break;
			case SIDE_INFO_JD:
				return getOneBundleOlderThan_JD(t, has_pending_punc, node);
				break;
			case SIDE_INFO_JDD:
				return getOneBundleOlderThan_JDD(t, has_pending_punc, node);
			default:
				xzl_bug("unknown type info?");
				break;
		}
		return nullptr;
	}

	shared_ptr<BundleBase> getOneBundleOlderThan_J(etime const & t,
																								 bool* has_pending_punc,
																								 int node = -1) {
		shared_ptr<BundleBase> ret;

		if(this->GetPartialWatermarkSafe(LIST_L) < this->GetPartialWatermarkSafe(LIST_R)){
			if ((ret = getOneBundleOlderThanFromList(t, has_pending_punc, LIST_L, node))) {
				return ret;
			} else {
				ret = getOneBundleOlderThanFromList(t, has_pending_punc, LIST_R, node);
				return ret;  /* return even if it is nullptr */
			}
		} else {
			if ((ret = getOneBundleOlderThanFromList(t, has_pending_punc, LIST_R, node))) {
				return ret;
			} else {
				ret = getOneBundleOlderThanFromList(t, has_pending_punc, LIST_L, node);
				return ret;
			}
		}
	}

	shared_ptr<BundleBase> getOneBundleOlderThan_JD(creek::etime const &t,
																									bool *has_pending_punc, int node = -1)
	{
		/* hym
		 * 	-- grab bundles/puncs from orderd_containers first
		 *	-- if not, than only get bundles from left/right_unordered_contaienrs
		 *	   according to left_wm and right_wm
	 */
		shared_ptr<BundleBase> ret;
		ret = getOneBundleOlderThanFromList(t, has_pending_punc, LIST_ORDERED, node);
		if (ret != nullptr) {
			return ret;
		} else if (this->GetPartialWatermarkSafe(LIST_L) < this->GetPartialWatermarkSafe(LIST_R)) { // consider left first
			ret = getOneBundleOlderThanFromList(t, has_pending_punc, LIST_L, node);
			if (ret != nullptr) {
				return ret;
			} else {
				ret = getOneBundleOlderThanFromList(t, has_pending_punc, LIST_R, node);
				return ret;
			}
		} else { // consider right first
			ret = getOneBundleOlderThanFromList(t, has_pending_punc, LIST_R, node);
			if (ret != nullptr) {
				return ret;
			} else {
				ret = getOneBundleOlderThanFromList(t, has_pending_punc, LIST_L, node);
				return ret;
			}
		}
	}

	shared_ptr<BundleBase> getOneBundleOlderThan_JDD(creek::etime const & t,
																									 bool* has_pending_punc, int node = -1){
		shared_ptr<BundleBase> ret;
		if ((ret = getOneBundleOlderThanFromList(t, has_pending_punc, LIST_ORDERED, node)))
			return ret;
		else {
			ret = getOneBundleOlderThanFromList(t, has_pending_punc, LIST_UNORDERED, node);
			return ret;
		}
	}

	/* Get a bundle/punc for a specific list.
	 * This should be the core getBundle func. */
protected:
	shared_ptr<BundleBase> getOneBundleFromList(LIST_INFO info, int node = -1)
	{
		shared_ptr<BundleBase> ret;
		get_one_bundle_from_list_counter.fetch_add(1, std::memory_order_relaxed);

		/* for locking rule, see comment in the above func */

		auto & container_list = container_lists[info]; /* this won't go away. so we're safe */

		boost::shared_lock<boost::shared_mutex> rlock(mtx_container);
		boost::upgrade_lock<boost::shared_mutex> ulock(mtx_container, boost::defer_lock);
		unique_ptr<boost::upgrade_to_unique_lock<boost::shared_mutex>> pwlock
				= nullptr; /* will std::move later */

		start:
		if (container_list.empty()) {
//  		dump_containers("no containers");
			return nullptr;
		}

		/* Start from oldest container (at back()) and look for one available
		 * bundle.
		 * For each container, we first check available data bundles and then
		 * any ready-to-retrieve punc. If no luck, move to a newer container.
		 *
		 * when encountering an empty container (no bundles), check its refcnt.
		 */

		auto it = container_list.rbegin();

		while (it != container_list.rend()) {

			/* non-empty container. grab & go */
			if (it->peekBundleCount()) { /* try to avoid getBundle as much as possible */
//			if ((ret = it->getBundleSafe())) {
//				get_one_bundle_from_list_counter.fetch_add(1, std::memory_order_relaxed); // dbg
				if ( (ret = it->getBundleSafe()) ) {
					VV("got one bundle from container %lx", (unsigned long) (&(*it)));
					this->DecBundleCounter();
					return ret;
				} else {
					/* failed attempt */
//					get_one_bundle_from_list_counter.fetch_add(1, std::memory_order_relaxed); // dbg
				}
			}

			/* Can't get any bundle: an empty container... */

			if (it->punc_refcnt == PUNCREF_UNDECIDED) {
				/* punc to be assigned. since we allow multi "open" containers, we
				 * should continue to the next container... */
				xzl_assert(it->verifyPuncSafe());
				it ++;
				continue;
			}

			/* some data bundles are outstanding.
			 * don't wait. move to a newer container. */
			if (it->refcnt != 0) {
#if 0 /* can no longer verify the following due to concurrent readers. unless we lock the container */
				xzl_assert(it->punc_refcnt != PUNCREF_CONSUMED && it->punc_refcnt != PUNCREF_RETRIEVED);
				it ++;
#endif
				continue;
			}

			/* an empty container, punc has been determined.
			 * all bundles already consumed (refcnt = 0).
			 *
			 * take a peek of punc_refcnt ...
			 * */

			auto punc_refcnt = it->punc_refcnt.load();

			/* first, clean any dead container so that it won't block the next punc.
			 */

			if (punc_refcnt == PUNCREF_CONSUMED) {
				/* has to be the oldest container since we can't have more
				 * than one outstanding punc (the second oldest container, whose
				 * punc is assigned, must still holds its punc).
				 */

				if (!upgrade_locks(&rlock, &ulock, &pwlock)){
					goto start;
				}

				xzl_assert(it == container_list.rbegin());
				/* erase w/ reverse iterator */
				it ++;
				it = list<bundle_container>::reverse_iterator(container_list.erase(it.base()));
				continue;
			} else if (punc_refcnt == PUNCREF_CANCELED) {
				/* erase */

				if (!upgrade_locks(&rlock, &ulock, &pwlock)){
					goto start;
				}

				it ++;
				it = list<bundle_container>::reverse_iterator(container_list.erase(it.base()));

				continue;
			} else if (punc_refcnt == PUNCREF_RETRIEVED) {
				/* punc outstanding. it may become CONSUMED soon but we don't wait. */
				it ++;
				continue;
			} else if (punc_refcnt == PUNCREF_ASSIGNED) { /* punc not emitted */
				if (it == container_list.rbegin()) {
					/* oldest container. (older dead containers should have been
					 * cleaned up now). okay to emit */
					long expected = PUNCREF_ASSIGNED;
					if (!it->punc_refcnt.compare_exchange_strong(expected,
																											 PUNCREF_RETRIEVED)) {
						//bug("bad punc refcnt");
						it ++;
						continue;
					}
					/* XXX: opt: we may check the next newer container and
					 * coalesce punc as needed. benefit unclear though.
					 */
					return it->getPuncSafe();
				} else {
					/* not oldest container. can't emit punc before older ones doing so. */
#if 0 /* not a good idea */
					/* opt: combine unemitted puncs.
					 *
					 * this is dangerous, since punc_refcnt may change under us.
					 * idea: temporarily decrements punc_refcnt to 1 so that
					 * other evaluators believe the punc is outstanding. after
					 * combing, increment to 2.  */
					auto & older = *(std::prev(it));
					if (older->punc_refcnt == 2) {
						older->wm = current->wm;
//						it = containers_.erase(it); /* erase & continue */
						it ++;
						it = list<bundle_container>::reverse_iterator(containers_.erase(it.base()));
					} else
#endif
					it ++;
					continue;
				}
			} else {
				EE("punc refcnt: %ld", punc_refcnt);
				xzl_assert(false && "illegal punc_perfcnt?");
			}

			xzl_bug("bug?");

		} // while

//  	dump_containers("no bundles");
		return nullptr;
	}

	/* Retrieve one bundle from @containers_ of this transform.
	 * We may get a data bundle or a punc.
	 */
public:
	virtual shared_ptr<BundleBase> getOneBundle(int node = -1) {
		auto sideinfo = this->get_side_info(); /* won't change. no lock needed */

		switch (sideinfo) {
			case SIDE_INFO_NONE:
			case SIDE_INFO_L:
			case SIDE_INFO_R:
				return getOneBundleFromList(LIST_ORDERED, node);
				break;
			case SIDE_INFO_J:
				return getOneBundle_J(node);
				break;
			case SIDE_INFO_JD:
				return getOneBundle_JD(node);
				break;
			case SIDE_INFO_JDD:
				return getOneBundle_JDD(node);
			default:
				xzl_bug("unknown type info?");
				break;
		}

		return nullptr;
		/* never reach here */
	}

	shared_ptr<BundleBase> getOneBundle_J(int node = -1) {
		shared_ptr<BundleBase> ret = nullptr;
		/* prefer the list that has a lagging wm.
		 * this is best effort: we don't lock both wm at the same time */
		if (this->GetPartialWatermarkSafe(LIST_L) < this->GetPartialWatermarkSafe(LIST_R)) {
			if ((ret = getOneBundleFromList(LIST_L, node))) {
				return ret;
			} else {
				ret = getOneBundleFromList(LIST_R, node);
				return ret;
			}
		} else {
			if ((ret = getOneBundleFromList(LIST_R, node))) {
				return ret;
			} else {
				ret = getOneBundleFromList(LIST_L, node);
				return ret;
			}
		}
	}

	shared_ptr<BundleBase> getOneBundle_JD(int node = -1) {
		/* hym
		 * 	-- grab bundles/puncs from orderd_containers first
		 *	-- if not, than only get bundles from left/right_unordered_contaienrs
		 *	   according to left_wm and right_wm
		 *
		 * xzl: this is best effort -- we don't lock all containers throughout the
		 * whole procedure.
	 */
		shared_ptr<BundleBase> ret;
		ret = getOneBundleFromList(LIST_ORDERED, node);
		if (ret != nullptr) {
			return ret;
		} else if (this->GetPartialWatermarkSafe(LIST_L) < this->GetPartialWatermarkSafe(LIST_R)) {
			ret = getOneBundleFromList(LIST_L, node);
			if (ret != nullptr) {
				return ret;
			} else {
				ret = getOneBundleFromList(LIST_R, node);
				return ret;
			}
		} else {
			ret = getOneBundleFromList(LIST_R, node);
			if (ret != nullptr) {
				return ret;
			} else {
				ret = getOneBundleFromList(LIST_L, node);
				return ret;
			}
		}
	}

	shared_ptr<BundleBase> getOneBundle_JDD(int node) {
		/*xzl: see above comments. we don't lock all containers */
		shared_ptr<BundleBase> ret;
		ret = getOneBundleFromList(LIST_ORDERED, node);
		if(ret != nullptr){
			return ret;
		} else {
			ret = getOneBundleFromList(LIST_UNORDERED, node);
			return ret;
		}
	}

	/* ----------------------------------------- */

	/*
		recalculate the watermark for a give transform.
		but does not propagate the watermark to downstream.

		default behavior for state-less transform: watermark
		is decided by the upstream transform and the oldest
		pending work (including those in input and being worked on)

		we don't expect this function is called often

		@upstream_wm: the snapshot of the upstream's watermark. If unspecified,
		actively fetch the current wm from the upstream.

		@return: the wm after recalculation.
	*/
	virtual etime RefreshWatermark(etime upstream_wm = etime_max) {
		PValue* v = getFirstInput(); // XXX deal with multiple ins
		xzl_assert(v);
		PTransform *upstream = v->producer;
		string upstream_name;
		xzl_assert(upstream);

		unique_lock<mutex> lock(mtx_watermark);

		// also examine all inflight bundles
		etime min_ts_flight = etime_max;
		for (auto && b : inflight_bundles) {
			if (b->min_ts < min_ts_flight)
				min_ts_flight = b->min_ts;
		}

		if (upstream_wm == etime_max) {
			upstream_wm = upstream->watermark;  // fetch the current wm
			upstream_name = upstream->getName() + string("(live)");
		} else {
			upstream_name = upstream->getName() + string("(snapshot)");
		}

		/* the upstream wm should be earlier than records that are yet to be
		 * processed. otherwise, we will observe the wm before processing records
		 * that are prior to the wm. */

		if (!((upstream_wm < v->min_ts) && (upstream_wm < min_ts_flight))) {
			EE("%s: (%lx): "
//				EE("%s: %s (%lx): new watermark: %s \n"
						 " \t\t upstream(%s) watermark: %s\n"
						 " \t\t input min_ts: %s\n"
						 " \t\t min_ts_flight: %s",
//						to_simple_string(boost::posix_time::microsec_clock::local_time()).c_str(), /* current ts */
				 name.c_str(), (unsigned long)this,
				 upstream_name.c_str(),
				 to_simplest_string(upstream_wm).c_str(),
				 to_simplest_string(v->min_ts).c_str(),
				 to_simplest_string(min_ts_flight).c_str()
			);
		}

		xzl_assert(upstream_wm < v->min_ts);
		xzl_assert(upstream_wm < min_ts_flight);

		etime pt = min(min_ts_flight, min(v->min_ts, upstream_wm));

		if (watermark > pt) {
			EE("bug: watermark regression. existing %s > new watermark",
				 to_simple_string(watermark).c_str());
			EE("%s: new watermark: %s"
						 " (input %s upstream %s",
				 name.c_str(),
				 to_simple_string(pt).c_str(),
				 to_simple_string(v->min_ts).c_str(),
				 to_simple_string(upstream_wm).c_str());
			xzl_assert(0);
		}

		xzl_assert(watermark <= pt);

		/* it's possible that watermark does not change (=pt) because it is held
		 * back by something other than the upstream wm (e.g. input).
		 */
		if (watermark < pt) {
//    if (watermark <= pt) {  /* debugging */
			watermark = pt;
#if 1
			/* debugging */
			if (name == string("[wc-mapper]"))
				EE("%s: (%lx): new watermark: %s \n"
//				EE("%s: %s (%lx): new watermark: %s \n"
							 " \t\t upstream(%s) watermark: %s\n"
							 " \t\t input min_ts: %s\n"
							 " \t\t min_ts_flight: %s",
//						to_simple_string(boost::posix_time::microsec_clock::local_time()).c_str(), /* current ts */
					 name.c_str(), (unsigned long)this,
					 to_simplest_string(watermark).c_str(),
					 upstream_name.c_str(),
					 to_simplest_string(upstream_wm).c_str(),
					 to_simplest_string(v->min_ts).c_str(),
					 to_simplest_string(min_ts_flight).c_str()
				);
#endif
		}
		return pt;
	}

	/* Actions to take when the transform sees a new snapshot of
	 * upstream's watermark. Often, the transform reacts by invoking the
	 * corresponding evaluator, e.g. for purging internal state.
	 *
	 * The action does not necessarily depends the local watermark; it
	 * should not depends on upstream's current wm, which may have advanced.
	 *
	 * default: do nothing
	 *
	 * @up_wm: the upstream watermark
	 XXX rename to OnUpstreamWatermarkChange()
	 */
//  virtual void OnNewUpstreamWatermark(int nodeid,
//  		EvaluationBundleContext *c, etime up_wm) { }

	virtual void ExecEvaluator(int nodeid, EvaluationBundleContext *c,
														 shared_ptr<BundleBase> bundle_ptr = nullptr) = 0;

	// xzl: protected ctor -- only subclasses can be instantiated.
	std::string name;

protected:

	PTransform(int sideinfo) : side_info(sideinfo) {
		bundle_counter_ = 0; /* can't be put in init list? */
		byte_counter_ = 0;
		record_counter_ = 0;
	}

	PTransform(std::string name, int sideinfo) : side_info(sideinfo), name(name) {
		bundle_counter_ = 0; /* can't be put in init list? */
		byte_counter_ = 0;
		record_counter_ = 0;
	}

//private:
public:
	// watermark. must be updated atomically. can't be protected by
	// container mutex
	etime watermark = etime_min;
	mutex mtx_watermark;

public:
	/* return a copy */
	etime GetWatermark() {
		unique_lock<mutex> lock(mtx_watermark);
		return watermark;
	}

	/* advance trans wm atomically.
	 * return: false if @new_wm == trans wm. true if update okay.
	 * panic if watermark regression.
	 */
	bool SetWatermark(etime const & new_wm) {
//  bool AdvanceWatermarkAtomic(etime const & new_wm) {
		unique_lock<mutex> lock(mtx_watermark);
		//assert(new_wm >= this->watermark);
		if (new_wm == this->watermark)
			return false;
		this->watermark = new_wm;
		return true;
	}

	/* return a copy */
	etime GetWatermarkSafe() {
		unique_lock<mutex> lock(mtx_watermark);
		return watermark;
	}

	/* advance trans wm atomically.
	 * return: false if @new_wm == trans wm. true if update okay.
	 * panic if watermark regression.
	 */
	bool SetWatermarkSafe(etime const & new_wm) {
//  bool AdvanceWatermarkAtomic(etime const & new_wm) {
		unique_lock<mutex> lock(mtx_watermark);

		if (new_wm < this->watermark) {
			cout << "wm regression: ";
			cout << "new:" << new_wm;
			cout << " old:" << this->watermark << endl;
		}

		xzl_assert(new_wm >= this->watermark);

		if (new_wm == this->watermark)
			return false;

		this->watermark = new_wm;
		return true;
	}

	etime GetPartialWatermarkSafe(enum LIST_INFO info) {
		unique_lock<mutex> lock(mtx_watermark);
		if (info == LIST_L)
			return left_wm;
		else if (info == LIST_R)
			return right_wm;

		xzl_bug("info=?");
		return 0;
	}

	/* Advance trans wm atomically. this DOES NOT change the joint wm
	 *
   * return: min(left_wm, right_wm).
   * panic if watermark regression.
   */
	etime SetPartialWatermarkSafe(etime const & new_wm, enum LIST_INFO info) {
		unique_lock<mutex> lock(mtx_watermark);

		if (info == LIST_L) {
			xzl_bug_on_msg(new_wm <= this->left_wm, "wm regression"); /* shouldn't see wms of same value*/
			this->left_wm = new_wm;
		} else if (info == LIST_R) {
			xzl_bug_on_msg(new_wm <= this->right_wm, "wm regression"); /* shouldn't see wms of same value*/
			this->right_wm = new_wm;
		} else {
			xzl_bug("info = ?");
			return 0;
		}

		auto new_joint_wm = min(this->left_wm, this->right_wm);
//		if (new_joint_wm > this->watermark) {
//			this->watermark = new_joint_wm;
//			return true;
//		}
		xzl_assert(new_joint_wm >= this->watermark); /* can't regress */
		return new_joint_wm;
	}

	/*
	 * statistics
	 */
	struct Statstics {
		const char * name;
		double lmbps, lmrps; /* in last interval */
		double mbps, mrps; 	/* total avg */
		double total_rec;
	};

	/* internal accounting  -- to be updated by the evaluator */
	atomic<unsigned long> byte_counter_, record_counter_;

	/* last time we report */
	unsigned long last_bytes = 0, last_records = 0;
	boost::posix_time::ptime last_check, start_time;
	int once = 1;

	/* @return: is stat filled */
	virtual bool ReportStatistics(Statstics* stat) {
		/* internal accounting */
		unsigned long total_records =
				record_counter_.load(std::memory_order_relaxed);
		unsigned long total_bytes =
				byte_counter_.load(std::memory_order_relaxed);

		auto now = boost::posix_time::microsec_clock::local_time();

		if (once) {
			once = 0;
			last_check = now;
			start_time = now;
			last_records = total_records;
			return false;
		}

		boost::posix_time::time_duration diff = now - last_check;

		{
			double interval_sec = (double) diff.total_milliseconds() / 1000;
			double total_sec = (double) (now - start_time).total_milliseconds() / 1000;

			stat->name = this->name.c_str();
			stat->mbps = (double) total_bytes / total_sec;
			stat->mrps = (double) total_records / total_sec;

			stat->lmbps = (double) (total_bytes - last_bytes) / interval_sec;
			stat->lmrps = (double) (total_records - last_records) / interval_sec;

			stat->total_rec = (double) total_records;

			last_check = now;
			last_bytes = total_bytes;
			last_records = total_records;
		}

		return true;
	}
};

////////////////////////////////////////////////////////////////////////

// xzl: do nothing but pass through.
// to construct the PCollection, we must use template. XXX remove template
template<class T>
//class PTransformNop : public PTransform <PTransformNop> {
class PTransformNop : public PTransform  {
public:
	PTransformNop(int sideinfo) : PTransform("nop", sideinfo) { }

	virtual ~PTransformNop() { }
};

/////////////////////////////////////////////////////////////////////

#include "DoFn.h"
#include "BundleContainer.h"

class EvaluationBundleContext;

#endif // TRANSFORMS_H

