#ifndef VALUES_H
#define VALUES_H

#include <typeinfo>
#include <list>
#include <mutex>
#include <memory>
#include <atomic>
#include <unordered_set>
#include <stdio.h>


#include <numaif.h>
//#include "utilities/threading.hh"

#include "config.h"
#include "creek-types.h"

#include "AppliedPTransform.h"
// xzl: be careful: this file should not include any other headers that
// contain actual implementation code.

//#include "Transforms.h"
#include "core/Pipeline.h"
#include "WindowingStrategy.h"
#include "log.h"

using namespace std;
using namespace creek;

#ifdef USE_NUMA_ALLOC
#include <numaif.h>
#include "utilities/threading.hh"
using namespace Kaskade;
#endif

/////////////////////////////////////////////////////////////////////////


#ifdef USE_TBB_DS  /* needed for output */
#include "tbb/concurrent_unordered_map.h"
#include "tbb/concurrent_vector.h"
#include "tbb/concurrent_unordered_set.h"
#endif

#ifdef USE_FOLLY_HASHMAP
#include "folly/AtomicHashMap.h"
#endif

#ifdef USE_CUCKOO_HASHMAP
#include "src/cuckoohash_map.hh"
#include "src/city_hasher.hh"
#endif

#ifdef CONFIG_CONCURRENT_CONTAINER
#include <concurrentqueue.h> /* using moodycamel's concurrent queue for bundle container */
#endif

/* need this for time measurement. not for pipeline timekeeping */
#include "boost/date_time/posix_time/ptime.hpp"
#include "boost/date_time/posix_time/posix_time.hpp"

/* For stats
 * in Values.cpp */
extern atomic<unsigned long> get_bundle_attempt_counter; /* # attempts of getting bundles from a container */
extern atomic<unsigned long> get_bundle_okay_counter; /* # getting bundles from a container */
extern atomic<unsigned long> get_one_bundle_from_list_counter; /* # attempts of getting bundles from a container list */

/////////////////////////////////////////////////////////////////////////

class PTransform;
//class AppliedPTransform<PInput, POutput, PTransform<PInput, POutput> >;

#if 1 /* xzl: these are for dealing with boost ptime. no longer useful? */
// helper. note: can't return c_str() of a stack string is wrong.
// XXX move to a separate file?
static inline std::string to_simplest_string(const boost::posix_time::ptime & pt)
{
    const unsigned int skip = 12;
    return boost::posix_time::to_simple_string(pt).substr(skip);
}

/* ref: http://stackoverflow.com/questions/22975077/how-to-convert-a-boostptime-to-string */
static inline std::stringstream to_simplest_string1(const boost::posix_time::ptime & pt)
{
    std::stringstream stream;
    boost::posix_time::time_facet* facet = new boost::posix_time::time_facet();
    facet->format("%H:%M:%S%F");
    stream.imbue(std::locale(std::locale::classic(), facet));
    stream << pt;
    return stream;
}
#endif

/* compatibility */
static inline std::string to_simple_string(const creek::etime& pt)
{
	return to_string(pt);
}

static inline std::string to_simplest_string(const creek::etime& pt)
{
	return to_string(pt);
}

static inline std::stringstream to_simplest_string1(const creek::etime& pt)
{
	std::stringstream stream;
	stream << pt;
	return stream;
}

//#ifdef MEASURE_LATENCY
struct mark_entry {

	using ptime = boost::posix_time::ptime;

	string msg;
	ptime ts;
	mark_entry(string const & msg, ptime const & ts)
		: msg(msg), ts(ts) { }
};
//#endif

/* order must be consistent with above */
#if 0
static const char * puncref_desc_[] = {
		"canceled",
		"undecided",
		"consumed",
		"retrieved",
		"assigned",
		""
};
#endif

//#ifdef MEASURE_LATENCY
//static const char * puncref_key_[] = {
//		"X",
//		"U",
//		"C",
//		"R",
//		"A",
//		""
//};
//#endif

/* transform's side info */

//#define LIST_ORDERED 	0
//#define LIST_L 				1
//#define LIST_R 				2
//#define LIST_UNORDERED 3
//#define LIST_MAX				4

/* error code. used by Transforms.h */

//#include "Bundle.h"

#define MAX_NUMA_NODES	8
#define ASSERT_VALID_NUMA_NODE(node) do { 			\
	assert(node >= 0 && node <MAX_NUMA_NODES);		\
} while (0)

//struct BundleBase;

////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////

class WindowingStrategy;

struct BundleBase;

////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////

/**
 * An immutable pair of a value and a timestamp.
 *
 * <p>The timestamp of a value determines many properties, such as its assignment to
 * windows and whether the value is late (with respect to the watermark of a {@link PCollection}).
 *
 * @param <V> the type of the value
 */

/* xzl: replace boost ptime with creek::etime, much easier */
//#include "boost/date_time/posix_time/ptime.hpp"
//using namespace boost::posix_time;

template <class V>
class TimestampedValue {
	using etime = creek::etime;

public:
	// xzl: note that we return value by copy.
	static TimestampedValue<V> of(V value, etime timestamp) {
		return TimestampedValue<V>(value, timestamp);
	}

	V getValue() { return _value; }

	etime getTimestamp() { return _timestamp; }

	bool equals(const TimestampedValue<V>& other) {
		return ((_value == other._value) && (_timestamp == other._timestamp));
	}

	string toString() {
		// xzl: XXX?
		return "TimestampedValue(" + to_string(_value) + \
				", " + "XXX to_string(_timestamp)" + ")";
	}
private:
	V _value;
	etime _timestamp;
};

////////////////////////////////////////////////////////////////////////////

#include <vector>
#include "BoundedWindow.h"

#if 0 // pane info obsoleted
#include "PaneInfo.h"

using namespace pane_info;

namespace windowed_value {
  extern vector<BoundedWindow *> GLOBAL_WINDOWS;
//  vector<BoundedWindow *> GLOBAL_WINDOWS { &(GlobalWindow::INSTANCE) };
//  vector<BoundedWindow *> GLOBAL_WINDOWS { &(global_window::INSTANCE) };
}

template <class T>
class WindowedValue {
protected:
	T 			_value;
	PaneInfo 	_pane;

public:

	WindowedValue(T value, PaneInfo pane) : _value(value), _pane(pane) { }
};

////////////////////////////////////////////

/**
 * The abstract superclass of WindowedValue representations where
 * timestamp == MIN.
 */

template <class T>
class MinTimestampWindowedValue : public WindowedValue<T> {
public:
	MinTimestampWindowedValue(T value, PaneInfo pane)
		: WindowedValue<T>(value, pane) { }

	creek::etime getTimestamp() {
		return BoundedWindow::TIMESTAMP_MIN_VALUE;
	}
};

////////////////////////////////////////////

/**
 * The representation of a WindowedValue where timestamp == MIN and
 * windows == {GlobalWindow}.
 */

template <class T>
class ValueInGlobalWindow : public MinTimestampWindowedValue<T> {
public:
	ValueInGlobalWindow(T value, PaneInfo pane)
		: MinTimestampWindowedValue<T>(value, pane) { }

	// xzl: upcasting?
	WindowedValue<T>* withValue(T value) {
		return (WindowedValue<T>*) new ValueInGlobalWindow(value, this->_pane);
	}

	vector<BoundedWindow *>* getWindows() {
	  return &(windowed_value::GLOBAL_WINDOWS);
	}
	// todo: equals?
};

////////////////////////////////////////////

/**
 * The representation of a WindowedValue where timestamp == MIN and
 * windows == {}.
 */
template <class T>
class ValueInEmptyWindows : public MinTimestampWindowedValue<T> {
public:
  ValueInEmptyWindows(T value, PaneInfo pane)
    : MinTimestampWindowedValue<T>(value, pane) { }

  // xzl: upcasting?
  WindowedValue<T>* withValue(T value) {
    return (WindowedValue<T>*) new ValueInEmptyWindows<T>(value, this->_pane);
  }

  vector<BoundedWindow *>* getWindows() {
    return new vector<BoundedWindow *> { };   // empty
  }
};
#endif

////////////////////////////////////////////

#if 0
// xzl: simplified version of each element (record)
// a record with two data fields
template <typename T1, typename T2>
struct Element2 {
  using Tuple = std::tuple<T1,T2>;

  Tuple cols;
  ptime ts;
  ptime ts2;
  BoundedWindow * window;

  Element2 (Tuple cols,
      ptime ts = max_date_time,
      ptime ts2 = max_date_time,
      BoundedWindow * window = nullptr
  ): cols(cols), ts(ts), ts2(ts2), window(window) { }

};

template <typename T1, typename T2>
struct Header2 {
  typedef std::tuple<vector<T1>*, vector<T2>*> Tuple;
  vector<bool>* selector = nullptr;
  vector<ptime>* ts = nullptr;
  vector<ptime>* ts2 = nullptr;
  Tuple cols;
  BoundedWindow * window = nullptr; // is this okay?
};
#endif

struct Window;

/* xzl: factored these two out */
#include "Record.h"
#include "Bundle.h"
#include "Window.h"

/* compared based on the value of the kvpair. used for sorting
 * the output kvpairs of a reducer.
 */
template<class KVPair>
struct ReducedKVPairCompLess {
  bool operator()(const KVPair & lhs,
      const KVPair & rhs) const {
    /* since we are going to maintain a min heap */
      return lhs.second > rhs.second;
  }
};


template<class KVPair>
struct ReducedKVPairCompLess1 {
  bool operator()(const KVPair & lhs,
      const KVPair & rhs) const {
    /* since we are going to maintain a min heap */
      return lhs.second < rhs.second;
  }
};



////////////////////////////////////////////////////////////////////////

#include <map>
#include <unordered_map>

/* a bunch of KVS belonging to one window. */
template <typename K, typename V>
struct WindowedKVOutput
{
  typedef pair<K, vector<V>> KVS;

  Window w;
  unordered_map<K, KVS*> vals;

  // return: if key (@k) already exists
  bool add(const K& k, const V& v) {
    bool ret = true;
    if (!vals.count(k)) {
        vals[k] = new KVS();
        ret = false;
    }
    vals[k].second.push_back(v);

    return ret;
  }
};

#include "ValueContainer.h"

#include "WinKeyFrag.h"

/* iterator that goes through each <key, value_container> pair in a window
 * map.
 *
 * A window map often has multiple windows, each of which associated with
 * multiple <key, value_container> pairs.
 *
 * an evaluator's unit of work is on values of same window/key,
 * and it may need to distribute multi units of work among workers.
 * this iterator interface makes such distribution easier.
 */
template<typename KVPair>
class window_map_iterator {

	using WindowMap = map<Window, WinKeyFrag_Std<KVPair>, Window>;
	using K = decltype(KVPair::first);
	using V = decltype(KVPair::second);
	using ValueContainerT = ValueContainer<V>;
	/* keys and their value containers (for a given window) */
	using KeyMap = typename WinKeyFrag_Std<KVPair>::KeyMap;
//	using KeyMap = tbb::concurrent_unordered_map<K, ValueContainerT>;

private:
	WindowMap const * _windows;
	typename WindowMap::const_iterator it_win;
	typename KeyMap::const_iterator it_kvcontainer;

	/* Start from the current iter position, skip any empty windows. Stop at:
	 * 1. the 1st valid item (e.g. begin() of the 1st non-empty win)
	 * 2. end() of the last win (which must be empty).
	 *
	 * If already points to a valid item, do nothing and stop there.
	 */
	void skip_empty_windows() {

		assert(!_windows->empty());

		auto lastbutone = _windows->end();
		std::advance(lastbutone, -1);  // last window.

		// currently pointing to a valid item
		while ( (it_kvcontainer == it_win->second.vals.end()) ) {
			// pointing to a window's end()
			if (it_win == lastbutone) {
				/* if we are at the last window, stop */
				return;
			} else {
				// moved to the beginning of the next window
				it_win++;
				it_kvcontainer = it_win->second.vals.begin();
				VV("moved to next win: %s",
						to_simplest_string(it_win->first.window_start()).c_str());
			}
		}
	}

public:
	window_map_iterator(WindowMap const * windows,
			typename WindowMap::const_iterator it_win,
			typename KeyMap::const_iterator it_kvcontainer) :
			_windows(windows), it_win(it_win), it_kvcontainer(it_kvcontainer)
	{
		skip_empty_windows(); /* needed as the 1st win may be empty */
	}

	bool operator!=(window_map_iterator const& other) const {
		/* cannot swap the order: debugging stl will complain */
		return (it_win != other.it_win || it_kvcontainer != other.it_kvcontainer);
	}

	window_map_iterator& operator++() {

#if 0
		/* undefined: if we already hit the end of the last window */
		assert (!(it_kvcontainer == it_win->second.vals.end()
						&& it_win == windows->end()));
#endif

		/* capture the following two situations:
		 * 1. undefined: if we already hit the end of the last window and still try
		 * to ++;
		 * 2. bug: we somehow stopped at the end of an intermediate window
		 */
		if (it_kvcontainer == it_win->second.vals.end()) {
			EE("bug: distance to win end %lu", std::distance(it_win, _windows->end()));
		}

		assert(it_kvcontainer != it_win->second.vals.end());

		it_kvcontainer++;

		skip_empty_windows();

		return *this;
	}

	/* xzl: why we get a warning here if we return by reference? */
	pair<K, ValueContainerT> const operator*() const {
		/* the internal iterator should be valid */
		assert(it_kvcontainer != it_win->second.vals.end());
#if 0
		if (it_kvcontainer == it_win->second.vals.end()) {
			auto lastbutone = _windows->end();
			lastbutone --;

			window_map_iterator itend(_windows, lastbutone,
					lastbutone->second.vals.end());

			if (it_win == lastbutone) {
				W("we are last but one");

				if (*this != itend) {
					W("we dont equal to end");
				} else
				W("we equal to end");
			} else
			W("we are NOT last but one");

			assert(0);
		}
#endif

		return *it_kvcontainer;
	}

	Window const & get_current_window() const {
		return it_win->first;
	}

};

/////////////////////////////////////////////////////////

/* similar to above, but @map uses shared_ptr<WindowKeyedFragmentStd> */

template<typename KVPair>
class window_map_iterator2 {

	using WindowMap = map<Window, shared_ptr<WinKeyFrag_Std<KVPair>>, Window>;

	using K = decltype(KVPair::first);
	using V = decltype(KVPair::second);
	using ValueContainerT = ValueContainer<V>;
	/* keys and their value containers (for a given window) */
	using KeyMap = typename WinKeyFrag_Std<KVPair>::KeyMap;

private:
	WindowMap const * _windows;
	typename WindowMap::const_iterator it_win;
	typename KeyMap::const_iterator it_kvcontainer;

	/* Start from the current iter position, skip any empty windows. Stop at:
	 * 1. the 1st valid item (e.g. begin() of the 1st non-empty win)
	 * 2. end() of the last win (which must be empty).
	 *
	 * If already points to a valid item, do nothing and stop there.
	 */
	void skip_empty_windows() {

		xzl_assert(!_windows->empty());

		auto lastbutone = _windows->end();
		std::advance(lastbutone, -1);  // last window.

		// currently pointing to a valid item
		while ( (it_kvcontainer == it_win->second->vals.end()) ) {
			// pointing to a window's end()
			if (it_win == lastbutone) {
				/* if we are at the last window, stop */
				return;
			} else {
				// moved to the beginning of the next window
				it_win++;
				it_kvcontainer = it_win->second->vals.begin();
				VV("moved to next win: %s",
						to_simplest_string(it_win->first.window_start()).c_str());
			}
		}
	}

public:
	window_map_iterator2(WindowMap const * windows,
			typename WindowMap::const_iterator it_win,
			typename KeyMap::const_iterator it_kvcontainer) :
			_windows(windows), it_win(it_win), it_kvcontainer(it_kvcontainer)
	{
		skip_empty_windows(); /* needed as the 1st win may be empty */
	}

	bool operator!=(window_map_iterator2 const& other) const {
		/* cannot swap the order: debugging stl will complain */
		return (it_win != other.it_win || it_kvcontainer != other.it_kvcontainer);
	}

	/* this can be hot: reducer will invoke this to partition all keys */
	window_map_iterator2 & operator++() {

		/* capture the following two situations:
		 * 1. undefined: if we already hit the end of the last window and still try
		 * to ++;
		 * 2. bug: we somehow stopped at the end of an intermediate window
		 */
		if (it_kvcontainer == it_win->second->vals.end()) {
			EE("bug: distance to win end %lu", std::distance(it_win, _windows->end()));
		}

		assert(it_kvcontainer != it_win->second->vals.end());

		it_kvcontainer++;

		skip_empty_windows();

		return *this;
	}

	/* xzl: why we get a warning here if we return by reference? */
	pair<K, ValueContainerT> const operator*() const {
		/* the internal iterator should be valid */
		assert(it_kvcontainer != it_win->second->vals.end());
		return *it_kvcontainer;
	}

	Window const & get_current_window() const {
		return it_win->first;
	}

};

/////////////////////////////////////////////////////////

/* pointing to a word in the input buffer.
 *
 * this source transform will not create copies of the data, but will defer it
 * to downstream transforms (e.g. mapper)
 */
struct word {
  const char *data;
  uint32_t len;
};

/* represented a segment in the input buffer */
struct string_range {
  const char *data;
  uint64_t len;
};

//hym: for Network latency app
struct srcdst_rtt{
	char sd_ip[32];
	long rtt;
};

/////////////////////////////////////////////////////////

#ifdef USE_TBB_DS
/* an array of set for fixing the scalability bottleneck in a single set.
 *
 * for distinct count */
namespace creek_set_array {
	/* good: 16 seems best
	 * 32 -- worse
	 * 128 -- bad
	 */
	#define NUM_SETS 16
//	using SetArray = tbb::concurrent_unordered_set<creek::string>[NUM_SETS];
using SetArray = std::array<tbb::concurrent_unordered_set<creek::string>, NUM_SETS>;
	using SetArrayPtr = shared_ptr<SetArray>;

	void insert(SetArrayPtr ar, creek::string const & in);

	/* merge two sets */
	void merge(SetArrayPtr mine, SetArrayPtr const & others);

}
#endif

/////////////////////////////////////////////////////////

//#include "creek-map.h"

#endif // TRANSFORMS_H
