//
// Created by xzl on 8/23/17.
//
// cf: WinGBK.h
//
// this impl using SeqBuf underneath

#ifndef CREEK_WINGBKSEQ_H
#define CREEK_WINGBKSEQ_H

#include "core/Transforms.h"

namespace creek {
	enum WinGBKMode {
		WINGBK_BYKEY,		/* sort by a primary key and optionally a secondary key */
		WINGBK_WINONLY  /* fixed window into only */
	};
}

template <class T, int keypos, unsigned N, int k2pos>
class WinGBKSeqEval;

// input bundle: RecordSeqBundle
// output bundle: WinKeySeqBundle
// N: # of partitions in each output window.
// specify k2pos to sort on a secondary key
template <class KVPair, int keypos, unsigned N, int k2pos = -1>
class WinGBKSeq : public PTransform {
	using InputT = Record<KVPair>;

public:
	const etime window_size;
	const etime start; // the starting point of windowing.
	const WinGBKMode mode;

	WinGBKSeq(string name, int sideinfo, WinGBKMode mode, etime window_size,
				 etime start = creek::epoch)
			: PTransform(name, sideinfo), window_size(window_size), start(start), mode(mode) { }

	void ExecEvaluator(int nodeid, EvaluationBundleContext *c,
										 shared_ptr<BundleBase> bundle_ptr) override {

//#ifndef NDEBUG /* if evaluators get stuck ...*/
//		static atomic<int> outstanding (0);
//#endif
		/* instantiate an evaluator */
		WinGBKSeqEval<KVPair, keypos, N, k2pos> eval(nodeid);

		/* sanity check */
		xzl_bug_on(keypos >= reclen<Record<KVPair>>() || keypos < 0);
		xzl_bug_on(k2pos >= reclen<Record<KVPair>>());

//#ifndef NDEBUG		// for debug
//		outstanding ++;
//#endif
		eval.evaluate(this, c, bundle_ptr);

//#ifndef NDEBUG   // for debug
//		outstanding --; I("end eval... outstanding = %d", outstanding);
//#endif
	 }

#if 0
	/* internal accounting  -- to be updated by the evaluator.
	 * XXX combine this with StatefulTransform's (same func) */
	atomic<unsigned long> byte_counter_, record_counter_;
	/* last time we report */
	unsigned long last_bytes = 0, last_records = 0;
	boost::posix_time::ptime last_check, start_time;
	int once = 1;

	bool ReportStatistics(PTransform::Statstics* stat) override {
		/* internal accounting */
		unsigned long total_records =
				record_counter_.load(std::memory_order_relaxed);
		unsigned long total_bytes =
				byte_counter_.load(std::memory_order_relaxed);

		auto now = boost::posix_time::microsec_clock::local_time();

		if (once) {
			once = 0;
			last_check = now;
			start_time = now;
			last_records = total_records;
			return false;
		}

		boost::posix_time::time_duration diff = now - last_check;

		{
			double interval_sec = (double) diff.total_milliseconds() / 1000;
			double total_sec = (double) (now - start_time).total_milliseconds() / 1000;

			stat->name = this->name.c_str();
			stat->mbps = (double) total_bytes / total_sec;
			stat->mrps = (double) total_records / total_sec;

			stat->lmbps = (double) (total_bytes - last_bytes) / interval_sec;
			stat->lmrps = (double) (total_records - last_records) / interval_sec;

			last_check = now;
			last_bytes = total_bytes;
			last_records = total_records;
		}

		return true;
	}
#endif

};


#endif //CREEK_WINGBKSEQ_H
