//
// Created by xzl on 8/30/17.
//
// cf Unbounded.cpp

#include "Values.h"
#include "SourceSeqZmq.h"
#include "SourceSeqZmqEval.h"

/* Must in .cpp to avoid circular dep
 *
 * no explicit instantiation needed either */

/* XXX handle lines start with #, which are comment lines */

template<>
void
SourceSeqZmq<creek::kvpair>::ReadOneLine(FILE *file, creek::kvpair *kv)
{
	int ret;

	ret = fscanf(file, KTYPE_FMT VTYPE_FMT "\n", &(kv->first), &(kv->second));
	xzl_bug_on(ret < 0);
}

template<>
void
SourceSeqZmq<creek::k2v>::ReadOneLine(FILE *file, creek::k2v *kv)
{
	int ret = fscanf(file, KTYPE_FMT KTYPE_FMT VTYPE_FMT "\n",
									 &(std::get<0>(*kv)), &(std::get<1>(*kv)), &(std::get<2>(*kv)));
	xzl_bug_on(ret < 0);
}

template<>
unsigned long
SourceSeqZmq<creek::kvpair>::ParseCSVFile(const string & fname, creek::kvpair* kv, unsigned long len)
{
	using namespace io;
	unsigned long count = 0;

	xzl_bug_on(!kv);

	CSVReader<2, trim_chars<' ', '\t'>, no_quote_escape<','>, throw_on_overflow, single_line_comment<'#'>> in(fname);
	while(in.read_row(kv->first, kv->second)) {
		kv++;
		count++;
		if (len == count)
			break;
	}
	return count;
}

template<>
unsigned long
SourceSeqZmq<creek::k2v>::ParseCSVFile(const string & fname, creek::k2v* kv, unsigned long len)
{
	using namespace io;
	unsigned long count = 0;

	xzl_bug_on(!kv);

	CSVReader<3, trim_chars<' ', '\t'>, no_quote_escape<','>, throw_on_overflow, single_line_comment<'#'>> in(fname);
	while(in.read_row(std::get<0>(*kv), std::get<1>(*kv), std::get<2>(*kv))) {
		kv++;
		count++;
		if (len == count)
			break;
	}
	return count;
}