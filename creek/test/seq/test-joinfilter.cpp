//
// Created by xzl on 8/29/17.
//

#include "config.h"

#include "core/Pipeline.h"

#include "Source/SourceZmqEval.h"
#include "Source/SourceSeqEval.h"
#include "Win/WinGBKSeqEval.h"
#include "WinSum/WinSumSeqEval.h"
#include "Join/JoinSeqEval.h"
#include "Sink/WinKeySeqBundleSinkEval.h"

#include "../test-common.h"

//using T = creek::kvpair;
//#define KEYPOS 0
//#define VPOS 1
//#define INPUT_FILE "/ssd/kv.txt"

using T = creek::k2v;
#define KEYPOS 1
#define VPOS 2
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

#define N 1

/*
 *         L: +----> wingbk(bykey)-> winsum (perkey) --+
 * source--+                                           +--> join (filter) -> sink
 *         R: +--> wingbk(winonly)-> winsum (sum_all)--+
 */
//#define KEYPOS 1
//#define VPOS 2

static void test_2streams_join_filter()
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
							2 									/* num of output streams */
	);
#else
	SourceSeq<T>
			unbound("unbounded-inmem",
							"/ssd/k2v.txt",
							config.records_per_epoch , /* records per wm interval */
							config.target_tput, 						/* target tput (rec/sec) */
							sizeof(Record<T>), /* config.record_size, */
							0,  					/* session_gap_ms */
							2 					/* num of output streams */
	);
#endif

	// create a new pipeline
	Pipeline* p = Pipeline::create(NULL);

	source_transform_1to2(unbound);

	/* left -- per key */
	WinGBKSeq<T, KEYPOS, N> gbk0("wingbk/perkey", WINGBK_BYKEY, 1000 * 1000);
	WinSumSeq<T, T, KEYPOS, VPOS, N>
			agg0("winsum/avg-perkey", 1 /*slide */, AVG_BYKEY);
	agg0.set_side_info(SIDE_INFO_L);

	/* right -- threshold */
	WinGBKSeq<T, KEYPOS, N> gbk1("wingbk/winonly", WINGBK_WINONLY, 1000 * 1000);
	WinSumSeq<T, T, KEYPOS, VPOS, N>
			agg1("winsum/avg-all", 1 /*slide */, AVG_ALL);
	agg1.set_side_info(SIDE_INFO_R);

	JoinSeq<T, KEYPOS, VPOS> join("join/filter", JOIN_BYFILTER_RIGHT, 1000 * 1000);
	join.set_side_info(SIDE_INFO_J);

	WinKeySeqBundleSink<T> sink("[sink]");
	sink.set_side_info(SIDE_INFO_JD);

	connect_transform_1to2(unbound, gbk0, gbk1);
	/* left */
	connect_transform(gbk0, agg0);
	/* right */
	connect_transform(gbk1, agg1);

	connect_transform_2to1(agg0, agg1, join);
	connect_transform(join, sink);

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
	test_2streams_join_filter();
}
