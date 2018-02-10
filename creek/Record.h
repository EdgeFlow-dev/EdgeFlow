//
// Created by xzl on 8/23/17.
//

#ifndef CREEK_RECORD_H_H
#define CREEK_RECORD_H_H

// a part of a PCollection. elements stored in a vector
// XXX add timestamp and window to each element?

#include "creek-types.h"

//#include "boost/date_time/posix_time/ptime.hpp"
//using namespace boost::posix_time;

template <typename T>
struct Record {

	using ElementT = T;

	/* @data should come before @ts, so that x-th item in data (e.g. a tuple) is also x-th item in Record. */
	T data;
	creek::etime ts;

	Record(const T& data, const creek::etime & ts)
			: data(data), ts(ts) { }

	// no ts provided.
	Record(const T& data) : data(data), ts(-1) { }

	// we seem to need this, as container may initialize a blank record?
	Record() { }
};

#if 0 /* todo */
/* make Record support std::get */
namespace std {
	template<typename T, size_t I> Record &get(Record<T> &r) { return c.array[I]; }
}
#endif

#endif //CREEK_RECORD_H_H
