/*
 * Values.cpp
 *
 *  Created on: Jul 4, 2016
 *      Author: xzl
 *
 *  Do not put TBB/folly stuffs here.
 */

#include "Values.h"

// we must do it here instead of the header
namespace windowed_value {
  GlobalWindow INSTANCE;
  vector<BoundedWindow *> GLOBAL_WINDOWS { &(INSTANCE) };
}

/* avoid ptime underflow problem.
 * (in fact this does not matter?) */
//const ptime Window::epoch(boost::gregorian::date(1970, Jan, 1));
//const creek::etime Window::epoch(0);


template<>
void WindowsBundle<long>::show_window_sizes() {
#ifndef NDEBUG
  unsigned long sum = 0;
  for (auto&& it = vals.begin(); it != vals.end(); it++) {
      auto output = it->second;
      assert(output);
      EE("window %d (start %s) size: %lu",
          output->id, to_simple_string(output->w.start).c_str(),
          output->vals.size());
      sum += output->vals.size();
      for (auto&& v : output->vals)
        printf("%lu ", v.data);
      printf("\n");
  }
#endif
//  EE("----- total %lu", sum);
}

#include "BundleContainer.h"

/* xzl: simply making the connection
 * XXX even for Sink, this will create one output PValue. This can be
 * confusing...
 * */

/* two outputs */

// for stat
atomic<unsigned long> get_bundle_attempt_counter(0);
atomic<unsigned long> get_bundle_okay_counter(0);
atomic<unsigned long> get_one_bundle_from_list_counter(0);