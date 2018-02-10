//
// Created by xzl on 8/30/17.
//
// Source specializing for producing SeqBuf

#ifndef CREEK_UNBOUNDEDSEQ_H
#define CREEK_UNBOUNDEDSEQ_H

#include "Unbounded.h"
#include "SeqBundle.h"

/* Producing SeqBuf of Records of kvpairs.
 * Only produces RecordSeqBundle
 * We don't use UnboundedInMem for Join, which is cluttered
 *
 * input file format:
 *
 * a text file.
 *
 * line 1: total # of ints in the file
 * line 2+: each line has two ints. key and value.
 *
 * see test/digit-gen.py
 */

/* NB: this is a fully specialized class. no template */
template<>
class UnboundedInMem<creek::kvpair, RecordSeqBundle> : public PTransform {
	using OutputBundleT = RecordSeqBundle<creek::kvpair>;

public:
	int interval_ms;  /* the ev time difference between consecutive bundles */

	const char * input_fname;
	const int punc_interval_ms = 1000;
	const unsigned long records_per_interval; // # records between two puncs
	int record_len = 0; /* the length covered by each record */
	const int session_gap_ms; // for tesging session windows
	const uint64_t target_tput; // in record/sec

	/*Raw data buffers, one for each NUMA node. Loaded from file */
	vector<creek::kvpair *> buffers;
	uint64_t buffer_size = 0;
	unsigned long record_num = 0;

	/* # of (duplicated) output streams */
	const int num_outputs;

	//array of prefilled buffers of record, one for each NUMA node
	//vector<Record<T> *> record_buffers;
	vector<Record<creek::kvpair> *> record_buffers;
	uint64_t buffer_size_records = 0;

	UnboundedInMem (string name, const char *input_fname,
									unsigned long rpi, uint64_t tt,
									uint64_t record_size, int session_gap_ms,
									int num_outputs = 1)
			: PTransform(name), input_fname(input_fname),
				records_per_interval(rpi),
				record_len(record_size), //= sizeof(long)
				session_gap_ms(session_gap_ms),
				target_tput(tt),
				num_outputs(num_outputs),
				byte_counter_(0),
				record_counter_(0) {

		interval_ms = 50;
		struct stat finfo;

		int num_nodes = numa_max_node() + 1;
//		int num_nodes = 1;
//		std::cout << "WARNING: num_nodes is set to 1!!!!! Reset it in Unbounded.h and UnboundedInMemEvaluator.h!!!" << std::endl;

		xzl_bug_on(record_len != sizeof(creek::kvpair));
		buffer_size = records_per_interval * record_len * 2 * 2;

		FILE * file = fopen(input_fname, "r");
		cout << "open file "  << input_fname << endl;
		xzl_bug_on_msg(!file, "open file failed");

		/* 1st line of the input file is # of records */
		int ret = fscanf(file, "%ld\n", &record_num);
		xzl_bug_on_msg(ret != 1, "bug: file format err");

		/* sanity check: file long enough for the buffer? */
		if (buffer_size > record_num * record_len) {
			EE("input data not enough. need %lu KB. has %lu KB",
				 buffer_size / 1024, record_num * sizeof(long) / 1024);
			abort();
		}

		buffer_size_records = buffer_size / record_len;
		xzl_assert(buffer_size_records > 0);

		char *rpath = realpath(input_fname, NULL);
		xzl_bug_on(!rpath);

		/* the actual record num in the buffer */
		record_num = buffer_size / record_len;

		// print source config info
		printf("---- source configuration ---- \n");
		printf("source file: %s (specified as: %s)\n", rpath, input_fname);
		free(rpath);
		printf("source file size: %.2f MB\n", (double)finfo.st_size/1024/1024);
		printf("buffer size: %.2f MB\n", (double)buffer_size/1024/1024);
		printf("Number of Records: %ld \n", record_num);
		printf("%10s %10s %10s %10s %10s %10s %10s %10s\n", "#nodes:", "KRec/nodebuf", "MB",
					 "epoch/ms", "KRec/epoch", "MB/epoc", "target:KRec/S", "RecSize");
		printf("%10d %10lu %10lu %10d %10lu %10lu %10lu %10d\n", num_nodes,
					 buffer_size_records / 1000, buffer_size / 1024 / 1024, punc_interval_ms,
					 records_per_interval / 1000,
					 records_per_interval * record_len / 1024 / 1024, target_tput / 1000,
					 record_len);

		// allocate and fill buffers: note data in buffers do not contain ts.
		// we load from the file once.
		for(int i = 0; i < num_nodes; i++){
			unsigned long j = 0;
			creek::kvpair kv;
			kvpair *p = (kvpair *)numa_alloc_onnode(buffer_size, i);
			assert(p);
			// seek back to beginning of file and skip line 1 (the # of kv)
			fseek(file, 0, SEEK_SET);
			xzl_bug_on(fscanf(file, KTYPE_FMT "\n", &kv.first) < 0);

			int ret = fscanf(file, KTYPE_FMT VTYPE_FMT "\n", &kv.first, &kv.second);
			while(!feof(file)){
				if (j == record_num)
					break;
				p[j++] = kv;
				ret = fscanf(file, KTYPE_FMT VTYPE_FMT "\n", &kv.first, &kv.second);
				xzl_bug_on(ret < 0);
			}

			buffers.push_back(p);
		}

		//fill the buffers of records
		for(int i = 0; i < num_nodes; i++){
			Record<kvpair> * record_buffer =
					(Record<kvpair> *) numa_alloc_onnode(sizeof(Record<kvpair>) * record_num, i);
			assert(record_buffer);
			for(unsigned long j = 0; j < record_num; j++){
				//record_buffer[j].data.data = buffers[i][j];
				record_buffer[j].data = buffers[i][j];
				//std::cout << "record_buffer[j.data] is " << record_buffer[j].data << std::endl;
				//record_buffer[j].data.len = record_len; //sizeof(long)
				//record_buffer[j].ts will be filled by eval
			}
			record_buffers.push_back(record_buffer);
		}

		fclose(file);

	}//end UnboundedInMem init

	// source, no-op. note that we shouldn't return the transform's wm
	virtual etime RefreshWatermark(etime wm) override {
		return wm;
	}

	/* internal accounting  -- to be updated by the evaluator*/
	atomic<unsigned long> byte_counter_, record_counter_;
	bool ReportStatistics(PTransform::Statstics* stat) override {
		/* internal accounting */
		unsigned long total_records =
				record_counter_.load(std::memory_order_relaxed);
		unsigned long total_bytes =
				byte_counter_.load(std::memory_order_relaxed);

		/* last time we report */
		static unsigned long last_bytes = 0, last_records = 0;
		static boost::posix_time::ptime last_check, start_time;
		static int once = 1;

		auto now = boost::posix_time::microsec_clock::local_time();

		if (once) {
			once = 0;
			last_check = now;
			start_time = now;
			last_records = total_records;
			return false;
		}

		boost::posix_time::time_duration diff = now - last_check;

		{
			double interval_sec = (double) diff.total_milliseconds() / 1000;
			double total_sec = (double) (now - start_time).total_milliseconds() / 1000;

			stat->name = this->name.c_str();
			stat->mbps = (double) total_bytes / total_sec;
			stat->mrps = (double) total_records / total_sec;

			stat->lmbps = (double) (total_bytes - last_bytes) / interval_sec;
			stat->lmrps = (double) (total_records - last_records) / interval_sec;

			last_check = now;
			last_bytes = total_bytes;
			last_records = total_records;
		}

		return true;
	}

	void ExecEvaluator(int nodeid, EvaluationBundleContext *c, shared_ptr<BundleBase>) override;

};

#endif //CREEK_UNBOUNDEDSEQ_H
