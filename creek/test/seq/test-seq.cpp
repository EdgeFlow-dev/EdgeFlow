//
// Created by xzl on 3/20/17.
//
// g++-5 -std=c++11 test-seqbuf.cpp ../../Values.cpp ../../SeqBuf.cpp -I../../ -I../../../Kaskade/ -lboost_system -Wall -g -Wall
// g++-5 -std=c++11 test-seqbuf.cpp ../../SeqBuf.cpp -I../../ -I../../../Kaskade/ -lboost_system -Wall -g -Wall
// update: build through cmake
#if 0
#include "core/SeqObject.h"

namespace creek {
	thread_local worker_context this_thread_context;
};

using namespace creek;

int main() {
	this_thread_context.init_memblock();

	auto *p = new SeqArray<int>();

	return 0;
}
#endif


#include "SeqBuf.h"
#include "SeqBuf2.h"
#include "Values.h"
#include "WinKeySeqFrag.h"

using namespace std;
using namespace creek;

/* each element is long */
void test_long()
{
	using T = long;

	long raw_data[] = {16, 2, 77, 9, 100};

	SeqBuf<long> sb(raw_data, sizeof(raw_data) / sizeof(T));
	sb.Dump();

	// sort
	SeqBuf<long> sb1;
	sb1.AllocBuf();
	sb1.CopyFrom(sb);
	sb1.Dump();

	sb1 = sb.Sort();
	sb1.Dump();

	// merge
	auto sb3 = sb1.Merge(sb1);
	sb3.Dump();
}

/* each element is a record of kvpair. sort based on key */

void test_record()
{
	using KVPair = creek::kvpair;
	using T = Record<KVPair>;

	printf("---------- test %s -------------\n", __func__);

	/* sort1 */
	Record<KVPair> r1(make_pair(30, 10));
	Record<KVPair> r2(make_pair(10, 10));
	Record<KVPair> r3(make_pair(40, 10));

	Record<KVPair> raw_data[] = {r1, r2, r3};

	SeqBuf<Record<KVPair>> sb(raw_data, sizeof(raw_data) / sizeof(T));
	auto sb1 = sb.Sort();
	sb1.Dump();

	/* sort2 */
	Record<KVPair> raw_data2[] = {
			Record<KVPair>(make_pair(100, 10)),
			Record<KVPair>(make_pair(5, 10)),
			Record<KVPair>(make_pair(50, 10))
	};
	SeqBuf<Record<KVPair>> sb21(raw_data2, sizeof(raw_data2) / sizeof(T));
	auto sb22 = sb21.Sort();
	sb22.Dump();

	/* merge */
	auto sb3 = sb1.Merge(sb22);
	sb3.Dump();
}

/* test k2v */
void test_record2()
{
	using KVPair = creek::k2v;
	using T = Record<KVPair>;

	printf("---------- test %s -------------\n", __func__);

	/* sort1 */
	Record<KVPair> r1(make_tuple(30u, 1u, 10u));
	Record<KVPair> r2(make_tuple(10u, 2u, 10u));
	Record<KVPair> r3(make_tuple(40u, 3u, 10u));

	Record<KVPair> raw_data[] = {r1, r2, r3};

	SeqBuf<Record<KVPair>> sb(raw_data, sizeof(raw_data) / sizeof(T));
	auto sb1 = sb.Sort2<1>();
	sb1.Dump();

	/* sort2 */
	Record<KVPair> raw_data2[] = {
			Record<KVPair>(make_tuple(100, 5, 10)),
			Record<KVPair>(make_tuple(5, 6, 10)),
			Record<KVPair>(make_tuple(50, 7, 10))
	};
	SeqBuf<Record<KVPair>> sb21(raw_data2, sizeof(raw_data2) / sizeof(T));
	auto sb22 = sb21.Sort2<1>();
	sb22.Dump();

	/* merge */
	auto sb3 = sb1.MergeByKey<1>(sb22);
	sb3.Dump();
}

void test_seg_record()
{
	using KVPair = creek::kvpair;
	using T = Record<KVPair>;

	printf("---------- test %s -------------\n", __func__);

	Record<KVPair> r1(make_pair(30, 10), 10 /* ts */);
	Record<KVPair> r2(make_pair(10, 10), 15 /* ts */);
	Record<KVPair> r3(make_pair(40, 10), 20 /* ts */);
	Record<KVPair> r4(make_pair(50, 10), 21 /* ts */);
	Record<KVPair> r5(make_pair(110, 10), 22 /* ts */);

	Record<KVPair> raw_data[] = {r1, r2, r3, r4, r5};

	SeqBuf<Record<KVPair>> sb(raw_data, sizeof(raw_data) / sizeof(T));

	/* return a vector */
	auto res = sb.Segment<vector<WinSeqFrag<KVPair>>>(etime(0), etime(10));
	for (auto & x : res) {
		auto & w = x.w;
		auto & r = x.vals;
		cout << "window: " << w.start;
		r.Dump();
		cout << endl;
	}

	/* return a map */
	auto res1 = sb.Segment<map<Window, WinKeySeqFrag<KVPair>, Window>>
	      (etime(0), etime(10));
	for (auto & x : res1) {
		cout << "window: " << x.first.start;
		x.second.seqbufs[0].Dump();
		cout << endl;
	}
}


int main() {
	test_long();
	test_record();
	test_record2();
	test_seg_record();
	return 0;
}
