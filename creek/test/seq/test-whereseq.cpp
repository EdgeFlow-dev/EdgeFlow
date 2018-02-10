//
// Created by xzl on 9/1/17.
//

#include "config.h"

#include "core/Pipeline.h"

#include "Source/SourceZmqEval.h"
#include "Source/SourceSeqEval.h"
#include "Source/SourceSeqZmqEval.h"
#include "Mapper/WhereSeqMapper.h"
#include "Sink/RecordSeqBundleSinkEval.h"

#include "../test-common.h"
#include "BundleContainer.h"

using T = creek::kvpair;
#define KEYPOS 0
#define VPOS 1
#define INPUT_FILE "/ssd/kv.txt"

//using T = creek::k2v;
//#define KEYPOS 0
//#define VPOS 1
//#define INPUT_FILE "/ssd/k2v.txt"


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

/*
 *      UnboundedInMem
 *            | RecordSeqBundle<kvpair>
 *            V
 *       WhereSeq
 *            | RecordSeqBundle<kvpair>
 *            V
 *          WinKeySeqBundleSink
 *
 */

void testWhereSeq()
{
//	SourceSeq<T>
//	SourceZmq<T>
	SourceSeqZmq<T>
			unbound("unbounded-inmem",
							config.input_file.c_str(),
							config.records_per_epoch , /* records per wm interval */
							config.target_tput, 						/* target tput (rec/sec) */
							config.record_size,
							0,  													/* session_gap_ms */
							config.source_tasks,
							1 									/* num of output streams */
	);

	// create a new pipeline
	Pipeline* p = Pipeline::create(NULL);

	PCollection *unbound_output = dynamic_cast<PCollection *>(p->apply1(&unbound));
	unbound_output->_name = "unbound_output";

	WhereSeqMapper<T, T, KEYPOS, VPOS> where(0/* low */, 500/* high */,
		SIDE_INFO_NONE, "where");
	RecordSeqBundleSink<T> sink("sink", SIDE_INFO_NONE);

	connect_transform(unbound, where);
	connect_transform(where, sink);

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
	testWhereSeq();
}

