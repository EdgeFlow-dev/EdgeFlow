//
// Created by xzl on 8/27/17.
//
// since partial template specialization is impossible, we have to resort to the following verbose code
//
// cf WinSum_addlong.cpp
//

#include "WinSumSeq.h"
#include "WinSumSeqEval.h"
#include "SeqBuf2.h"	// contain k2v specialization

#if 0
/* this is no longer static */
template <>
WinKeySeqFrag<kvpair> const &
WinSumSeq<kvpair, kvpair, 0/*key*/, 4/*N*/>::aggregate_init(WinKeySeqFrag<kvpair> * winres)
{
	xzl_bug_on(!winres);

//	winres->Resize(this->concurrency); /* only succeed if no underlying buffers */

	for (auto & s : winres->seqbufs)
		s.AllocBuf();

	return *winres;
};
#endif

#if 0
/* note that this is static
 * merge underlying seqbufs (single thread)
 */
template <>
void
WinSumSeq<kvpair, kvpair, 0>::combine(WinKeySeqFrag<kvpair> & mine,
																	 WinKeySeqFrag<kvpair> const & others)
{
	// will replace the underlying seqbuf
//	auto seqbuf = mine.seqbuf.Merge(others.seqbuf);
//	mine.seqbuf = seqbuf;

	xzl_bug_on(mine.seqbufs.size() != others.seqbufs.size());

	for (unsigned int i = 0; i < mine.seqbufs.size(); i++)
		mine.Merged2Locked<0>(others, i);

	/* refrain from summation as follows. instead, hold until state flush */
	// 	mine.Sumed(); /* aggregation again */
}

template <>
void
WinSumSeq<kvpair, kvpair, 0>::ExecEvaluator(int nodeid, EvaluationBundleContext *c,
							std::shared_ptr<BundleBase> bundle)
{
	WinSumSeqEval<kvpair, kvpair, 0> eval(nodeid);
	eval.evaluate(this, c, bundle);
};

/* ---------------------------------------------------------- */

template <>
WinKeySeqFrag<k2v> const &
WinSumSeq<k2v, k2v, 0>::aggregate_init(WinKeySeqFrag<k2v> * winres)
{
	xzl_bug_on(!winres);

	winres->Resize(this->concurrency); /* only succeed if no underlying buffers */

	for (auto & s : winres->seqbufs)
		s.AllocBuf();

	return *winres;
};

template <>
void
WinSumSeq<k2v, k2v, 0>::combine(WinKeySeqFrag<k2v> & mine,
																			WinKeySeqFrag<k2v> const & others)
{
	xzl_bug_on(mine.seqbufs.size() != others.seqbufs.size());

	for (unsigned int i = 0; i < mine.seqbufs.size(); i++)
		mine.Merged2Locked<0>(others, i);
}

template <>
void
WinSumSeq<k2v, k2v, 0>::ExecEvaluator(int nodeid, EvaluationBundleContext *c,
																						std::shared_ptr<BundleBase> bundle)
{
	WinSumSeqEval<k2v, k2v, 0> eval(nodeid);
	eval.evaluate(this, c, bundle);
};

/* ---------------------------------------------------------- */

template <>
WinKeySeqFrag<k2v> const &
WinSumSeq<k2v, k2v, 1>::aggregate_init(WinKeySeqFrag<k2v> * winres)
{
	xzl_bug_on(!winres);

	winres->Resize(this->concurrency); /* only succeed if no underlying buffers */

	for (auto & s : winres->seqbufs)
		s.AllocBuf();

	return *winres;
};

template <>
void
WinSumSeq<k2v, k2v, 1>::combine(WinKeySeqFrag<k2v> & mine,
																WinKeySeqFrag<k2v> const & others)
{
	xzl_bug_on(mine.seqbufs.size() != others.seqbufs.size());

	for (unsigned int i = 0; i < mine.seqbufs.size(); i++)
		mine.Merged2Locked<1>(others, i);
}

template <>
void
WinSumSeq<k2v, k2v, 1>::ExecEvaluator(int nodeid, EvaluationBundleContext *c,
																			std::shared_ptr<BundleBase> bundle)
{
	WinSumSeqEval<k2v, k2v, 1> eval(nodeid);
	eval.evaluate(this, c, bundle);
};

#endif
/* ---------------------------------------------------------- */
