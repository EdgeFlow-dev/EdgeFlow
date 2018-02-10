#ifndef FILE_SOURCE_H
#define FILE_SOURCE_H

#include <Source.hpp>

//template<typename T>
//class SourceBase;

template<typename T>
class FileSource : public SourceBase<T> {
private:
	const string fname;

public:
	FileSource(const string & addr, const string & fname, uint8_t rec_type,
			uint64_t ts_start, uint64_t ts_delta, uint64_t rpb, uint64_t interval)
		: SourceBase<T>(addr, rec_type, ts_start, ts_delta, rpb, interval), fname(fname) {
		uint64_t lines;

		I("Check lines of rec in the file");
		lines = LoadFile(this->fname.c_str());

		this->kv = (T *) malloc (sizeof(T) * lines);
		std::memset(this->kv, 0, sizeof(T) * lines);
	}

	~FileSource() {
		this->bundle_pool.clear();
		free(this->kv);
	}

	void MakeBundlePool(void) {
		ParseCSVFile(this->rpb);
	}
private:
	uint64_t ParseCSVFile(uint64_t nrecs);

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

	/* return # of lines */
	uint64_t LoadFile(const char *fname) {
		uint64_t cnt = 0;
		std::ifstream fin(fname);

		if(!fin.is_open()) {
			EE("file open error");
			abort();
		} else {
			std::string line;
			/* read one line and remove unnecessary thigns (e.g., white space) */
			while (std::getline(fin, line)) {
				/* skip comment */
				if (line.c_str()[0] == '#')
					continue;
				cnt++;
			}
			fin.close();
			I("Number of Records: %ld", cnt);
		}
		return cnt;
	}


};

template<>
uint64_t FileSource<zmq_source::x3>::ParseCSVFile(uint64_t nrecs) {
	using namespace io;

	unsigned long start = 0;
	unsigned long cnt = 0;

	CSVReader<2, trim_chars<' ', '\t'>, no_quote_escape<','>, throw_on_overflow, single_line_comment<'#'>> in(fname);

	try {
		while (in.read_row(std::get<0>(kv[cnt]), std::get<1>(kv[cnt]))) {
			cnt++;

			if (cnt % nrecs == 0) {
				/* add bundle into pool */
				bundle_pool.push_back(&kv[start]);
				start = cnt;
				//			ts += ts_delta;		/* increase timestamp */
#if 0
				x3 *tmp;
				tmp = bundle_pool.back();
				for (int i = 0 ; i < nrecs; i++) {
					cout << std::get<0>(tmp[i]) << std::get<1>(tmp[i]) << std::get<2>(tmp[i]) << endl;
				}
#endif
			}
		}
	} catch (const io::error::too_few_columns & e) {
		EE("skipped line %lu in input txt, which has no enough columns.", cnt);
	}

	xzl_bug_on_msg(bundle_pool.size() == 0, "no enough valid bundle");

	return cnt;
}

template<>
uint64_t FileSource<zmq_source::x4>::ParseCSVFile(uint64_t nrecs) {
	using namespace io;

	unsigned long start = 0;
	unsigned long cnt = 0;

	CSVReader<3, trim_chars<' ', '\t'>, no_quote_escape<','>, throw_on_overflow, single_line_comment<'#'>> in(fname);

	try {
		while (in.read_row(std::get<0>(kv[cnt]), std::get<1>(kv[cnt]), std::get<2>(kv[cnt]))) {
			cnt++;
			if (cnt % nrecs == 0) {
				/* add bundle into pool */
				bundle_pool.push_back(&kv[start]);
				start = cnt;
#if 0
				x4 *tmp;
				tmp = bundle_pool.back();
				for (int i = 0 ; i < nrecs; i++) {
					cout << std::get<0>(tmp[i]) << std::get<1>(tmp[i]) << std::get<2>(tmp[i]) << std::get<3>(tmp[i]) << endl;
				}
#endif
			}
		}
	} catch (const io::error::too_few_columns & e) {
		EE("skipped line %lu in input txt, which has no enough columns.", cnt);
	}

	xzl_bug_on_msg(bundle_pool.size() == 0, "no enough valid bundle");

	return cnt;
}

#endif

