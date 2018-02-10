//
// Created by xzl on 10/3/17.
//


#include "config.h"

#include "core/Pipeline.h"
#include "Source/SourceZmqEval.h"
#include "Source/SourceSeqEval.h"
#include "Sink/RecordSeqBundleSinkEval.h"

#include "test/test-common.h"

//using T = creek::kvpair;
//#define KEYPOS 0
//#define VPOS 1
//#define INPUT_FILE "/ssd/kv.txt"

using T = creek::k2v;
#define KEYPOS 0
#define VPOS 1
#define INPUT_FILE "/ssd/k2v.txt"

/* default config. can override on cmdline options */
#ifdef DEBUG
pipeline_config config = {
		.records_per_epoch = 1000,
		.target_tput = (20 * 1000),
		.record_size = sizeof(Record<T>),
		.input_file = INPUT_FILE,
};
#else
pipeline_config config = {
		.records_per_epoch = SZ_1M,
		.target_tput = SZ_1M,
		.record_size = sizeof(Record<T>),
		.input_file = INPUT_FILE,
};
#endif

/* Source --> Sink */

void testSourceSink()
{
#if 1
	SourceZmq<T>
			unbound("unbounded-inmem",
							config.input_file.c_str(),
							config.records_per_epoch , /* records per wm interval */
							config.target_tput, 						/* target tput (rec/sec) */
							config.record_size,
							0,  													/* session_gap_ms */
							config.source_tasks,
							1 									/* num of output streams */
	);
#else
	SourceSeq<T>
			unbound("unbounded-inmem",
							config.input_file.c_str(),
							config.records_per_epoch , /* records per wm interval */
							config.target_tput, 						/* target tput (rec/sec) */
							config.record_size,
							0  													/* session_gap_ms */
	);
#endif

	// create a new pipeline
	Pipeline* p = Pipeline::create(NULL);

	PCollection *unbound_output = dynamic_cast<PCollection *>(p->apply1(&unbound));
	unbound_output->_name = "unbound_output";

	RecordSeqBundleSink<T> sink("sink");

	connect_transform(unbound, sink);

	EvaluationBundleContext eval(1, config.cores);
	eval.runSimple(p);
}

int main(int ac, char *av[])
{
	print_config();
	parse_options(ac, av, &config);
#ifdef USE_TZ
	xplane_init();
#endif

	testSourceSink();
}

