//
// Created by xzl on 9/19/17.
//
// Contains SeqBuf's specializaiton for k2v
//
// partial nested template specialization should also go to .h file

// cf: SeqBuf.h

#ifndef CREEK_SEQBUF2_H
#define CREEK_SEQBUF2_H

#include "SeqBuf.h"

using RecordK2V = Record<creek::k2v>;

#if 0
/* aggregation -- K2V. per key agg. NB: return K2V (instead of record) */
template<>
template<int key>
vector<SeqBuf<RecordK2V>>
SeqBuf<RecordK2V>::SumByKey(int n)
{
//	if (!this->IsSorted() || this->SortedKey() != key) {
//		EE("bug: this->Size() %lu this->SortedKey %d", this->Size(), this->SortedKey());
//	}

	xzl_bug_on_msg(this->Size() && this->SortedKey() != key, "bug: unsorted or sorted on a diff key");

	vector<SeqBuf<RecordK2V>> res(n);
	creek::ktype k;
	creek::vtype v;
	int outi = 0;

	for (auto & r : res) {
		r.AllocBuf();
		r.sorted_ = key;
	}

	if (!this->pbuf->size())
		return res; 		// nothing

	k = std::get<key>(this->pbuf->at(0).data);
	v = std::get<2>(this->pbuf->at(0).data);

	for (auto & rec : *(this->pbuf)) {
		auto & kk = std::get<key>(rec.data);
		auto & vv = std::get<2>(rec.data);

		if (kk != k) { /* emit a record of k2v. round robin to output seqbufs */
			/* NB: no k2 provided; no ts provided */
			res[outi].pbuf->push_back(RecordK2V(make_tuple(k, 0u, v)));
			outi = (outi + 1) % n;
			k = kk;
			v = 0;
		}
		v += vv;
	}

	/* emit last */
	res[outi].pbuf->push_back(make_tuple(k, 0u, v));

	return res;
}

template<>
template<int key>
SeqBuf<RecordK2V>
SeqBuf<RecordK2V>::SumByKey(void)
{
	auto res = this->SumByKey<key>(1);
	return res[0];
}
#endif

template<>
void SeqBuf<RecordK2V>::Dump(const char *) const;

//template<>
//SeqBuf<RecordK2V> SeqBuf<RecordK2V>::Filter(RecordK2V const & low,
//																						RecordK2V const & high);

//template<> template<>
//map<Window, WinKeySeqFrag<creek::kvpair,1/*N*/>, Window>
//SeqBuf<RecordK2V>::Segment(creek::etime base, creek::etime subrange);

#endif //CREEK_SEQBUF2_H
