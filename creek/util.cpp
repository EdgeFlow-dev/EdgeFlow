//
// Created by xzl on 12/11/17.
//

#include "util.h"

namespace creek {

/*
 * support rlock --> ulock and ulock --> wlock
 *
 * - if called with rlock held, release; acquire ulock. need restart.
 * - if called with ulock held, upgrade to wlock in place. no restart needed.
 *
 * return: true if wlock (exclusive) is acquired. can proceed to modify
 * data structures
 * false if rlock is released and ulock is acquired. need to restart.
 *
 * since boost::upgrade_to_unique_lock must be init'd once its declared,
 * we dynamically create it to defer its init. to avoid manual mgmt of the
 * dyn object, the caller can simply point a unique_ptr to it.
 *
 */
	bool upgrade_locks(boost::shared_lock<boost::shared_mutex> *prlock,
										 boost::upgrade_lock<boost::shared_mutex> *pulock,
										 std::unique_ptr<boost::upgrade_to_unique_lock<boost::shared_mutex>> *ppwlock) {
		/* at least one lock is owned */
		xzl_assert(prlock->owns_lock()
							 || pulock->owns_lock()
							 || (ppwlock && *ppwlock && (*ppwlock)->owns_lock()));

		auto &rlock = *prlock;
		auto &ulock = *pulock;
		auto &pwlock = *ppwlock;

		if (rlock.owns_lock()) {  /* upgrade rlock -> ulock */
			xzl_assert(!ulock.owns_lock());
			rlock.unlock();
			ulock.lock();
			return false;
		}

		if (ulock.owns_lock()) { /* upgrade ulock -> wlock in place. */
			xzl_assert(!rlock.owns_lock());
			xzl_assert(!pwlock); /* must points to nullptr */
// 			(*ppwlock) = std::move(
// 			pwlock = std::move(
//// 					make_unique<boost::upgrade_to_unique_lock<boost::shared_mutex>>(ulock)
// 					unique_ptr<boost::upgrade_to_unique_lock<boost::shared_mutex>>
// 							(new boost::upgrade_to_unique_lock<boost::shared_mutex>(ulock))
// 					);

			/* also okay, since assign temp object == move */
			pwlock =
					std::unique_ptr<boost::upgrade_to_unique_lock<boost::shared_mutex>>
							(new boost::upgrade_to_unique_lock<boost::shared_mutex>(ulock));
			xzl_assert(pwlock);
			/* no need start over since we atomically upgrade */
		}

		xzl_assert(!rlock.owns_lock() && !ulock.owns_lock()
							 && pwlock->owns_lock());

		return true;
	}

} /* creek */