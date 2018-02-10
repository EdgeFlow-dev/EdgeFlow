//
// Created by xzl on 8/25/17.
//

#include "config.h"

#include "core/Pipeline.h"

#include "Source/SourceSeqEval.h"
#include "Win/WinGBKSeqEval.h"
#include "WinSum/WinSumSeqEval.h"
#include "Sink/WinKeySeqBundleSinkEval.h"

#include "../test-common.h"

/* default config. can override on cmdline options */
#ifdef DEBUG
pipeline_config config = {
		.records_per_interval = 1000,
		.target_tput = (20 * 1000),
		.record_size = sizeof(Record<creek::k2v>), /* estimate buffer size */
		.input_file = "/ssd/debs14.txt",
};
#else
pipeline_config config = {
		.records_per_interval = SZ_1M,
		.target_tput = SZ_1M,
		.record_size = sizeof(Record<creek::k2v>), /* estimate buffer size */
		.input_file = "/ssd/k2v.txt",
};
#endif

#define KEYPOS 1
#define VPOS 2

/* source -> wingbk -> sink */
void testWinGBKSeq()
{
	SourceSeq<creek::k2v>
			unbound("unbounded-inmem",
							config.input_file.c_str(),
							config.records_per_interval , /* records per wm interval */
							config.target_tput, 						/* target tput (rec/sec) */
							config.record_size,
							0  													/* session_gap_ms */
	);

	// create a new pipeline
	Pipeline* p = Pipeline::create(NULL);

	PCollection *unbound_output = dynamic_cast<PCollection *>(p->apply1(&unbound));
	unbound_output->_name = "unbound_output";

	WinGBKSeq<creek::k2v, KEYPOS> gbk ("wingbk", creek::WINGBK_BYKEY, 1000 * 1000);
	WinKeySeqBundleSink<creek::k2v> sink("sink");

	connect_transform(unbound, gbk);
	connect_transform(gbk, sink);

	EvaluationBundleContext eval(1, config.cores);
	eval.runSimple(p);
}

/* source -> wingbk -> winsum (perkey) -> sink */
void testWinSumByKey()
{
	SourceSeq<creek::k2v>
			unbound("unbounded-inmem",
							config.input_file.c_str(),
							config.records_per_interval , /* records per wm interval */
							config.target_tput, 						/* target tput (rec/sec) */
							config.record_size,
							0  													/* session_gap_ms */
	);

	// create a new pipeline
	Pipeline* p = Pipeline::create(NULL);

	PCollection *unbound_output = dynamic_cast<PCollection *>(p->apply1(&unbound));
	unbound_output->_name = "unbound_output";

	WinGBKSeq<creek::k2v, KEYPOS> gbk ("wingbk", WINGBK_BYKEY, 1000 * 1000);
	WinSumSeq<creek::k2v, creek::k2v, KEYPOS/*keypos*/, VPOS/*vpos*/, 4/*parallelism*/>
			agg("sum", 1 /*slide */, AVG_BYKEY);
	WinKeySeqBundleSink<creek::k2v> sink("sink");

	connect_transform(unbound, gbk);
	connect_transform(gbk, agg);
	connect_transform(agg, sink);

	EvaluationBundleContext eval(1, config.cores);
	eval.runSimple(p);
}

/* source -> wingbk (winonly) -> winsum (all) -> sink
 *
 * the output of winsum(all) has UNDEFINED key
 */
void testWinSumAll()
{
	SourceSeq<creek::k2v>
			unbound("unbounded-inmem",
							config.input_file.c_str(),
							config.records_per_interval , /* records per wm interval */
							config.target_tput, 						/* target tput (rec/sec) */
							config.record_size,
							0  													/* session_gap_ms */
	);

	// create a new pipeline
	Pipeline* p = Pipeline::create(NULL);

	PCollection *unbound_output = dynamic_cast<PCollection *>(p->apply1(&unbound));
	unbound_output->_name = "unbound_output";

	WinGBKSeq<creek::k2v, KEYPOS> gbk ("wingbk", WINGBK_WINONLY, 1000 * 1000);
	WinSumSeq<creek::k2v, creek::k2v, KEYPOS/*keypos*/, VPOS/*vpos*/, 1/*parallelism*/>
			agg("sum", 1 /*slide */, SUM_ALL);
	WinKeySeqBundleSink<creek::k2v> sink("sink");

	connect_transform(unbound, gbk);
	connect_transform(gbk, agg);
	connect_transform(agg, sink);

	EvaluationBundleContext eval(1, config.cores);
	eval.runSimple(p);
}

/* source -> wingbk (winonly) -> winsum (avgall) -> sink */
void testWinSumAvgAll()
{
	SourceSeq<creek::k2v>
			unbound("unbounded-inmem",
							config.input_file.c_str(),
							config.records_per_interval , /* records per wm interval */
							config.target_tput, 						/* target tput (rec/sec) */
							config.record_size,
							0  													/* session_gap_ms */
	);

	// create a new pipeline
	Pipeline* p = Pipeline::create(NULL);

	PCollection *unbound_output = dynamic_cast<PCollection *>(p->apply1(&unbound));
	unbound_output->_name = "unbound_output";

	WinGBKSeq<creek::k2v, KEYPOS> gbk ("wingbk", WINGBK_WINONLY, 1000 * 1000);
	WinSumSeq<creek::k2v, creek::k2v, KEYPOS/*keypos*/, VPOS/*vpos*/, 1/*parallelism*/>
			agg("sum", 1 /*slide */, AVG_ALL);
	WinKeySeqBundleSink<creek::k2v> sink("sink");

	connect_transform(unbound, gbk);
	connect_transform(gbk, agg);
	connect_transform(agg, sink);

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
	testWinGBKSeq();
//	testWinSumByKey();
//	testWinSumAll();
//	testWinSumAvgAll();
}

