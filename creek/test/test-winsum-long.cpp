//
// Created by xzl on 7/11/17.
//
/* a simple pipeline for windowed aggregation, which is easily reproducible for comparison */

#include "config.h"
#include "core/Pipeline.h"

#include "Source/UnboundedInMemEvaluator.h"
#include "Win/FixedWindowIntoEvaluator.h"
#include "WinSum/WinSum_addlong.h"
#include "Sink/WindowsBundleSinkEvaluator.h"

#include "Sink/RecordBundleSinkEvaluator.h"

#include "test-common.h"


/*
 * Unbounded (unsigned long)
 *    |
 *    V
 * FixedWindowInto
 *    |
 *    V
 * WinSum_addlong
 *    |
 *    V
 * RecordBundleSink
 *
 */

#ifdef DEBUG
pipeline_config config = {
		.records_per_interval = 100,
		.target_tput = (20 * 100),
		.record_size = sizeof(long),
		.input_file = "/ssd/test-digit.txt",
};
#else
pipeline_config config = {
		.records_per_interval = 1000 * 1000,
		//.target_tput = (10 * 1000 * 1000),
		.target_tput =  2000 * 1000,   //k2: 2000 should be okay
		.record_size = sizeof(long),
		.input_file = "/ssd/test-digit.txt",
};
#endif

/* -- define the bundle type we use -- */
template<class T>
using BundleT = RecordBundle<T>;
//using BundleT = RecordBitmapBundle<T>;

int main(int ac, char *av[]) {
	parse_options(ac, av, &config);
	print_config();
	// create a new Bounded transform
	//UnboundedInMem<long, BundleT> unbound ("[unbounded]", 50, 0);
	UnboundedInMem<long, BundleT> unbound("[unbounded]",
																				config.input_file.c_str(),
																				config.records_per_interval, /* records per wm interval */
																				config.target_tput,        /* target tput (rec/sec) */
																				config.record_size,
																				0 //XXX session_gap_ms = ????
	);

	// create a new pipeline
	Pipeline* p = Pipeline::create(NULL);

	PCollection *unbound_output = dynamic_cast<PCollection *>(p->apply1(&unbound));
	unbound_output->_name = "unbound_output";

	FixedWindowInto<long, BundleT> fwi ("window", 1000 * 1000 /* seconds(1) */);
	WinSum_addlong<long, long> agg ("agg", 1); /* fixed window */
	RecordBundleSink<long> sink("sink");

	connect_transform(unbound, fwi);
	connect_transform(fwi, agg);
	connect_transform(agg, sink);

	EvaluationBundleContext eval(1, config.cores);
	eval.runSimple(p);

}