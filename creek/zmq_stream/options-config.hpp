#ifndef OPTIONS_CONFIG_H
#define OPTIONS_CONFIG_H

#include <boost/program_options.hpp>

namespace {
	const size_t ERROR_IN_COMMAND_LINE = 1;
	const size_t SUCCESS = 0;
	const size_t ERROR_UNHANDLED_EXCEPTION = 2;
}

struct options_config {
	std::string addr;
	std::string input_file;
	uint8_t rec_type;
	uint64_t recs_per_bundle;
	uint64_t ts_start;
	uint64_t ts_delta;
	uint64_t interval;
};

int print_config(options_config config) {
	printf(_k2clr_red "%20s : %s\n", "[net addr]", config.addr.c_str());
	printf("%20s : %s\n", "[input file]", config.input_file.c_str());
	printf("%20s : %u\n", "[record type]", config.rec_type);
	printf("%20s : %lu\n", "[records per bundle]", config.recs_per_bundle);
	printf("%20s : %lu\n", "[timestamp start]", config.ts_start);
	printf("%20s : %lu\n", "[timestamp delta]", config.ts_delta);
	printf("%20s : %lu\n" _k2clr_none, "[bundle interval]", config.interval);
	return SUCCESS;
}

#if 0 /* obsoleted */
int parse_options_boost(int argc, const char *argv[], options_config* config) {
	try {
		namespace po = boost::program_options;
		po::options_description desc("Options");
		desc.add_options()
			("help,h", "print help message")
			("addr,a", po::value<std::string>(), "set network address")
			("input_file,f", po::value<std::string>(), "set input file")
			("rec_type,t", po::value<uint64_t>(), "recrod type 3 (x3), 4 (x4)")
			("recs_per_bundle,r", po::value<uint64_t>(), "recrods per bundle")
			("ts_start,s", po::value<uint64_t>(), "start timestamp")
			("ts_delta,d", po::value<uint64_t>(), "timestamp delta")
			("interval,i", po::value<uint64_t>(), "bundle interval (us)");

		po::variables_map vm;
		try {
			po::store(po::parse_command_line(argc, argv, desc),
					vm); // can throw

			/* --help option */
			if (vm.count("help"))
			{
				std::cout << "Basic Command Line Parameter App" << std::endl
					<< desc << std::endl;
				exit(1);
			}

			if (vm.count("addr")) {
				config->addr = vm["addr"].as<std::string>();
			}

			if (vm.count("input_file")) {
				config->input_file = vm["input_file"].as<std::string>();
			}

			if (vm.count("rec_type")) {
				config->rec_type = vm["rec_type"].as<size_t>();
			}

			if (vm.count("recs_per_bundle")) {
				config->recs_per_bundle = vm["recs_per_bundle"].as<uint64_t>();
			}

			if (vm.count("ts_start")) {
				config->ts_start = vm["ts_start"].as<uint64_t>();
			}

			if (vm.count("ts_delta")) {
				config->ts_delta = vm["ts_delta"].as<uint64_t>();
			}

			if (vm.count("interval")) {
				config->interval = vm["interval"].as<uint64_t>();
			}

			po::notify(vm); // throws on error, so do after help in case
			// there are any problems
		} catch(po::error& e) {
			std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
			std::cerr << desc << std::endl;
			return ERROR_IN_COMMAND_LINE;
		}

		// application code here //

	}
	catch(std::exception& e)
	{
		std::cerr << "Unhandled Exception reached the top of main: "
			<< e.what() << ", application will now exit" << std::endl;
		return ERROR_UNHANDLED_EXCEPTION;

	}
	return SUCCESS;
}
#endif

#include <argp.h>

const char *argp_program_version =
		"argp-ex3 1.0";
const char *argp_program_bug_address =
		"<bug-gnu-utils@gnu.org>";
/* Program documentation. */
static char doc[] =
		"Note: postfix such K/M/G is supported";
/* A description of the arguments we accept. */
static char args_doc[] = "ARG1 ARG2 ...";

/* The options we understand. */
static struct argp_option const my_opts[] = {
		{
				.name = nullptr,
				.key = 'h',
				.arg = nullptr,
				.flags = 0,
				.doc = "produce help message"
		},
		{
				.name = "addr",
				.key = 'a',
				.arg = "tcp://IP:port",
				.flags = 0,
				.doc = "set address"
		},
		{
				.name = "input_file",
				.key = 'f',
				.arg = "FILE",
				.flags = 0,
				.doc = "input file pathr"
		},
		{
				.name = "rec_type",
				.key = 't',
				.arg = "RECTYPE",
				.flags = 0,
				.doc = "record type 3 (x3) or 4 (x4)"
		},
		{
				.name = "recs_per_bundle",
				.key = 'r',
				.arg = "RECNUM",
				.flags = 0,
				.doc = "records per bundle"
		},
		{
				.name = "ts_start",
				.key = 's',
				.arg = "TIME",
				.flags = 0,
				.doc = "start timestamp"
		},
		{
				.name = "ts_delta",
				.key = 'd',
				.arg = "TIME",
				.flags = 0,
				.doc = "timestamp delta"
		},
		{
				.name = "interval",
				.key = 'i',
				.arg = "TIME (us)",
				.flags = 0,
				.doc = "bundle interval (us)"
		},
		{ 0 }
};

static error_t parse_callback(int key, char *arg, struct argp_state *state);

struct argp myargp {
		.options = my_opts,
		.parser = parse_callback,
		.args_doc = args_doc,
		.doc = doc
};

/* Parse a single option. Callback for parser */
static error_t parse_callback(int key, char *arg, struct argp_state *state)
{
	options_config *config = (options_config *) state->input;

	switch (key) {
		case 'h':
			argp_help (&myargp, stderr, ARGP_HELP_LONG | ARGP_HELP_DOC, nullptr /*name*/);
			exit(1);
			break;
		case 'a':
			config->addr = arg;
			break;
		case 'f':
			config->input_file = arg;
			break;
		case 't':
			xzl_bug_on(atol(arg) > UINT8_MAX);
			config->rec_type = (uint8_t)atol(arg);
			break;
		case 'r':
			config->recs_per_bundle = xzl_ulong_postfix(arg);
			break;
		case 's':
			config->ts_start = xzl_ulong_postfix(arg);
			break;
		case 'd':
			config->ts_delta = xzl_ulong_postfix(arg);
			break;
		case 'i':
			config->interval = xzl_ulong_postfix(arg);
		break;
		case ARGP_KEY_ARG:
			/* Too many arguments. */
			argp_usage (state);
			exit(1);
			break;
		case ARGP_KEY_END:
			break;
		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

namespace zmq_stream {
	void parse_options(int ac, char *av[], options_config *config) {
		/* TODO: default values */

		/* Parse our arguments; every option seen by parse_opt will
			 be reflected in arguments. */
		argp_parse(&myargp, ac, av, 0, 0, config);

		/* TODO: post config */
	}
}
#endif
