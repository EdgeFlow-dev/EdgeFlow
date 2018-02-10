//
// Created by xzl on 1/11/18.
//

#ifndef CREEK_BUNDLECONTAINER_H
#define CREEK_BUNDLECONTAINER_H

#include "Bundle.h"

/* container open */
#define PUNCREF_UNDECIDED 			(-1)
#define PUNCREF_CANCELED		 		(-2)  /* container sealed */
#define PUNCREF_MAX							3     /* no actual meaning */
#define PUNCREF_ASSIGNED 				2			/* container sealed */
#define PUNCREF_RETRIEVED 			1			/* container sealed */
#define PUNCREF_CONSUMED 				0			/* container sealed */

#define SIDE_INFO_NONE 		0  /* default. single input */
#define SIDE_INFO_L 			1
#define SIDE_INFO_R 			2
#define SIDE_INFO_J 			3  /* join */
#define SIDE_INFO_JD 			4  /* join's downstream */
#define SIDE_INFO_JDD 		5  /* join's downstream downstream */
#define SIDE_INFO_MAX 		6  /* join's downstream downstream */
#define CREEKERR_CONTAINER_MOVED 1

const int puncref_desc_offset = 2;
extern const char * puncref_key_[];

#define puncref_desc(x) (puncref_desc_[x + puncref_desc_offset])
#define puncref_key(x) (puncref_key_[x + puncref_desc_offset])

struct bundle_container;

/* future ideas:
 *  - @bundles can use some concurrent d/s where adding is lockfree
 *  - make @bundles lockfree and make @punc atomic (enforce a fence
 * around it)
 */
struct bundle_container {

public:

	/* which side the container is on?
	 * used to be side info. now separate as list info.
	 *
	 * this has to be sync with the actual topology of container lists.
	 * XXX: must be procted by container lock
	 */
	atomic<int> list_info;
	void set_list_info(LIST_INFO i){
		list_info = i;
	}

	LIST_INFO get_list_info(){
		return (LIST_INFO)list_info.load();
	}

	int has_punc = 0; //0: this container doesn't have pun cyet
			  //1: this container already has a punc

	shared_ptr<Punc> punc; /* unsafe to access this from outside w/o protection */

public:
	/* # of deposited, but not yet consumed, bundles.
	 * atomic because we allow bundle consumers to decrement this w/o locking
	 * containers. */
	atomic<long> refcnt;

	/* punc 	 punc_refcnt
	 * ---------------------
	 * nullptr 	  -2				punc invalidated by upstream (i.e. will never be assigned)
	 * nullptr		-1				punc never assigned
	 * valid			2					assigned, not emitted (== retrieved)
	 * valid			1			  	assigned, emitted, not consumed
	 * valid			0					consumed
	 */
	atomic<long> punc_refcnt;


	/* Can be a variety of d/s. Does not have to be ordered.
	 * the concurrent version still needs lock for erase -- unsafe */
#ifdef CONFIG_CONCURRENT_CONTAINER
	moodycamel::ConcurrentQueue<shared_ptr<BundleBase>> bundles;
	boost::shared_mutex punc_mtx_; /* container-wide lock. r: operate on the queue; w: punc */
	atomic<unsigned long> bundle_counter; /* for stat only */
#else
//  	unordered_set<shared_ptr<BundleBase>> bundles;
	vector<shared_ptr<BundleBase>> bundles;
//	list<shared_ptr<BundleBase>> bundles;
	std::mutex mtx_;  /* container-wide lock */
#endif


	/* OBSOLETED:
	 * Staged bundles are output produced by processing bundles in this container;
	 * however, if there is an earlier container which may assign a punc to
	 * downstream later, depositing these bundles to downstream at this time.will
	 * enable them overtake the punc, which will incur punc delay.
	 *
	 * Instead, we stage them here. Once the earlier container becomes dead and
	 * is cleaned, we flush the staged bundles to downstream.
	 */
	vector<shared_ptr<BundleBase>> staged_bundles;

public:
	/* the container for output bundles. Has to be atomic pointer since a thread
	 * may open the downstream pointer and link to it. */
	atomic<bundle_container*> downstream;

public:
//	bundle_container () : punc(nullptr), bundles(), punc_refcnt(-1), refcnt(0) { }
//		bundle_container () : punc_refcnt(-1), punc(nullptr), refcnt(0) { }
	/* xzl: the only workarond...why? */
	bundle_container (LIST_INFO info = LIST_INVALID)
		//: punc(nullptr), downstream(nullptr), side_info(side_info)
		: list_info(info), punc(nullptr), downstream(nullptr) //fix warning
	{
		//side_info = 0;
		punc_refcnt = PUNCREF_UNDECIDED; refcnt = 0;
#ifdef CONFIG_CONCURRENT_CONTAINER
		bundle_counter = 0;
#endif
	}

	/* for stat */
#ifdef CONFIG_CONCURRENT_CONTAINER
	unsigned long peekBundleCount(void) {
		return bundle_counter.load(memory_order::memory_order_relaxed);
	}
#else
	unsigned long peekBundleCount(void) {
		return bundles.size();  /* is this okay? */
	}
#endif

private:
	/* the following are unsafe: need lock, unless we already w locked all
	 * containers of a trans.
	 */

	/* get and erase one bundle. order does not matter.
	 * unsafe. need lock, unless we already locked all containers of a trans.
	 * return: nullptr if no bundle to return. */

#ifdef CONFIG_CONCURRENT_CONTAINER
	shared_ptr<BundleBase> getBundleUnsafe(void) {

		shared_ptr<BundleBase> ret = nullptr;

		if (!peekBundleCount())
			return ret;

		get_bundle_attempt_counter.fetch_add(1, std::memory_order_relaxed);
		bundles.try_dequeue(ret);

		if (ret) {
			xzl_assert(this->refcnt); /* > 0 since at least have one bundle */
			bundle_counter.fetch_sub(memory_order::memory_order_relaxed);
			get_bundle_okay_counter.fetch_add(1, std::memory_order_relaxed);
		}

		return ret;
	}
#else
	shared_ptr<BundleBase> getBundleUnsafe(void) {

		shared_ptr<BundleBase> ret;

		get_bundle_attempt_counter.fetch_add(1, std::memory_order_relaxed); /* XXX do this w/o lock */

		if (bundles.empty())
			return nullptr;

		assert(this->refcnt); /* > 0 since at least have one bundle */

		ret = bundles.back();
		bundles.pop_back();

		get_bundle_okay_counter.fetch_add(1, std::memory_order_relaxed);

		return ret;
	}
#endif

#ifdef CONFIG_CONCURRENT_CONTAINER
	void putBundleUnsafe(shared_ptr<BundleBase> const & bundle) {
		/* increment refcnt before depositing, so that when a consumer
  	 * consumes the bundle, it always
  	 * sees a legit refcnt in decrementing the refcnt. */
		auto oldref = this->refcnt.fetch_add(1);
		xzl_assert(oldref >= 0);
		bundle->refcnt = &(this->refcnt);
		/* atomic implies mem fence here */
		bundle->container = this;
		/* XXX make ->container atomic? */

		bundles.enqueue(bundle);

		bundle_counter.fetch_add(1, memory_order::memory_order_relaxed);
	}
#else
	void putBundleUnsafe(shared_ptr<BundleBase> const & bundle) {
  	/* increment refcnt before depositing, so that when a consumer
  	 * consumes the bundle, it always
  	 * sees a legit refcnt in decrementing the refcnt. */
  	auto oldref = this->refcnt.fetch_add(1);
  	xzl_assert(oldref >= 0);
//		if(oldref < 0){
//			EE("oldref is %ld < 0", oldref);
//			abort();
//		}

  	bundle->refcnt = &(this->refcnt);
  	/* atomic implies mem fence here */
  	bundle->container = this;
  	/* XXX make ->container atomic? */
		this->bundles.push_back(bundle);
	}
#endif

		/* assign a punc to this container.
		 * By pipeline design, there's no concurrent punc writer.
		 * However, there could be concurrent reader/writer of punc.
		 */
	void setPuncUnsafe(shared_ptr<Punc> const & punc) {
		xzl_assert(punc_refcnt != 0 && "can't do so on dead container");
		xzl_assert(punc_refcnt != 1 && "can't overwrite an emitted punc");
		xzl_assert(punc_refcnt <= 2);

		if (!this->punc)
			xzl_assert(punc_refcnt == -1);

		punc->refcnt = &(this->punc_refcnt);
		punc->container = this;
		punc->staged_bundles = &(this->staged_bundles);

		this->punc_refcnt = PUNCREF_ASSIGNED; /* see comment above */
		this->punc = punc;
	}

public:
	/*
	 * The MT Safe verion is necessary: some execution paths, e.g.
	 * depositBundlesToContainer(), since it already knows the source/dest
	 * containers, no longer locks all containers of a trans.
	 *
	 * Therefore, these paths must lock individual containers.
	 */

	shared_ptr<BundleBase> getBundleSafe(void) {
#ifdef CONFIG_CONCURRENT_CONTAINER
//		boost::shared_lock<boost::shared_mutex> lck(this->mtx_);
#else
		unique_lock<mutex> lck(this->mtx_);
#endif
		return getBundleUnsafe();
	}

	void putBundleSafe(shared_ptr<BundleBase> const & bundle) {
#ifdef CONFIG_CONCURRENT_CONTAINER
//		boost::shared_lock<boost::shared_mutex> lck(this->mtx_);
#else
		unique_lock<mutex> lck(this->mtx_);
#endif
		putBundleUnsafe(bundle);
	}

	/* get/set Punc should be locked since @punc is not atomic (does not
	 * imply a fence)
	 */
#ifdef CONFIG_CONCURRENT_CONTAINER
	void setPuncSafe(shared_ptr<Punc> const & punc) {
		boost::unique_lock<boost::shared_mutex> lck(this->punc_mtx_);
		setPuncUnsafe(punc);
		std::atomic_thread_fence(memory_order::memory_order_seq_cst);
	}
#else
	void setPuncSafe(shared_ptr<Punc> const & punc) {
		unique_lock<mutex> lck(this->mtx_);
		setPuncUnsafe(punc);
	}
#endif

#ifdef CONFIG_CONCURRENT_CONTAINER
	shared_ptr<Punc> const & getPuncSafe(void) {
		boost::unique_lock<boost::shared_mutex> lck(this->punc_mtx_);
		std::atomic_thread_fence(memory_order::memory_order_seq_cst);
		return punc;
	}
#else
	shared_ptr<Punc> const & getPuncSafe(void) {
		unique_lock<mutex> lck(this->mtx_);
		return punc;
	}
#endif

	/* check the invariant between punc_refcnt and punc.
	 * have to lock to enforce consistency for @punc (not aotmic) and
	 * @punc_refcnt */
	bool verifyPuncSafe() {
#ifdef CONFIG_CONCURRENT_CONTAINER
		boost::unique_lock<boost::shared_mutex> lck(this->punc_mtx_);
#else
		unique_lock<mutex> lck(this->mtx_);
#endif
		std::atomic_thread_fence(memory_order::memory_order_seq_cst);
		if (this->punc_refcnt == PUNCREF_UNDECIDED && this->punc) {
			return false;
		}
		return true;
	}


};

#include "Values.h"

#endif //CREEK_BUNDLECONTAINER_H
