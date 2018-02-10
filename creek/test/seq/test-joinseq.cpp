//
// Created by xzl on 8/29/17.
//

#include "config.h"

#include "core/Pipeline.h"

#include "Source/SourceSeqZmqEval.h"
#include "Source/SourceZmqEval.h"
#include "Source/SourceSeqEval.h"

#include "Win/WinGBKSeqEval.h"
#include "WinSum/WinSumSeqEval.h"
#include "Join/JoinSeqEval.h"

#include "Sink/RecordSeqBundleSinkEval.h"
#include "Sink/WinKeySeqBundleSinkEval.h"

#include "../test-common.h"
#include "BundleContainer.h"

using T = creek::kvpair;
#define KEYPOS 0
#define VPOS 1
//#define INPUT_FILE "/ssd/kv.txt tcp://localhost:5563"
#define INPUT_FILE "/ssd/kv.txt tcp://128.46.76.161:5563"

//using T = creek::k2v;
//#define KEYPOS 1
//#define VPOS 2
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

#define N 1

/*
 *         +----> wingbk-----> sink1
 * source--+
 *         +----> wingbk-----> sink2
 */

static void test_2streams()
{
	SourceSeqZmq<T>
//	SourceSeq<T>
//	SourceZmq<T>
			unbound("unbounded-inmem",
							config.input_file.c_str(),
							config.records_per_epoch , /* records per wm interval */
							config.target_tput, 						/* target tput (rec/sec) */
							config.record_size,
							0,  													/* session_gap_ms */
							config.source_tasks,
							2 									/* num of output streams */
	);

	// create a new pipeline
	Pipeline* p = Pipeline::create(NULL);
	source_transform_1to2(unbound);

	WinGBKSeq<T, KEYPOS, N> gbk0("wingbk", SIDE_INFO_NONE, WINGBK_BYKEY, 1000 * 1000);
	WinGBKSeq<T, KEYPOS, N> gbk1("wingbk", SIDE_INFO_NONE, WINGBK_BYKEY, 1000 * 1000);

	WinKeySeqBundleSink<T> sink0("sink0", SIDE_INFO_NONE);
	WinKeySeqBundleSink<T> sink1("sink1", SIDE_INFO_NONE);
//	RecordSeqBundleSink<T> sink2("sink2", SIDE_INFO_NONE);


	connect_transform_1to2(unbound, gbk0, gbk1);
	connect_transform(gbk0, sink0);
	connect_transform(gbk1, sink1);

//	connect_transform_1to2(unbound, sink0, sink1);
//	connect_transform_1to2(unbound, sink0, sink2);

	EvaluationBundleContext eval(1, config.cores);
	eval.runSimple(p);
}

/*
 *         +----> wingbk-----+
 * source--+                 +-----> join (bykey) --> sink
 *         +----> wingbk-----+
 */

static void test_2streams_join()
{
	SourceSeqZmq<T>
//	SourceSeq<T>
//	SourceZmq<T>
			unbound("unbounded-inmem",
							config.input_file.c_str(),
							config.records_per_epoch , /* records per wm interval */
							config.target_tput, 						/* target tput (rec/sec) */
							config.record_size,
							0,  													/* session_gap_ms */
							config.source_tasks,
							2 									/* num of output streams */
	);

	// create a new pipeline
	Pipeline* p = Pipeline::create(NULL);

	source_transform_1to2(unbound);

	WinGBKSeq<T, KEYPOS, N> gbk0("wingbk", SIDE_INFO_L, WINGBK_BYKEY, 1000 * 1000);
//	gbk0.set_side_info(SIDE_INFO_L);

	WinGBKSeq<T, KEYPOS, N> gbk1("wingbk", SIDE_INFO_R, WINGBK_BYKEY, 1000 * 1000);
//	gbk1.set_side_info(SIDE_INFO_R);

	JoinSeq<T, KEYPOS, -1 /* vpos, don't care */, N> join("[join]", JOIN_BYKEY, 1000 * 1000);
//	join.set_side_info(SIDE_INFO_J);

	WinKeySeqBundleSink<T> sink("[sink]", SIDE_INFO_JD);
//	sink.set_side_info(SIDE_INFO_JD);

//		RecordBundleSink<pair<long, vector<long>>> sink1("[sink1]");
//		sink1.set_side_info(SIDE_INFO_JDD);

	connect_transform_1to2(unbound, gbk0, gbk1);
	connect_transform_2to1(gbk0, gbk1, join);
	connect_transform(join, sink);
//	  connect_transform(sink, sink1);

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

	test_2streams_join();
//	test_2streams();
}
