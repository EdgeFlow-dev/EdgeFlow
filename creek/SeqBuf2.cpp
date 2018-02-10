//
// Created by xzl on 9/18/17.
//
// SeqBuf specialized for creek::k2v (each record has 2 keys and 1 vec)
//
// cf SeqBuf.cpp

#include "SeqBuf2.h"
#include "WinKeySeqFrag.h"

using K2V = creek::k2v;

/* XXX: move funcs here to SeqBuf.cpp */

#if 0
/* we may also return a win map */
template<> template<>
std::map<Window, WinKeySeqFrag<K2V>, Window>
SeqBuf<RecordK2V>::Segment(creek::etime base, creek::etime subrange)
{
	std::map<Window, WinKeySeqFrag<K2V>, Window> m;

	/* seg into map */
	for (auto & x : *pbuf) {
		xzl_bug_on(x.ts < base);
		Window w(x.ts - ((x.ts - base) % subrange) /* window start */, subrange);
		if (m.find(w) == m.end()) {
			m[w] = WinKeySeqFrag<K2V>(w); /* default has 1 seqbuf */
			m[w].seqbufs[0].AllocBuf();  /* alloc underlying buffer */
		}
		m[w].seqbufs[0].pbuf->push_back(x);
	}
	DD("map has %lu segments", m.size());

	return m;
}
#endif
