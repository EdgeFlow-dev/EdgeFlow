//
// Created by xzl on 8/29/17.
//

#define K2_NO_MEASUREMENT 1

#include "config.h"

#include "core/Pipeline.h"

#include "Source/SourceSeqZmqEval.h"
#include "Source/SourceZmqEval.h"
#include "Source/SourceSeqEval.h"

#include "Win/WinGBKSeqEval.h"
#include "Win/GBKSeqEval.h"
#include "WinSum/WinSumSeqEval.h"
#include "Join/JoinSeqEval.h"
#include "Sink/WinKeySeqBundleSinkEval.h"

#include "../test-common.h"

using T = creek::k2v;
#define HOUSEID 0
#define PLUGID 1
#define VPOS 2
#define INPUT_FILE "/ssd/debs14.txt"

/* default config. can override on cmdline options */
#ifdef DEBUG
pipeline_config config = {
		.records_per_epoch = 1000 * 1000,
		.target_tput = 1000,
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
 *         L: +----> wingbk(by plug)-> winsum (med perkey) --+
 * source--+                                                 +--> join (filter) -> sink
 *         R: +--> wingbk(winonly)-> winsum (med all)      --+
 */
__attribute__((__unused__))
static void test_join_filter()
{
	SourceSeq<T>
			unbound("unbounded-inmem",
							"/ssd/k2v.txt",
							config.records_per_epoch , /* records per wm interval */
							config.target_tput, 						/* target tput (rec/sec) */
							sizeof(Record<T>), /* config.record_size, */
							0,  					/* session_gap_ms */
							config.source_tasks,
							2 					/* num of output streams */
	);

	// create a new pipeline
	Pipeline* p = Pipeline::create(NULL);

	source_transform_1to2(unbound);

	/* left -- per plug med */
	WinGBKSeq<T, PLUGID, N>
			gbk0("wingbk/perkey", SIDE_INFO_NONE, WINGBK_BYKEY, 1000 * 1000);
	WinSumSeq<T, T, PLUGID, VPOS, N>
			agg0("winsum/avg-perkey", SIDE_INFO_L, 1 /*slide */, AVG_BYKEY);
//	agg0.set_side_info(SIDE_INFO_L);

	/* right -- threshold */
	WinGBKSeq<T, PLUGID, N> gbk1("wingbk/winonly", SIDE_INFO_NONE, WINGBK_WINONLY, 1000 * 1000);
	WinSumSeq<T, T, PLUGID, VPOS, N>
			agg1("winsum/avg-all", SIDE_INFO_R, 1 /*slide */, AVG_ALL);
//	agg1.set_side_info(SIDE_INFO_R);

	JoinSeq<T, PLUGID, VPOS, 1> join("join/filter", JOIN_BYFILTER_RIGHT, 1000 * 1000);
//	join.set_side_info(SIDE_INFO_J);

	WinKeySeqBundleSink<T> sink("[sink]", SIDE_INFO_JD);
//	sink.set_side_info(SIDE_INFO_JD);

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

/*
 *         L: +----> wingbk(by plug)-> winsum (med perkey) --+
 * source--+                                                 +--> join (filter) -> gbk (by houseid) -> sumcount -> sink
 *         R: +--> wingbk(winonly)-> winsum (med all)      --+
 */
/* an adv version */
__attribute__((__unused__))
static void test_join_filter_sum()
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

	/* left -- per plug med */
	WinGBKSeq<T, PLUGID, N> gbk_plug("wingbk/plug", SIDE_INFO_NONE, WINGBK_BYKEY, 1000 * 1000);
	WinSumSeq<T, T, PLUGID, VPOS, N> avg_plug("winsum/avg-perplug", SIDE_INFO_L, 1 /*slide */, AVG_BYKEY);
//	avg_plug.set_side_info(SIDE_INFO_L);

	/* right -- threshold */
	WinGBKSeq<T, PLUGID, N> winonly("wingbk/winonly", SIDE_INFO_NONE, WINGBK_WINONLY, 1000 * 1000);
	WinSumSeq<T, T, PLUGID, VPOS, N> avg_all("winsum/avg-all", SIDE_INFO_R, 1 /*slide */, AVG_ALL);
//	avg_all.set_side_info(SIDE_INFO_R);

	JoinSeq<T, PLUGID, VPOS, 1> join("join/filter", JOIN_BYFILTER_RIGHT, 1000 * 1000);
//	join.set_side_info(SIDE_INFO_J);

	GBKSeq<T, N, HOUSEID> gbk_house("gbk/house", SIDE_INFO_JD);
//	gbk_house.set_side_info(SIDE_INFO_JD);

	/* overkill now. todo: count bykey only */
	WinSumSeq<T, T, HOUSEID, VPOS, N> cnt_house("winsum/house", SIDE_INFO_JDD, 1/*slide*/, AVG_BYKEY);
//	cnt_house.set_side_info(SIDE_INFO_JDD);

	WinKeySeqBundleSink<T> sink("[sink]", SIDE_INFO_JDD);
//	sink.set_side_info(SIDE_INFO_JDD);

	connect_transform_1to2(unbound, gbk_plug, winonly);
	/* left */
	connect_transform(gbk_plug, avg_plug);
	/* right */
	connect_transform(winonly, avg_all);

	connect_transform_2to1(avg_plug, avg_all, join);
	connect_transform(join, gbk_house);
	connect_transform(gbk_house, cnt_house);
	connect_transform(cnt_house, sink);

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
//	test_join_filter();
	test_join_filter_sum();
}
