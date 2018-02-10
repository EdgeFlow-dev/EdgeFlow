//
// Created by xzl on 9/1/17.
//
// factored out from Values.cpp, so that the latter does not depend on TBB

#include "Values.h"

#ifdef USE_FOLLY_STRING
/* cf: folly/Hash.h */
namespace tbb {
	size_t tbb_hasher(const folly::fbstring & key) {
		return static_cast<size_t>(
						folly::hash::SpookyHashV2::Hash64(key.data(), key.size(), 0));
	};
}
#endif

#ifdef USE_TBB_DS
namespace creek_set_array {

	void insert(SetArrayPtr ar, creek::string const & in) {
		int index = tbb::tbb_hasher(in) % NUM_SETS;
		//		(ar + index)->insert(in);
		(*ar)[index].insert(in);
	}

	void merge(SetArrayPtr mine, SetArrayPtr const & others) {
		for (int i = 0; i < NUM_SETS; i++) {
			//			(mine + i)->insert((others + i)->begin(), (others + i)->end());
			(*mine)[i].insert((*others)[i].begin(), (*others)[i].end());
		}
	}
}

#endif


