//
// Created by xzl on 8/23/17.
//

#ifndef CREEK_WINDOW_H
#define CREEK_WINDOW_H

#include "config.h"
#include "creek-types.h"

////////////////////////////////////////////////////////////////////////

/* we need to hash this object
 * ref: http://stackoverflow.com/questions/1574647/a-way-to-turn-boostposix-timeptime-into-an-int64 */
#if 0
template <unsigned int duration_ms>
struct Window {
//  ptime start;
  boost::posix_time::time_duration start; // window start
  const static boost::posix_time::time_duration duration; // not needed if we do fixed window
  const static ptime epoch; // the start of our time
};

template <unsigned int duration_ms>
const boost::posix_time::time_duration
  Window<duration_ms>::duration = milliseconds(duration_ms);

template <unsigned int duration_ms>
const ptime
  Window<duration_ms>::epoch = boost::gregorian::date(1970, Jan, 1);
#endif

struct Window {
	creek::etime start; // window start
	// cannot be const, since it'll block the implict copy function
	// its value is not used in sorting

	// the length of the window. not needed if we do fixed window
	mutable creek::etime duration;
//	const static creek::etime epoch; // the start of our time -- use creek::etime_zero instead

	Window (creek::etime start, creek::etime const & duration)
			: start(start - creek::epoch), duration(duration) { }

	// dummy ctor -- just do nothing
	Window () { }

	inline creek::etime window_start() const {
		return creek::epoch + start;
	}

	inline creek::etime window_end() const {
		return creek::epoch + start + duration;
	}

	// assuming unified sized window, we only compare their start time.
	// this enables Window as key in std::map.
#if 0
	bool operator()(const Window*& lhs, const Window*& rhs) const {
    return lhs->start < rhs->start;
  }
#endif
	bool operator()(const Window& lhs, const Window& rhs) const {
		return lhs.start < rhs.start;
	}
};

/* one struct. needed if we use tbb hash table.
 * see https://www.threadingbuildingblocks.org/docs/help/tbb_userguide/More_on_HashCompare.html */
struct WindowHashCompare {
	static size_t hash(const Window & w) {
//		return (size_t)(w.duration.ticks());
		return (size_t)(w.duration);
	}

	static bool equal(const Window& lhs, const Window& rhs) {
		return (lhs.start == rhs.start && lhs.duration == rhs.duration);
	}
};

/* needed for (tbb) unordered map */
struct WindowHash {
	size_t operator()(const Window & w)  const {
		return (size_t)(w.duration);
	}
};

struct WindowEqual {
	bool operator()(const Window& lhs, const Window& rhs) const {
		return (lhs.start == rhs.start && lhs.duration == rhs.duration);
	}
};


#if 0 /* not in use. maybe useful in the future? */
// window that facilitates on-the-fly merge
// @T: the element type, e.g. long
template<typename T>
struct SessionWindow : public Window {
	using RecordT = Record<T>;
	using InputBundleT = RecordBundle<T>;

	/* a space-efficient way to store records that fall in the window.
	 * motivation: in the raw input stream, consecutive records often
	 * fall into the same session window; thus, they can be stored as
	 * (start_record, cnt) instead of individual records.
	 *
	 * the locality exists because we assume that windowing.is done before
	 * GBK (which may break consecutive records into different key groups)
	 */
	struct RecordRange {
		// both inclusive
		RecordT const * start = nullptr;
		RecordT const * end = nullptr;
//    unsigned long cnt;
	};

	enum class RETCODE {
		OK = 0,
		/* no overlap */
				TOO_EARLY,
		TOO_LATE,
		/* has overlap */
				MODIFY_START,
	};

	// perhaps we should use list since we merge often
	mutable vector<RecordRange> ranges;

	// since we only store pointers (not the actual records in the input bundle),
	// we need to hold pointers to the input bundle so that they do not go away
	// when all the windows referring to an input bundle are purged, the input
	// bundle is automatically destroyed.
	mutable unordered_set<shared_ptr<InputBundleT>> referred_bundles;

	// length of each record
	const boost::posix_time::time_duration record_duration;

	// is the session window *strictly* smaller than the record's window?
	bool operator()(const SessionWindow & lhs, const RecordT & rhs) const {
		return lhs.window_end() <= rhs.ts;
	}

#if 0
	bool operator<(const SessionWindow & lhs, const RecordT & rhs) const {
    return lhs.window_end() <= rhs.ts;
  }
#endif

	bool operator<(const RecordT & rhs) const {
		return this->window_end() <= rhs.ts;
	}

	bool operator<(const SessionWindow& rhs) const {
		return this->start < rhs.start;
	}

	/* see SessionWindowInto::RetrieveWindows() */
	bool operator<(const boost::posix_time::ptime & watermark) const {
		return this->window_end() <= watermark;
	}

#if 1
	/* this may modify window's start point. we cannot call this on any window
		 that is already in a set.
		 @return: whether add succeeds (i.e. record falls into the window).
	*/
	bool try_add_record (RecordT const * rec,
											 shared_ptr<InputBundleT> bundle) {
		assert(rec);
		ptime && wstart = window_start();
		ptime && wend = window_end();
		ptime const & rstart = rec->ts;
		ptime rend = rec->ts + record_duration;

		// the window range and record range must intersect
		if (!(
				(rec->ts >= wstart && rec->ts < wend) || (rend >= wstart && rend < wend)
		))
			return false;

		// is @rec contiguous wrt to the previous rec that this window receives?
		if (ranges.empty() || ranges.back().end + 1 != rec) {
			ranges.emplace_back();
			ranges.back().start = ranges.back().end = rec;
		} else
			ranges.back().end = rec;

		// update_window
		if (rstart < wstart) { // extend window start
			this->start = rstart - epoch;
			this->duration += (wstart - rstart);
		}

		if (rend > wend) {  // extend window end
			assert(rstart > wstart);
			this->duration += (rend - wend);
		}

		this->referred_bundles.insert(bundle);

		return true;
	}
#endif

	/*  @return true iff
			1. @rec overlaps with the window,
			2. the merge does not change the window's starting point
			since we'll sort windows based on their starting points, this function
			will not change one window's rank (e.g. in a std::map)

			otherwise false, nothing will happen
	*/
	RETCODE try_add_record_notbefore (RecordT const * rec,
																		shared_ptr<InputBundleT> bundle) const {
		assert(rec);
		ptime && wstart = window_start();
		ptime && wend = window_end();
		ptime const & rstart = rec->ts;
		ptime rend = rec->ts + record_duration;

		/*  [ success ]
		 *
		 *   wstart                       wend
		 *    +----------- window ----------+
		 *            +------------- record ---------+
		 *            rstart                        rend
		 */


		/*
		 *                       wstart                       wend
		 *                        +----------- window ----------+
		 *   +-- record -------+
		 * rstart            rend
		 */
		if (rend <= wstart)
			return RETCODE::TOO_EARLY;

		/*
		 *    wstart                       wend
		 *     +----------- window ----------+
		 *                                            +-- record -------+
		 *                                           rstart            rend
		 */
		if (rstart >= wend)
			return RETCODE::TOO_LATE;

		/*
		 *   [ MODIFY_START, which will modify window's starting point ]
		 *
		 *           wstart                       wend
		 *            +----------- window ----------+
		 *       +------------- record ---------+
		 *      rstart                        rend
		 */
		if (rstart < wstart && rend >= wstart)
			return RETCODE::MODIFY_START;

		// (rend >= wstart && rend < wend)

		// is @rec contiguous wrt to the previous rec that this window receives?
		if (ranges.empty() || ranges.back().end + 1 != rec) {
			ranges.emplace_back();
			ranges.back().start = ranges.back().end = rec;
		} else
			ranges.back().end = rec;

		// update_window
		assert(!(rstart < wstart)); // shouldn't have to extend window start
		if (rend > wend) {  // extend window end
			assert(rstart > wstart);
			this->duration += (rend - wend);
		}

		this->referred_bundles.insert(bundle);

		return RETCODE::OK;
	}

	// merge @other into this window. update this window if succeeds
	// @return: true if merge okay.
	bool merge(SessionWindow const & other) {
		ptime && start = window_start();
		ptime && end = window_end();
		ptime const && ostart = other.window_start();
		ptime const && oend = other.window_end();

		// if no overlap, merge fails
		if (!(
				(ostart >= start && ostart < end) || (oend >= start && oend < end)
		))
			return false;

		// update this window
		if (ostart < start) { // extend window start
			this->start = ostart - epoch;
			this->duration += (start - ostart);
		}

		if (oend > end) {  // extend window end
//      assert(ostart > start);   // possible when we merge window sets
			this->duration += (oend - end);
		}

		this->ranges.insert(this->ranges.end(),
												other.ranges.begin(), other.ranges.end());

		// duplicated ptr to bundles will fail the insertion. so we're fine.
		this->referred_bundles.insert(other.referred_bundles.begin(),
																	other.referred_bundles.end());

		return true;
	}

	// the merge succeeds only when it won't modify this window's starting point
	RETCODE merge_notbefore(SessionWindow const &other) const {
		ptime && start = window_start();
		ptime && end = window_end();
		ptime const && ostart = other.window_start();
		ptime const && oend = other.window_end();

		if (oend <= start)
			return RETCODE::TOO_EARLY;

		if (ostart >= end)
			return RETCODE::TOO_LATE;

		if (ostart < start && oend >= start)
			return RETCODE::MODIFY_START;

		if (oend > end) {  // extend window end
			assert(ostart > start); // since we shouldn't extend window start
			this->duration += (oend - end);
		}

		//cout << *this;
		//cout << other;

		this->ranges.insert(this->ranges.end(),
												other.ranges.begin(), other.ranges.end());

		// duplicated ptr to bundles will fail the insertion. so we're fine.
		this->referred_bundles.insert(other.referred_bundles.begin(),
																	other.referred_bundles.end());

		return RETCODE::OK;
	}

	void print(ostream & os) const {
		os << "dump SessionWindow: " << to_simplest_string(this->window_start()) \
        << "--" << to_simplest_string(this->window_end());
		for (auto && range : this->ranges) {
			os << " { ";
			for (auto ptr = range.start; ptr <= range.end; ptr++) {
				if (ptr - range.start <= 4)
					os << (ptr->data) << " ";
				else {
					os << "....";
					break;
				}
			}
			os << " } ";
		}
		os << endl;
	}

	SessionWindow (ptime start, boost::posix_time::time_duration const & duration,
								 boost::posix_time::time_duration const & record_duration)
			: Window(start, duration), record_duration(record_duration) { }

};

/*
 * Window comparison functions
 */
template<typename T>
struct WindowRecComp {
	// is the session window *strictly* smaller than the record's window?
	bool operator()(const SessionWindow<T> & lhs, const Record<T> & rhs) const {
		return lhs.window_end() <= rhs.ts;
	}
};

template<typename T>
struct WindowStrictComp {
	// is the 1st session window *strictly* smaller than 2nd window?
	bool operator()(const SessionWindow<T> & lhs,
									const SessionWindow<T> & rhs) const {
		return lhs.window_end() <= rhs.window_start();
	}
};
#endif

#endif //CREEK_WINDOW_H
