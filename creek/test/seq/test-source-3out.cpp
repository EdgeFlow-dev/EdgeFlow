//
// Created by xzl on 10/3/17.
//


#include "config.h"

#include "Source/SourceSeqZmqEval.h"
#include "Source/SourceZmqEval.h"
#include "Source/SourceSeqEval.h"

#include "core/Pipeline.h"
#include "Sink/RecordSeqBundleSinkEval.h"

#include "test/test-common.h"
#include "BundleContainer.h"

using T = creek::kvpair;
#define KEYPOS 0
#define VPOS 1
#define INPUT_FILE "/ssd/kv.txt"


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


using T = creek::kvpair;
#define KEYPOS 0
#define VPOS 1

/* Source --> Sink */

void testSourceSink()
{
	SourceSeqZmq<T>
//	SourceSeq<T>
//	SourceZmq<T>
	unbound("unbounded-inmem",
							config.input_file.c_str(),
							config.records_per_epoch , /* records per wm interval */
							config.target_tput, 						/* target tput (rec/sec) */
							config.record_size,
							0,  							  /* session_gap_ms */
							config.source_tasks,
							3 									/* num of output streams */
	);

	// create a new pipeline
	Pipeline* p = Pipeline::create(NULL);

	source_transform_1to3(unbound);
//	source_transform_1to2(unbound);

	RecordSeqBundleSink<T> sink0("sink0", SIDE_INFO_NONE);
	RecordSeqBundleSink<T> sink1("sink1", SIDE_INFO_NONE);
	RecordSeqBundleSink<T> sink2("sink2", SIDE_INFO_NONE);

//	connect_transform_1to2(unbound, sink0, sink1);
	connect_transform_1to3(unbound, sink0, sink1, sink2);

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

