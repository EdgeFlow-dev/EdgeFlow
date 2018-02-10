//
// Created by xzl on 8/23/17.
//

#include <map>
#include <memory>
#include "SeqBuf.h"
#include "WinKeySeqFrag.h"

using namespace std;

/* specialization -- sort/merge/etc */
using KVPair = creek::kvpair;
using RecordKVPair = Record<KVPair>;

template<>
void SeqBuf<RecordKVPair>::Dump(const char * tag) const
{

#ifdef USE_UARRAY
	/* read back and dump */
	unsigned len = 10; /* in T */
	const unsigned rlen = reclen<RecordKVPair>();
	unsigned slen = len * rlen; /* in simd_t */

//	unique_ptr<simd_t[]> nsbuf(new simd_t[len]);
//	uarray_to_nsbuf(nsbuf.get(), pbuf->xaddr, len);

	printf("dump (%s): xaddr %lx print %u items\n", tag, pbuf->xaddr, slen);
	if (!pbuf->xaddr) {
		printf("<no underlying sbatch>\n");
		return;
	}

	RecordKVPair *nsbuf = new RecordKVPair[len];
	uarray_to_nsbuf((simd_t *)nsbuf, pbuf->xaddr, &slen);

	for (unsigned i = 0; i < slen; i++) {
		printf("%08x ", *((simd_t *) nsbuf + i));
		if (i % rlen == rlen - 1)
			printf("\t");
		if ((i % (3 * rlen)) + 1 == 3 * rlen)
			printf("\n");
	}
	printf("\n");

	for (unsigned i = 0; i < len; i++) {
		auto & x = nsbuf[i];
		printf("t: " ETIME_FMT "<" KTYPE_FMT "," VTYPE_FMT ">\n",
					 x.ts, std::get<0>(x.data), std::get<1>(x.data)
		);
	}

	delete nsbuf;

#else
	printf("dump (%s): total %lu items\n", tag, Size());
	for (auto && x : *pbuf)
		cout << "t:" << x.ts << " " << x.data.first << ":" << x.data.second << " " << endl;
	cout << endl;
#endif
};


template<>
void SeqBuf<RecordK2V>::Dump(const char *tag) const
{
#ifdef USE_UARRAY

#else
	printf("dump (%s): total %lu items\n", tag, pbuf->size());
	for (auto && x : *pbuf)
		printf("t: " ETIME_FMT "<" KTYPE_FMT "," KTYPE_FMT "," VTYPE_FMT ">\n",
			x.ts, std::get<0>(x.data), std::get<1>(x.data), std::get<2>(x.data)
		);
#endif
};


#if 0 // obsoleted
template<>
SeqBuf<RecordKVPair>
SeqBuf<RecordKVPair>::Merge(SeqBuf<RecordKVPair> const & other)
{
	SeqBuf res;
	res.AllocBuf();

	xzl_bug_on_msg(!IsSortedSame(*this, other), "bug: try to merge unsorted seqbufs");

	std::merge(pbuf->begin(), pbuf->end(), other.pbuf->begin(), other.pbuf->end(),
						 back_inserter(*(res.pbuf)),   /* must use this. otherwise no enough elements in dst */
						 KVRecordCompLess<RecordKVPair>());

	res.sorted_ = 0;

	return res;
}
#endif

/* to be specialized
 * We won't return too many Windows. So return a vector by copy is fine.
 *
 * Note: this is a slow implementation.
 */
#if 0
template<> template<>
vector<WinSeqFrag<KVPair>>
SeqBuf<RecordKVPair>::Segment(creek::etime base, creek::etime subrange)
{
	std::map<Window, SeqBuf, Window> m;

	/* seg into map */
	for (auto & x : *pbuf) {
		xzl_bug_on(x.ts < base);
		Window w(x.ts - ((x.ts - base) % subrange) /* window start */, subrange);
		if (m.find(w) == m.end()) {
			m[w].AllocBuf();
		}
		m[w].pbuf->push_back(x);
	}

	/* dump map into a vector */
	vector<WinSeqFrag<KVPair>> ret;
	for (auto & winfrag : m)
		ret.push_back(WinSeqFrag<KVPair>(winfrag.first, winfrag.second));
	return ret;
}
#endif

#if 0
/* we may also return a win map */
template<> template<>
std::map<Window, WinKeySeqFrag<KVPair>, Window>
SeqBuf<RecordKVPair>::Segment(creek::etime base, creek::etime subrange)
{
	std::map<Window, WinKeySeqFrag<KVPair>, Window> m;
	/* seg into map */

	for (auto & x : *pbuf) {
		xzl_bug_on(x.ts < base);
		Window w(x.ts - ((x.ts - base) % subrange) /* window start */, subrange);
		if (m.find(w) == m.end()) {
			m[w] = WinKeySeqFrag<KVPair>(w); /* default has 1 seqbuf */
			m[w].seqbufs[0].AllocBuf();  /* alloc underlying buffer */
		}
		m[w].seqbufs[0].pbuf->push_back(x);
	}
	DD("map has %lu segments", m.size());

	return m;
}
#endif

#if 0
/* emit WHERE low < key < high
 * @low/high are all records for simplicity
 * */
template<>
SeqBuf<RecordKVPair> SeqBuf<RecordKVPair>::Filter(RecordKVPair const & low,
																									RecordKVPair const & high)
{
	SeqBuf<RecordKVPair> ret;
	ret.AllocBuf();
	for (auto & x : *pbuf) {
		if (low.data.first < x.data.first && x.data.first < high.data.first)
			ret.pbuf->push_back(x);
	}
	return ret;
}
#endif
