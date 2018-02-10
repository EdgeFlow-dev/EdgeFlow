//
// Created by xzl on 12/28/17.
//
// cf. Source Seq.h

#ifndef CREEK_SOURCEZMQ_H
#define CREEK_SOURCEZMQ_H

#include <zmq.hpp> /* for zmq */

#include "pipeline-config.h"
//#include "uarray.h"

#include "core/Transforms.h"
#include "SeqBundle.h"
#include "BundleContainer.h"

template <typename T>
class SourceZmqEval;


/* Producing SeqBuf of Records of kvpairs/tuples.
 * Output bundle format; RecordSeqBundle
 *
 * We don't use UnboundedInMem for Join, which is cluttered
 *
 * cf: UnboundedInMem.h
 *
 * @T: element type, e.g. kvpair
 */

template<typename T>
class SourceZmq : public PTransform {
	using OutputBundleT = RecordSeqBundle<T>;

public:
	int interval_ms;  /* the ev time difference between consecutive bundles */

	const char * input_fname;

	const int punc_interval_ms = 1000;
//	const unsigned long records_per_interval; //
	int record_len = 0; /* the length covered by each record */
	const int session_gap_ms; // for tesging session windows
	const uint64_t target_tput; // in record/sec
	const unsigned int num_source_tasks;

	/* # of (duplicated) output streams */
	const unsigned num_outputs;

	zmq::context_t zcontext;

	uint64_t buffer_size_records = 0;

	SourceZmq(string name, const char *input_fname,
						unsigned long rpi /* not used */, uint64_t tt,
						uint64_t record_size, int session_gap_ms,
						unsigned int n_source_tasks,
						unsigned num_outputs,
						int sideinfo = SIDE_INFO_NONE)
			: PTransform(name, sideinfo), input_fname(input_fname),
				record_len(record_size),
				session_gap_ms(session_gap_ms),
				target_tput(tt),
				num_source_tasks(n_source_tasks),
				num_outputs(num_outputs),
				zcontext(1 /* # of io threads */) {

		interval_ms = 50;
//		const unsigned element_len = sizeof(T);

		int num_nodes = creek::num_numa_nodes();

		xzl_bug_on_msg(record_len != sizeof(Record<T>), "bug: record length mismatch");

		/* print source config info. do this before we actual load the file */

		printf("---- source configuration ---- \n");
		printf("source addr: %s \n", input_fname);
		printf("Note: the network source determines # records per bundle.\n"
		"ingress produces watermarks every X bundles (see below). Hence, ingress has no notion\n"
							 " of records_per_epoch\n"
		);
		printf("%10s %10s %10s %10s %10s %10s\n",
					 "#nodes", "MB", "KRec/epoch", "MB/epoc", "target:KRec/S", "RecSize");
		printf("%10d %10lu %10s %10s %10lu %10d\n",
					 num_nodes,
					 buffer_size_records / 1000,
					 "[sender]",
					 "[sender]",
					 target_tput / 1000,
					 record_len);

		printf("%20s %20s %20s\n",
					 "#bundles/epoch", "bundle_sz(krec)", "#source_tasks");
		printf("%20lu %20s %20lu\n",
					 config.bundles_per_epoch,
					 "[sender]",
					 config.source_tasks
		);
		VV(" ---- punc internal is %d sec (ev time) --- ", this->punc_interval_ms / 1000);
		printf("---- source configuration ---- \n");

	}

	// source, no-op. note that we shouldn't return the transform's wm
	virtual etime RefreshWatermark(etime wm) override {
		return wm;
	}

	/* impl goes to .cpp to avoid circular inclusion */
	void ExecEvaluator(int nodeid, EvaluationBundleContext *c, shared_ptr<BundleBase> bundle_ptr) override {
		/* instantiate an evaluator */
		SourceZmqEval<T> eval(nodeid);
		eval.evaluate(this, c, bundle_ptr);
	}
};

#endif //CREEK_SOURCEZMQ_H
