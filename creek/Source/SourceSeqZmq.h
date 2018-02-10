//
// Created by xzl on 8/30/17.
//
// Source specializing for producing SeqBuf

#ifndef CREEK_UNBOUNDEDSEQZMQ_H
#define CREEK_UNBOUNDEDSEQZMQ_H

#include <zmq.hpp> /* for zmq */

#include "pipeline-config.h"
extern "C" {
#include "xplane_lib.h"
};

#include "csv.h" /* ben-strasser/fast-cpp-csv-parser */
//#include "Unbounded.h"
#include "core/Transforms.h"
#include "SeqBundle.h"
#include "BundleContainer.h"

template <typename T>
class SourceSeqZmqEval;

/* Producing SeqBuf of Records of kvpairs/tuples.
 * Output bundle format; RecordSeqBundle
 *
 * We don't use UnboundedInMem for Join, which is cluttered
 *
 * input file format:
 * a text file.
 *
 * line 1: total # of ints in the file
 * line 2+: each line has two ints. key and value.
 *
 * see test/digit-gen.py
 *
 * cf: UnboundedInMem.h
 *
 * @T: element type, e.g. kvpair
 */

template<typename T>
class SourceSeqZmq : public PTransform {
	using OutputBundleT = RecordSeqBundle<T>;

public:
	int interval_ms;  /* the ev time difference between consecutive bundles */

	char input_fnames[100];
	char * input_txt = nullptr, * input_socket = nullptr;
	const int punc_interval_ms = 1000;
	const unsigned long records_per_interval; // # records between two puncs
	int record_len = 0; /* the length covered by each record */
	const int session_gap_ms; // for tesging session windows
	const uint64_t target_tput; // in record/sec
	const unsigned int num_source_tasks;

	/*Raw data buffers, one for each NUMA node. Loaded from file */
	vector<T *> raw_bufs;
	uint64_t raw_buf_size = 0;  /* raw buffer size in bytes */

//private:
//	unsigned long total_record = 0; /* total #records read from file to buffer */

public:
	/* # of (duplicated) output streams */
	const unsigned num_outputs;

	zmq::context_t zcontext;

	//array of prefilled buffers of record, one for each NUMA node
	vector<Record<T> *> record_buffers;
#ifdef USE_UARRAY
	vector<SeqBuf<Record<T>>> record_buffers_sec; /* mirror buf in tz */
#endif

	uint64_t buffer_size_records = 0;

	void ReadOneLine(FILE *fp, T* t);  /* to be specialized */

	unsigned long ParseCSVFile(const string & fname, T* p, unsigned long len);

	SourceSeqZmq (string name, const char *input_f,
									unsigned long rpi, uint64_t tt,
									uint64_t record_size, int session_gap_ms,
						 unsigned int n_source_tasks,
						 unsigned num_outputs,
						 int sideinfo = SIDE_INFO_NONE)
			: PTransform(name, sideinfo), /* input_fnames(input_fnames), */
				records_per_interval(rpi),
				record_len(record_size),
				session_gap_ms(session_gap_ms),
				target_tput(tt),
				num_source_tasks(n_source_tasks),
				num_outputs(num_outputs),
				zcontext(1 /* # of io threads */) {

		/* split input names */
		strncpy(this->input_fnames, input_f, sizeof(this->input_fnames));
		xzl_bug_on_msg(strnlen(this->input_fnames, sizeof(this->input_fnames)) >= sizeof(this->input_fnames),
									 "input fname too long");
		input_txt = strtok(input_fnames, " ");
		xzl_bug_on_msg(!input_txt, "-i {local_file} {remote_socket}");
		input_socket = strtok(NULL, " ");
		xzl_bug_on_msg(!input_socket, "-i {local_file} {remote_socket}");

		interval_ms = 50;
		const unsigned element_len = sizeof(T);

		int num_nodes = numa_max_node() + 1;

		/* used to be x4. we make it small because we want the global buffer to fit in xplane lib's CHUNK_SIZE_TO_SECURE which is 32MB */
		unsigned long expected_record_num = records_per_interval;

		xzl_bug_on_msg(record_len != sizeof(Record<T>), "bug: record length mismatch");
		raw_buf_size = element_len * expected_record_num;

		#if 0 /* read plain txt file */
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
					 records_per_epoch / 1000,
					 records_per_epoch * record_len / 1024 / 1024, target_tput / 1000,
					 record_len);

		// allocate and fill buffers: note data in buffers do not contain ts.
		// we load from the file once.
		for(int i = 0; i < num_nodes; i++){
			unsigned long j = 0;
			T kv;
			T *p = (T *)numa_alloc_onnode(buffer_size, i);
			assert(p);

			// seek back to beginning of file and skip line 1 (the # of kv)
			fseek(file, 0, SEEK_SET);
			skip_comment_lines(file);
			xzl_bug_on(fscanf(file, KTYPE_FMT "\n", &std::get<0>(kv)) < 0);  /* just a dummy read */

			skip_comment_lines(file);
			ReadOneLine(file, &kv);

			while(!feof(file)){
				if (j == record_num)
					break;
				p[j++] = kv;
				skip_comment_lines(file);
				ReadOneLine(file, &kv);
			}

			buffers.push_back(p);
		}

		fclose(file);
		#endif

		/* print source config info. do this before we actual load the file */

		char *rpath = realpath(input_txt, NULL);
		xzl_bug_on(!rpath);

		printf("---- source configuration ---- \n");
		printf("source file: %s (specified as: %s), socket: %s\n", rpath, input_txt, input_socket);
		free(rpath);
		printf("raw buffer (loaded from file, no ts): %.2f MB\t", (double)raw_buf_size/1024/1024);
		printf("expected # of Records: %ld \n", expected_record_num);
		printf("%10s %10s %10s %10s %10s %10s %10s\n",
					 "#nodes", "KRec/node", "MB", "KRec/epoch", "MB/epoc", "target:KRec/S", "RecSize");
		printf("%10d %10lu %10lu %10lu %10lu %10lu %10d\n",
					 num_nodes,
					 buffer_size_records / 1000, raw_buf_size / 1024 / 1024,
					 records_per_interval / 1000,
					 records_per_interval * record_len / 1024 / 1024, target_tput / 1000,
					 record_len);

		printf("%20s %20s %20s\n",
					 "#bundles/epoch", "bundle_sz(krec)", "#source_tasks");
		printf("%20lu %20lu %20lu\n",
					 config.bundles_per_epoch,
					 records_per_interval / config.bundles_per_epoch / 1000,
					 config.source_tasks
		);
		VV(" ---- punc internal is %d sec (ev time) --- ", this->punc_interval_ms / 1000);
		printf("---- source configuration ---- \n");


		#if 1 /* using CSV parser */
		// allocate and fill buffers: note data in buffers do not contain ts.
		// we load from the file once.
		xzl_bug_on_msg(num_nodes != 1, "unsupported");

		for(int i = 0; i < num_nodes; i++) {
			T kv;
			T *p = (T *)numa_alloc_onnode(raw_buf_size, i);
			assert(p);

			buffer_size_records = ParseCSVFile(input_txt, p, raw_buf_size / sizeof(T));

			if (buffer_size_records < expected_record_num) {
				EE("warning: repeat input data to fill source buffer"
				   "(buffer %lu KB. input %lu KB)",
					 raw_buf_size / 1024, buffer_size_records * sizeof(T) / 1024);
			}

			/* fill the remaining buffer by replicating data loaded from the file */
			unsigned long s = 0;
			while (buffer_size_records < expected_record_num) {
				p[buffer_size_records ++] = p[s++];
			}

			raw_bufs.push_back(p);
		}
		#endif

		//fill the buffers of *records*
		for (int i = 0; i < num_nodes; i++) {
			Record<T> *record_buffer =
					(Record<T> *) numa_alloc_onnode(sizeof(Record<T>) * buffer_size_records, i);
			xzl_assert(record_buffer);
			for (unsigned long j = 0; j < buffer_size_records; j++) {
				record_buffer[j].data = raw_bufs[i][j];
			}
			record_buffers.push_back(record_buffer);
#ifdef USE_UARRAY
			SeqBuf<Record<T>> s(record_buffer, buffer_size_records);
//			EE("check record buffer ...");
//			s.Dump();
//			EE("check end ");
			record_buffers_sec.push_back(s);
#endif

			printf("node %d: actual # of Records: %ld, %.2f MB\n", i, buffer_size_records,
						 1.0 * buffer_size_records * sizeof(Record<T>) / 1024 / 1024);
		}
	}

	// source, no-op. note that we shouldn't return the transform's wm
	virtual etime RefreshWatermark(etime wm) override {
		return wm;
	}

#if 0 /* cf: Transform's function */
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
#endif

	/* impl goes to .cpp to avoid circular inclusion */
//	void ExecEvaluator(int nodeid, EvaluationBundleContext *c, shared_ptr<BundleBase>) override;

	void ExecEvaluator(int nodeid, EvaluationBundleContext *c, shared_ptr<BundleBase> bundle_ptr) override {
			/* instantiate an evaluator */
			SourceSeqZmqEval<T> eval(nodeid);
			eval.evaluate(this, c, bundle_ptr);
	}

private:
	/* skip as many comment lines. return # of line skipped */
	int skip_comment_lines(FILE *file) {
		int count = 0;
		int ret;
		char linebuffer[256]; /* for comment line. this should be fine as long as loading file is single threaded */

		xzl_bug_on(!file);

		while (true) {
			ret = fscanf(file, "#%s\n", linebuffer);
			if (ret == EOF || ret == 0 /* mismatch */)
				return count;
			xzl_bug_on(ret != 1); /* must be a match */
			count ++;
		}
	}
};

template<>
void
SourceSeqZmq<creek::kvpair>::ReadOneLine(FILE *file, creek::kvpair *kv);

template<>
void
SourceSeqZmq<creek::k2v>::ReadOneLine(FILE *file, creek::k2v *kv);

template<>
unsigned long
SourceSeqZmq<creek::kvpair>::ParseCSVFile(const string & fname, creek::kvpair* p, unsigned long len);

template<>
unsigned long
SourceSeqZmq<creek::k2v>::ParseCSVFile(const string & fname, creek::k2v* kv, unsigned long len);

#endif //CREEK_UNBOUNDEDSEQZMQ_H
