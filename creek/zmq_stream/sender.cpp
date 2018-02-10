#include <iostream>
#include <unistd.h>

#include "log.h"
#include "FileSource.hpp"
#include "options-config.hpp"

using namespace std;

options_config config = {
	"tcp://*:5563",			// network address
	"/ssd/k2v-10M.txt",		// input_file
	4,						// record type
	5,						// records per bundle
	0,						// timestamp start
	10,						// timestamp delta
	0						// bundle interval (us)
};

int main (int argc, char *argv[]) {
	/* parse options */
	/* default is in options-config.hpp */
	zmq_stream::parse_options(argc, argv, &config);
	print_config(config);

	/* create global source */
	/* parse file and make bundle pool */
	if (config.rec_type == 3) {
		FileSource<zmq_source::x3> src(config.addr, config.input_file, config.rec_type,
							config.ts_start, config.ts_delta, config.recs_per_bundle, config.interval);
		src.MakeBundlePool();
		src.SendMessage();

	} else if (config.rec_type == 4) {
		FileSource<zmq_source::x4> src(config.addr, config.input_file, config.rec_type,
							config.ts_start, config.ts_delta, config.recs_per_bundle, config.interval);
		src.MakeBundlePool();
		src.SendMessage();
	} else {
		EE("do not support this type");
		exit(1);
	}

    return 0;
}
