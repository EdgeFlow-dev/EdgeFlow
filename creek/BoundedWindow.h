/*
 * BoundedWindow.h
 *
 *  Created on: Jun 21, 2016
 *      Author: xzl
 */

#ifndef BOUNDEDWINDOW_H_
#define BOUNDEDWINDOW_H_

#include <climits>
//#include "boost/date_time/posix_time/posix_time.hpp"
//#include "boost/date_time/special_defs.hpp"

#include "creek-types.h"

//using namespace boost::posix_time;
//using namespace boost::gregorian;
//using namespace boost::date_time;

using namespace creek;

class BoundedWindow {
public:
	static const etime TIMESTAMP_MIN_VALUE;
	static const etime TIMESTAMP_MAX_VALUE;

	/**
	 * Returns the inclusive upper bound of timestamps for values in this window.
     */
	 virtual etime maxTimestamp() = 0;

	 virtual ~BoundedWindow() { }
};


/**
 * The default window into which all data is placed (via {@link GlobalWindows}).
 */
class GlobalWindow : public BoundedWindow{
private:

  // Triggers use maxTimestamp to set timers' timestamp. Timers fires when
  // the watermark passes their timestamps. So, the maxTimestamp needs to be
  // smaller than the TIMESTAMP_MAX_VALUE.
  // One standard day is subtracted from TIMESTAMP_MAX_VALUE to make sure
  // the maxTimestamp is smaller than TIMESTAMP_MAX_VALUE even after rounding up
  // to seconds or minutes.
  static etime END_OF_GLOBAL_WINDOW;

public:

  /**
   * Singleton instance of {@link GlobalWindow}.
   */
  // xzl: c++ seems to have problem by such a nested/forward decl.
//  static GlobalWindow INSTANCE;

  etime maxTimestamp() {
    return END_OF_GLOBAL_WINDOW;
  }
};

//namespace global_window {
//  static GlobalWindow INSTANCE;
//  static ptime END_OF_GLOBAL_WINDOW = TIMESTAMP_MAX_VALUE - days(1);
//}


#endif /* BOUNDEDWINDOW_H_ */
