/*
 * test-common.cpp
 *
 *  Created on: Oct 7, 2016
 *      Author: xzl
 */

#include <array>
#include <stdio.h>
#include "log.h"
#include "config.h"
#include "version.h"

void print_config()
{
	printf(">>>> git commit: %s br: %s\n", GIT_COMMIT_HASH, GIT_BRANCH);
	printf("config:\n");

#ifdef DEBUG
	xzl_show_define(DEBUG);
#else
	xzl_show_undefine(DEBUG);
#endif

#ifdef USE_NUMA_TP
	xzl_show_define(USE_NUMA_TP);
#else
	xzl_show_undefine(USE_NUMA_TP);
#endif

#ifdef USE_TBB_DS
	xzl_show_define(USE_TBB_DS)
#else
	xzl_show_undefine(USE_TBB_DS)
#endif

#ifdef USE_FOLLY_VECTOR
	xzl_show_define(USE_FOLLY_VECTOR)
#else
	xzl_show_undefine(USE_FOLLY_VECTOR)
#endif

#ifdef USE_FOLLY_STRING
	xzl_show_define(USE_FOLLY_STRING)
#else
	xzl_show_undefine(USE_FOLLY_STRING)
#endif

#ifdef USE_FOLLY_HASHMAP
	xzl_show_define(USE_FOLLY_HASHMAP)
#else
	xzl_show_undefine(USE_FOLLY_HASHMAP)
#endif

#ifdef USE_CUCKOO_HASHMAP
	xzl_show_define(USE_CUCKOO_HASHMAP)
#else
	xzl_show_undefine(USE_CUCKOO_HASHMAP)
#endif

#ifdef USE_TBB_HASHMAP
	xzl_show_define(USE_TBB_HASHMAP)
#else
	xzl_show_undefine(USE_TBB_HASHMAP)
#endif

#ifdef USE_NUMA_ALLOC
	xzl_show_define(USE_NUMA_ALLOC)
#else
	xzl_show_undefine(USE_NUMA_ALLOC)
#endif

#ifdef INPUT_ALWAYS_ON_NODE0
	xzl_show_define(INPUT_ALWAYS_ON_NODE0)
#else
	xzl_show_undefine(INPUT_ALWAYS_ON_NODE0)
#endif

#ifdef MEASURE_LATENCY
	xzl_show_define(MEASURE_LATENCY)
#else
	xzl_show_undefine(MEASURE_LATENCY)
#endif

#ifdef CONFIG_SOURCE_THREADS
	xzl_show_define(CONFIG_SOURCE_THREADS)
#else
//	xzl_show_undefine(CONFIG_SOURCE_THREADS)
#error
#endif

#ifdef CONFIG_MIN_PERNODE_BUFFER_SIZE_MB
	xzl_show_define(CONFIG_MIN_PERNODE_BUFFER_SIZE_MB)
#else
//	xzl_show_undefine(CONFIG_MIN_PERNODE_BUFFER_SIZE_MB)
#error
#endif

#ifdef CONFIG_MIN_EPOCHS_PERNODE_BUFFER
	xzl_show_define(CONFIG_MIN_EPOCHS_PERNODE_BUFFER)
#else
//	xzl_show_undefine(X)
#error
#endif

#ifdef CONFIG_NETMON_HT_PARTITIONS
	xzl_show_define(CONFIG_NETMON_HT_PARTITIONS)
#else
#error
#endif

#ifdef CONFIG_JOIN_HT_PARTITIONS
	xzl_show_define(CONFIG_JOIN_HT_PARTITIONS)
#else
#error
#endif

	/* warn about workarounds */
#ifdef WORKAROUND_JOIN_JDD
	EE("warning: WORKAROUND_JOIN_JDD = 1. Sure? (any key to continue)\n");
	getchar();
#endif

#ifdef WORKAROUND_WINKEYREDUCER_RECORDBUNDLE
	EE("warning: WORKAROUND_WINKEYREDUCER_RECORDBUNDLE = 1. Sure? (any key to continue)\n");
	getchar();
#endif

#ifdef USE_UARRAY
		xzl_show_define(USE_UARRAY)
	//	xzl_show_string("xplane_lib_arch", xplane_lib_arch);
#else
		xzl_show_undefine(USE_UARRAY)
#endif

	/* todo: add more here */
}

// Copyright Vladimir Prus 2002-2004.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

/* The simplest usage of the library.
 *
 * http://www.boost.org/doc/libs/1_61_0/libs/program_options/example/first.cpp
 * http://www.boost.org/doc/libs/1_61_0/libs/program_options/example/options_description.cpp
 */
//#undef _GLIBCXX_DEBUG /* does not get along with program options lib */

#include <iostream>
#if 0
#include <boost/program_options.hpp>
namespace po = boost::program_options;
#endif

using namespace std;
#include <thread>
#include "test-common.h"

#if 0 /* obsoleted. boost::options not flexible enough and require linking
 * that lib, which causes GXX_LIBC_DEBUG failure */
void parse_options(int ac, char *av[], pipeline_config* config)
{
  po::variables_map vm;
  xzl_assert(config);

  try {

      po::options_description desc("Allowed options");
      desc.add_options()
          ("help", "produce help message")
          ("records", po::value<unsigned long>(), "records per wm interval")
          ("target_tput", po::value<unsigned long>(), "target throughput (krec/s)")
          ("record_size", po::value<unsigned long>(), "record (string_range) size (bytes)")
          ("cores", po::value<unsigned long>(), "# cores for worker threads")
          ("input_file", po::value<vector<string>>(), "input file path") /* must be vector */
      ;

      po::store(po::parse_command_line(ac, av, desc), vm);
      po::notify(vm);

      if (vm.count("help")) {
          cout << desc << "\n";
          exit(1);
      }

      if (vm.count("records")) {
          config->records_per_epoch = vm["records"].as<unsigned long>();
      }
      if (vm.count("target_tput")) {
          config->target_tput = vm["target_tput"].as<unsigned long>() * 1000;
      }
      if (vm.count("record_size")) {
          config->record_size = vm["record_size"].as<unsigned long>();
      }

      if (vm.count("cores")) {
          config->cores = vm["cores"].as<unsigned long>();
      } else { /* default value for # cores (save one for source) */
      	config->cores = std::thread::hardware_concurrency() - 1;
      }

      if (vm.count("input_file")) {
					config->input_file = vm["input_file"].as<vector<string>>()[0];
			}
  }
  catch(exception& e) {
      cerr << "error: " << e.what() << "\n";
      abort();
  }
  catch(...) {
      cerr << "Exception of unknown type!\n";
      abort();
  }
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
//			.name = "help",
				.name = nullptr,
				.key = 'h',
				.arg = nullptr,
				.flags = 0,
				.doc = "produce help message"
		},
		{
				.name = "records",
				.key = 'r',
				.arg = "RECORDS",
				.flags = 0,
				.doc = "records per wm interval"
		},
		{
				.name = "bundles",
				.key = 'b',
				.arg = "BUNDLES",
				.flags = 0,
				.doc = "bundles per wm interval"
		},
		{	/* backward compatibility. can't coexist with -T */
				.name = "target_tput",
				.key = 't',
				.arg = "K RECORDS",
				.flags = 0,
				.doc = "target throughput (krec/s) -- obsoleted"
		},
		{
				.name = "Target_tput",
				.key = 'T',
				.arg = "RECORDS",
				.flags = 0,
				.doc = "target throughput (rec/s)"
		},
		{
				.name = "record_size",
				.key = 'z',
				.arg = "BYTES",
				.flags = 0,
				.doc = "record size (bytes)"
		},
		{
				.name = "cores",
				.key = 'c',
				.arg = "NCORES",
				.flags = 0,
				.doc = "# cores for worker threads"
		},
		{
				.name = "source_tasks",
				.key = 's',
				.arg = "NTASKS",
				.flags = 0,
				.doc = "# of source tasks"
		},
		{
				.name = "input_file",
				.key = 'i',
				.arg = "FILE",
				.flags = 0,
				.doc = "input file or sender path (alias supported)"
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

static std::array<std::pair<std::string, std::string>, 4> aliases =
		{{
				 make_pair("k2", "tcp://k2.ecn.purdue.edu:5563"),
				 make_pair("localhost", "tcp://localhost:5563"),
				 make_pair("makalu", "tcp://makalu.ecn.purdue.edu:5563"),
				 make_pair("p", "tcp://128.46.76.161:5563"),
				 /* XXX add more */
		 }};

/* replace @input with the full addr. if not found, return @input */
static std::string expand_alias(std::string const input) {
	for (auto & i : aliases) {
		if (i.first == input)
			return i.second;
	}
	return input;
}

/* Parse a single option. Callback for parser */
static error_t parse_callback(int key, char *arg, struct argp_state *state)
{
	/* Get the input argument from argp_parse, which we
		 know is a pointer to our arguments structure. */
	pipeline_config* config = (pipeline_config *)state->input;

	switch (key)
	{
		case 'h':
			argp_help (&myargp, stderr, ARGP_HELP_LONG | ARGP_HELP_DOC, nullptr /*name*/);
			exit(1);
			break;
		case 't':
			xzl_bug_on_msg(config->target_tput != 0, "t and T are exclusive");
			config->target_tput = 1000 * xzl_ulong_postfix(arg);
			break;
		case 'T':
			xzl_bug_on_msg(config->target_tput != 0, "t and T are exclusive");
			config->target_tput = xzl_ulong_postfix(arg);
			break;
		case 'z':
			config->record_size = xzl_ulong_postfix(arg);
			break;
		case 'c':
			config->cores = xzl_ulong_postfix(arg);
			break;
		case 's':
			config->source_tasks = xzl_ulong_postfix(arg);
			break;
		case 'b':
			config->bundles_per_epoch = xzl_ulong_postfix(arg);
			break;
		case 'r':
			config->records_per_epoch = xzl_ulong_postfix(arg);
			break;
		case 'i':
			config->input_file = expand_alias(arg);
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

void parse_options(int ac, char *av[], pipeline_config* config)
{
	/* default values */
	config->cores = std::thread::hardware_concurrency() - 1;
	config->bundles_per_epoch = 4 * config->cores;
	config->records_per_epoch =
			config->bundles_per_epoch * CONFIG_DEFAULT_BUNDLE_SIZE;
	config->source_tasks = 0; /* unconfigured */
	config->target_tput = 0; /* unconfigured */

	/* Parse our arguments; every option seen by parse_opt will
		 be reflected in arguments. */
	argp_parse (&myargp, ac, av, 0, 0, config);

	if (config->source_tasks == 0)
		config->source_tasks = config->cores;
}

#ifdef USE_TZ
#include <signal.h>
extern "C"{
#include "xplane_lib.h"
};

static int is_xplane_init = 0; /* XXX race condition? */

__attribute__((__unused__))
static void abort_handler(int i)
{
	if (is_xplane_init) {
		EE("cleaning up....");
		kill_map();
		EE("mapped killed");
		close_context();
		EE("context closed");
		is_xplane_init = 0;
	}

	exit(1);
}

/* xzl: we don't want to call this as ctor. e.g. for --help we dont need this */
//__attribute__((constructor))
void xplane_init(void)
{
#if 1
	if (signal(SIGABRT, abort_handler) == SIG_ERR) {
		fprintf(stderr, "Couldn't set signal handler\n");
		exit(1);
	}

	if (signal(SIGINT, abort_handler) == SIG_ERR) {
		fprintf(stderr, "Couldn't set signal handler\n");
		exit(1);
	}
	EE("sigabrt & sigint will be handled");
#else
	EE("sigabrt & sigint won't be handled");
#endif

	open_context();
	populate_map();

	is_xplane_init = 1;
}

/* will be called when main() ends or exit() called */
__attribute__((destructor))
void xplane_fini(void)
{
	if (is_xplane_init) {
		EE("cleaning up....");
		kill_map();
		EE("mapped killed");
		close_context();
		EE("context closed");
		is_xplane_init = 0;
	}
}

#endif
