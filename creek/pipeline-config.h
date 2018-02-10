//
// Created by xzl on 12/1/17.
//

#ifndef CREEK_PIPELINE_CONFIG_H
#define CREEK_PIPELINE_CONFIG_H

#include <string>

void print_config(void);

using namespace std;
struct pipeline_config {
	/* determine the wm frequency -- should be used by local source only*/
	unsigned long records_per_epoch;
	unsigned long target_tput; /* krec/s */
	unsigned long record_size; /* e.g. string range etc. */
	std::string input_file;
	unsigned long cores;
	unsigned long bundles_per_epoch;
	unsigned long source_tasks;
};

void parse_options(int ac, char *av[], pipeline_config* config);

extern pipeline_config config; /* decl in each test pipeline */

//struct pool_desc;
//int get_pool_config(pipeline_config const & config, struct pool_desc **pools);

#endif //CREEK_PIPELINE_CONFIG_H
