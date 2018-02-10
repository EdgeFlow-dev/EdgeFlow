#ifndef SOURCE_H
#define SOURCE_H

#include <iostream>

#include <stdlib.h>
#include <fstream>
#include <boost/lexical_cast.hpp>
#include <zmq.hpp>
#include <unistd.h>

//#include "hexdump.hpp"
#include "log.h"
#include "csv.h"

#define TS_POS 0

// key value type
namespace zmq_source {
#ifdef USE_32BIT_ELEMENT /* unsigned. we don't want to be negative when @ts exceeds 32 bit range */
	using simd_t 	= uint32_t;
	using ts_t		= uint32_t;
#else
	using simd_t 	= int64_t;
	using ts_t		= int64_t;
#endif

	template<int N>
	using xN = std::array<simd_t, N>;

	using x2 = xN<2>;
//	using x2		= std::pair<simd_t, simd_t>;
//	using x3		= std::tuple<simd_t, simd_t, simd_t>;
	using x3		= std::array<simd_t, 3>;
	using x4		= std::array<simd_t, 4>;
//	using x4		= std::tuple<simd_t, simd_t, simd_t, simd_t>;

	using x10 = xN<10>;
}

using namespace std;
using namespace zmq_source;

template<typename T>
class SourceBase {
private:
	uint64_t b_idx = 0;

	const string addr;
	uint8_t rec_type;
	uint64_t ts;
	uint64_t ts_delta;

protected:
	uint64_t rpb;	/* recs per bundle */

private:
	uint64_t interval;

protected:
	vector<T *> bundle_pool;
	T *kv;

public:
	SourceBase(const string & addr, uint8_t rec_type, uint64_t ts_start,
				uint64_t ts_delta, uint64_t rpb, uint64_t interval)
		: addr(addr), rec_type(rec_type), ts(ts_start), ts_delta(ts_delta), rpb(rpb), interval(interval){ }

	~SourceBase() { }

	T* GetOneBundle(void) {
		/* update bp idx */
		if (b_idx >= bundle_pool.size())
			b_idx = 0;

		T *kv = bundle_pool[b_idx];
		AddTimeStamp(kv);
		return bundle_pool[b_idx++];
	}

	virtual void MakeBundlePool(void) { }

#if 1
	void SendMessage() {
		/* Prepare our context and sender */
		/* zmq context is thread safe and may be shared among as many application threads as necessary,
		   without any additional locking required on the part of the caller */
		zmq::context_t context(4 /* # of io_threads */);
		zmq::socket_t sender(context, ZMQ_PUSH);

#if 1
		/* high water mark setting */
		uint64_t hwm = 100 ;
		/* the defualt ZMQ_HWM value of zero means "no limit". */
#if ZMQ_VERSION < ZMQ_MAKE_VERSION(3, 3, 0)
		sender.setsockopt(ZMQ_HWM, &hwm, sizeof(hwm));
#else
		sender.setsockopt(ZMQ_SNDHWM, &hwm, sizeof(hwm));
#endif

#endif
		sender.bind(addr.c_str());

//		std::cout << "Press Enter when the workers are ready: " << std::endl;
//		getchar();
		std::cout << "Sending tasks to workers...\n" << std::endl;

		void *bundle;
		uint64_t msg_size = sizeof(simd_t) * rec_type * rpb;

		zmq::message_t message(msg_size);

		while (1) {
			/* add timestamp */
			//		add_timestamp<T>(bundle_pool[offset], config.recs_per_bundle, ts);

			/* get one record bundle */
			//		bundle = (void *) bundle_pool[offset];

			bundle = (void *) GetOneBundle();
			/* construct message */
			message.rebuild(msg_size);
			memcpy(message.data(), bundle, msg_size);

			/* hex dump */
			//		neolib::hex_dump(message.data(), message.size(), std::cout);

			/* send message */
			sender.send(message);
			W("[send msg] bundle offset : %ld timestamp : %ld", b_idx, ts);

			usleep(interval);
		}
	}
#endif
private:
	void AddTimeStamp(T *kv) {
		/* xzl: @ts should be the last column. see Record.h */
		static_assert(std::tuple_size<T>::value - 1 != 0, "can't only have the ts column");
		xzl_bug_on(!kv);

		for (uint64_t i = 0; i < rpb; i++) {
			//			std::get<TS_POS>(kv[i]) = ts;
			std::get<std::tuple_size<T>::value - 1>(kv[i]) = ts;
		}
		ts += ts_delta;		/* update offset and timestamp */
	}

};


#if 0

class FileSource : public SourceBase<T> {
private:
	const string fname;

public:
	FileSource(const string & addr, const string & fname, uint8_t rec_type,
			uint64_t ts_start, uint64_t ts_delta, uint64_t rpb, uint64_t interval)
		: SourceBase<T>(addr, rec_type, ts_start, ts_delta, rpb, interval), fname(fname) {
		uint64_t lines;

		I("Check lines of rec in the file");
		lines = LoadFile(this->fname.c_str());

		this->kv = (T *) malloc (sizeof(T) * lines);
	}

	~FileSource() {
		this->bundle_pool.clear();
		free(this->kv);
	}

	void MakeBundlePool(void) {
		ParseCSVFile(this->rpb);
	}
private:
	uint64_t ParseCSVFile(uint64_t nrecs);

	int skip_comment_lines(FILE *file) {

		int count = 0;
		int ret;
		char linebuffer[256]; /* for comment line. this should be fine as long as loading file is single threaded */

		xzl_bug_on(!file);

		while (true) {
			ret = fscanf(file, "#%s\n", linebuffer);
			if (ret == EOF || ret == 0 /* mismatch */)
				return count;
			xzl_bug_on(ret != 1); /* must be a match */
			count ++;
		}
	}

	/* return # of lines */
	uint64_t LoadFile(const char *fname) {
		uint64_t cnt = 0;
		std::ifstream fin(fname);

		if(!fin.is_open()) {
			EE("file open error");
			abort();
		} else {
			std::string line;
			/* read one line and remove unnecessary thigns (e.g., white space) */
			while (std::getline(fin, line)) {
				/* skip comment */
				if (line.c_str()[0] == '#')
					continue;
				cnt++;
			}
			fin.close();
			I("Number of Records: %ld", cnt);
		}
		return cnt;
	}


};


template<>
uint64_t FileSource<zmq_source::x3>::ParseCSVFile(uint64_t nrecs) {
	using namespace io;

	unsigned long start = 0;
	unsigned long cnt = 0;

	CSVReader<2, trim_chars<' ', '\t'>, no_quote_escape<','>, throw_on_overflow, single_line_comment<'#'>> in(fname);

	//	kv[cnt] = std::make_tuple(ts, 0, 0);
	kv[cnt] = std::make_tuple(0, 0, 0);

	while(in.read_row(std::get<1>(kv[cnt]), std::get<2>(kv[cnt]))) {
		cnt++;

		if (cnt % nrecs == 0) {
			/* add bundle into pool */
			bundle_pool.push_back(&kv[start]);
			start = cnt;
			//			ts += ts_delta;		/* increase timestamp */
#if 0
			x3 *tmp;
			tmp = bundle_pool.back();
			for (int i = 0 ; i < nrecs; i++) {
				cout << std::get<0>(tmp[i]) << std::get<1>(tmp[i]) << std::get<2>(tmp[i]) << endl;
			}
#endif
		}
		//		kv[cnt] = std::make_tuple(ts, 0, 0);
		kv[cnt] = std::make_tuple(0, 0, 0);
	}

	return cnt;
}

template<>
uint64_t FileSource<zmq_source::x4>::ParseCSVFile(uint64_t nrecs) {
	using namespace io;

	unsigned long start = 0;
	unsigned long cnt = 0;

	CSVReader<3, trim_chars<' ', '\t'>, no_quote_escape<','>, throw_on_overflow, single_line_comment<'#'>> in(fname);

	kv[cnt] = std::make_tuple(0, 0, 0, 0);
	//	kv[cnt] = std::make_tuple(ts, 0, 0, 0);

	while(in.read_row(std::get<1>(kv[cnt]), std::get<2>(kv[cnt]), std::get<3>(kv[cnt]))) {
		cnt++;
		if (cnt % nrecs == 0) {
			/* add bundle into pool */
			bundle_pool.push_back(&kv[start]);
			start = cnt;
			//			ts += ts_delta;		/* increase timestamp */
#if 0
			x4 *tmp;
			tmp = bundle_pool.back();
			for (int i = 0 ; i < nrecs; i++) {
				cout << std::get<0>(tmp[i]) << std::get<1>(tmp[i]) << std::get<2>(tmp[i]) << std::get<3>(tmp[i]) << endl;
			}
#endif
		}
		//		kv[cnt] = std::make_tuple(ts, 0, 0, 0);
		kv[cnt] = std::make_tuple(0, 0, 0, 0);
	}

	return cnt;
}

#if 0
template<typename T>
class RandomSource : public SourceBase<T> {
private:

public:
	RandomSource(const string & addr, uint8_t rec_type,
			uint64_t ts_start, uint64_t ts_delta, uint64_t rpb, uint64_t interval)
		: SourceBase<T>(addr, rec_type, ts_start, ts_delta, rpb, interval) {

		uint64_t lines;

		I("Check lines of rec in the file");
		lines = LoadFile(this->fname.c_str());

		this->kv = (T *) malloc (sizeof(T) * lines);
	}

	~FileSource() {
		this->bundle_pool.clear();
		free(this->kv);
	}

private:
	uint64_t ParseCSVFile(uint64_t nrecs);

	void MakeBundlePool(void) {
		ParseCSVFile(this->rpb);
	}

	int skip_comment_lines(FILE *file) {

		int count = 0;
		int ret;
		char linebuffer[256]; /* for comment line. this should be fine as long as loading file is single threaded */

		xzl_bug_on(!file);

		while (true) {
			ret = fscanf(file, "#%s\n", linebuffer);
			if (ret == EOF || ret == 0 /* mismatch */)
				return count;
			xzl_bug_on(ret != 1); /* must be a match */
			count ++;
		}
	}

	/* return # of lines */
	uint64_t LoadFile(const char *fname) {
		uint64_t cnt = 0;
		std::ifstream fin(fname);

		if(!fin.is_open()) {
			EE("file open error");
			abort();
		} else {
			std::string line;
			/* read one line and remove unnecessary thigns (e.g., white space) */
			while (std::getline(fin, line)) {
				/* skip comment */
				if (line.c_str()[0] == '#')
					continue;
				cnt++;
			}
			fin.close();
			I("Number of Records: %ld", cnt);
		}
		return cnt;
	}


};


template<>
uint64_t FileSource<zmq_source::x3>::ParseCSVFile(uint64_t nrecs) {
	using namespace io;

	unsigned long start = 0;
	unsigned long cnt = 0;

	CSVReader<2, trim_chars<' ', '\t'>, no_quote_escape<','>, throw_on_overflow, single_line_comment<'#'>> in(fname);

	//	kv[cnt] = std::make_tuple(ts, 0, 0);
	kv[cnt] = std::make_tuple(0, 0, 0);

	while(in.read_row(std::get<1>(kv[cnt]), std::get<2>(kv[cnt]))) {
		cnt++;

		if (cnt % nrecs == 0) {
			/* add bundle into pool */
			bundle_pool.push_back(&kv[start]);
			start = cnt;
			//			ts += ts_delta;		/* increase timestamp */
#if 0
			x3 *tmp;
			tmp = bundle_pool.back();
			for (int i = 0 ; i < nrecs; i++) {
				cout << std::get<0>(tmp[i]) << std::get<1>(tmp[i]) << std::get<2>(tmp[i]) << endl;
			}
#endif
		}
		//		kv[cnt] = std::make_tuple(ts, 0, 0);
		kv[cnt] = std::make_tuple(0, 0, 0);
	}

	return cnt;
}

template<>
uint64_t FileSource<zmq_source::x4>::ParseCSVFile(uint64_t nrecs) {
	using namespace io;

	unsigned long start = 0;
	unsigned long cnt = 0;

	CSVReader<3, trim_chars<' ', '\t'>, no_quote_escape<','>, throw_on_overflow, single_line_comment<'#'>> in(fname);

	kv[cnt] = std::make_tuple(0, 0, 0, 0);
	//	kv[cnt] = std::make_tuple(ts, 0, 0, 0);

	while(in.read_row(std::get<1>(kv[cnt]), std::get<2>(kv[cnt]), std::get<3>(kv[cnt]))) {
		cnt++;
		if (cnt % nrecs == 0) {
			/* add bundle into pool */
			bundle_pool.push_back(&kv[start]);
			start = cnt;
			//			ts += ts_delta;		/* increase timestamp */
#if 0
			x4 *tmp;
			tmp = bundle_pool.back();
			for (int i = 0 ; i < nrecs; i++) {
				cout << std::get<0>(tmp[i]) << std::get<1>(tmp[i]) << std::get<2>(tmp[i]) << std::get<3>(tmp[i]) << endl;
			}
#endif
		}
		//		kv[cnt] = std::make_tuple(ts, 0, 0, 0);
		kv[cnt] = std::make_tuple(0, 0, 0, 0);
	}

	return cnt;
}

#endif


#endif

#endif
