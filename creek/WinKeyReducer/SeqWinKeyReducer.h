//
// Created by xzl on 3/17/17.
//

#ifndef CREEK_SEQSeqWinKeyReducer_H
#define CREEK_SEQSeqWinKeyReducer_H



#include "Values.h"
#include "core/StatefulTransform.h"

/* Stateful reducer of windowed/keyed records.
 * Execute DoFn on each window / each key
 *
 * InputT : WindowKeyedFragment<KVPair>
 */
template <
		typename KVPairIn,
		/* the input windowkeyfrag. its vcontainer must be the same as the
		 * upstream's output, e.g wingbk.
		 * the format can be really cheap, i.e. no concurrent support.
		 */
		template<class> class InputWinKeyFragT,  // = SeqWinKeyFrag (family)
		/* internal state. concurrent d/s
		 * can be specialized based on key/val distribution.
		 * regarding the d/s for holding eval's local aggregation results,
		 * see comment below. */
		template<class> class InternalWinKeyFragT, // = SeqWinKeyFragTable
		typename KVPairOut,
		template<class> class OutputBundleT_
>
class SeqWinKeyReducer
		: public StatefulTransform<
				SeqWinKeyReducer<KVPairIn,InputWinKeyFragT,InternalWinKeyFragT,KVPairOut,OutputBundleT_>,
				InputWinKeyFragT<KVPairIn>,			/* InputT -- unused? */
				/* --- transform- or local- window result: all shared ptr --- */
				shared_ptr<InternalWinKeyFragT<KVPairIn>>,		/* WindowResultT */
				shared_ptr<InputWinKeyFragT<KVPairIn>>		/* LocalWindowResultT */
		>
{

public:
	using K = decltype(KVPairIn::first);
	using V = decltype(KVPairIn::second);

	struct KVInter {
		K k;
		V v;  /* aggregated */
		/* custom */
		unsigned int count = 0;
		bool is_init = false; /* once */
	};
//	using KVInter = KVPairIn; /* intermediate per key reduction result */

	/* another type of def */
	struct VInter {
		V v;  /* sum'ed */
		/* custom field */
		unsigned int count = 0;
		bool is_init = false; /* once? */
	};
	using KVInter1 = std::pair<K, VInter>;

#if 0
	/* container can be unsafe */
	using InputValueContainerT = typename InputWinKeyFragT<KVPairIn>::ValueContainerT;
	/* container must be safe */
	using InternalValueContainerT = typename InternalWinKeyFragT<KVPairIn>::ValueContainerT;
#endif

	using TransformT = SeqWinKeyReducer<KVPairIn, InputWinKeyFragT,
	InternalWinKeyFragT,
	KVPairOut,OutputBundleT_>;

	using InputT = InputWinKeyFragT<KVPairIn>;

	/* Aggregation types
	 *
	 * localwindowresult: used by eval to hold local result
	 * windowresult: used for transform's internal state.
	 *
	 * the intention of separating these two is that localwindowresult does not have
	 * to be concurrent d/s and therefore can be cheap.
	 * However, when making localwindowresult as based on InputWindowKeyedFragmentT
	 * (e.g. std::unordered_map with SimpleValueContainerUnsafe), the performance
	 * is even worse than (localwindowresult == windowresult).
	 *
	 * This can be tested by toggling the comments below.
	 */
	using WindowResultT = shared_ptr<InternalWinKeyFragT<KVPairIn>>;
//	using LocalWindowResultT = WindowResultT;
	using LocalWindowResultT = shared_ptr<InputWinKeyFragT<KVPairIn>>; /* we don't do local agg */

	/* AggResultT and LocalAggResultT are decl in StatefulTransform */

#if 0
	// can't do this. each window cannot be associated with multiple "fragments"
  // where the keys may be duplicated
//  typedef map<Window,
//              vector<shared_ptr<WindowKeyedFragment<K,V>>>, Window>
//              windows_map;

private:
   unsigned long window_count = 0;
  /* holds per-window states. ordered by window time, thus can be iterated.
     each window contains multiple keys and the associated value containers */
  windows_map windows;
  mutex _mutex;

protected:
  // the age of the oldest internal record. *not* the watermark
  ptime min_ts = max_date_time;
#endif

public:
	SeqWinKeyReducer(string name)
			: StatefulTransform<TransformT, InputT, WindowResultT, LocalWindowResultT>(name) { }

	////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////

	// reduce on one specific key and all its values (as in a value container)
//	virtual KVPairIn do_reduce(K const &, InternalValueContainerT const &);
//	virtual KVPairIn do_reduce_unsafe(K const &, InputValueContainerT const &);

	/* XXX need intermediate results: key-(some intermediate results)
	 * return: true if kvpair successfully reduced;
	 * false if failed (e.g. @kvpair has a new key than @iv)
	 * */
	virtual bool do_reduce_one(KVPairIn const & kvpair, KVInter * iv);

	/* complete reduction on one key. emit kvpair from intermediate res */
	virtual KVPairIn emit_kvpair(KVInter *iv);

	/* init a window's agg results (before reduction) inside the transform
	 * XXX still useful?
	 * */
	static WindowResultT const & aggregate_init(WindowResultT * acc) {
		xzl_assert(acc);
		(*acc) = make_shared<typename WindowResultT::element_type>();
		return *acc;
	}

	/* init a window's state in an eval */
	static LocalWindowResultT const & local_aggregate_init(LocalWindowResultT * acc) {
		xzl_assert(acc);
		/* more generic -- don't have to change back/forth */
		(*acc) = make_shared<typename LocalWindowResultT::element_type>();
		return *acc;
	}

	static WindowResultT const & aggregate(WindowResultT * acc, InputT const & in);

	/* combine the evaluator's (local) aggregation results (for a particular window)
	 * to the tran's internal state.
	 * For seq, this is just depositing the seq buffer ptr
	 */
	static WindowResultT const & combine(WindowResultT & mine,
																LocalWindowResultT const & others);

	void ExecEvaluator(int nodeid, EvaluationBundleContext *c,
										 shared_ptr<BundleBase> bundle_ptr) override;
};

/* the default version that outputs WindowsBundle */
template <
		typename KVPairIn,
		template<class> class InputWinKeyFragT,
		template<class> class InternalWinKeyFragT // = WindowKeyedFragment
>
using SeqWinKeyReducer_winbundle = SeqWinKeyReducer<KVPairIn,
		InputWinKeyFragT, InternalWinKeyFragT,
		KVPairIn, /* kv out */
		WindowsBundle>;

#endif //CREEK_SEQSeqWinKeyReducer_H
