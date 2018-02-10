/*
 * creek-types.h
 *
 *  Created on: Jan 15, 2017
 *      Author: xzl
 */

#ifndef CREEK_TYPES_H_
#define CREEK_TYPES_H_

#include <string>
#include <climits>
#ifdef USE_FOLLY_STRING
#include "folly/FBString.h"

namespace creek {
	using string = folly::fbstring;
}

/* hasher for fbstring. inject into the tbb namespace, must be done before including concurrent ds. otherwise
 * they will fall back to default tbb hasher. */
namespace tbb {
	size_t tbb_hasher(const folly::fbstring & key);
}

	/* for dbg -- ideally should provide tbb_hasher() */
//	struct FBStringHash {
//		size_t operator()(const folly::fbstring & key)  const {
//			return tbb::tbb_hasher(key);
//		}
//	};
#else
namespace creek {
	using string = std::string;
}
#endif

namespace creek {
	#ifdef USE_NUMA_ALLOC
	#include "utilities/threading.hh"
	template<typename T>
		using allocator = Kaskade::NumaAllocator<T>;
	#else
	template<typename T>
		using allocator = std::allocator<T>;
	#endif
}

namespace creek {
//	using ippair = uint64_t;
	using ippair = long;
	using tvpair = std::pair<long, long>;

	/* our std kvpair -- have to be consistent with KVTYPE_FMT below */
	using ktype =  uint32_t;
	using vtype = uint32_t;
	using kvpair = std::pair<ktype, vtype>;
	using k2v = std::tuple<ktype, ktype, vtype>;

	/* event time in records. start from 0.
	 * conceptually, this is in us. but our code does not depend on that.
	 * boost ptime is too verbose.  */
	using etime = int32_t;
	const etime etime_max = INT_MAX;
	const etime etime_min = INT_MIN;
	const etime etime_zero = 0;
	const etime epoch = 0;
}

/* helper */
#include <numa.h>
namespace  creek {
	static inline int num_numa_nodes() {
#ifdef ARCH_KNL
		return 1;
#else
		return numa_max_node() + 1;
#endif
	}
}

#define KTYPE_FMT	"%08u"
#define VTYPE_FMT	"%08u"
#define ETIME_FMT "%08d"

#ifdef USE_TBB_DS

#include "creek-map.h"

/* -- concurrent vector -- */
#include "tbb/concurrent_unordered_map.h"
#include "tbb/concurrent_vector.h"
#include "tbb/concurrent_unordered_set.h"

namespace creek {
	template<class T>
	using concurrent_vector = tbb::concurrent_vector<T>;

	template<class T>
	using concurrent_vector_ptr = shared_ptr<creek::concurrent_vector<T>>;

	template<class T>
	using concurrent_unordered_set = tbb::concurrent_vector<T>;
//    using concurrent_vector_ptr = shared_ptr<creek::concurrent_vector>;
}
#endif

#endif /* CREEK_TYPES_H_ */
