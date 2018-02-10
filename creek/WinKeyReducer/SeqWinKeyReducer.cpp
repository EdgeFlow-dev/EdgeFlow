//
// Created by xzl on 3/17/17.
//
// cf: WinKeyReducer-wc-common.h
//

#include "config.h"

#include "SeqWinKeyReducer.h"
#include "SeqWinKeyReducerEval.h"

#if 1
/* necessary since instantiation later needs these defs.  */
template <typename KVPair,
		template<class> class InputWinKeyFragT,
		template<class> class InternalWinKeyFragT,
		typename KVPairOut,
		template<class> class OutputBundleT_
>
void SeqWinKeyReducer<KVPair,InputWinKeyFragT,
		InternalWinKeyFragT, KVPairOut, OutputBundleT_>::ExecEvaluator
		(int nodeid, EvaluationBundleContext *c, shared_ptr<BundleBase> bundle_ptr)
{
	/* instantiate an evaluator */
	SeqWinKeyReducerEval<KVPair,InputWinKeyFragT,InternalWinKeyFragT,
	KVPairOut, OutputBundleT_> eval(nodeid);
	eval.evaluate(this, c, bundle_ptr);
}
#endif

/* ----------------------- ops specialization for netmon --------------------------------- */

using KVPair1 = pair<creek::ippair, long>;
using MyWinKeyReducer = SeqWinKeyReducer<
		KVPair1,  /* kvpair in */
		SeqWinKeyFrag,
		SeqWinKeyFragTable,
		KVPair1,  /* kvpair out */
		WindowsBundle>;

//using WindowResultPtr1 = shared_ptr<SeqWinKeyFragTable<SeqWinKeyFrag>>;

/* simply appending vcontainers w/o actually touching values.
 * actual reduction is done in the wm path.
 *
 * @mine @others: shared_ptrs.
 *
 * XXX if @mine is empty, should we just swap pointers? */
template <>
WindowResultT const & MyWinKeyReducer::combine
(WindowResultT & mine, LocalWindowResultT const & others)
{
	xzl_assert(mine && others);
	xzl_assert(WindowEqual(mine->w, others->w));

	mine->depositOneFragT(creek::this_thread_context.wid, others);
	return mine;
}

template<>
bool MyWinKeyReducer::do_reduce_one
		(KVPair1 const & kvpair, KVInter * iv)
{
	xzl_assert(iv);

	/* save key if first time */
	if (!iv->is_init) {
		iv->k = kvpair.first;
		iv->is_init = true;
	} else {
		/* cmp key */
	  if (iv->k != kvpair.first)
	  	return false; /* key mismatch */
	}

	iv->count += 1;
	iv->v += kvpair.second;

	return iv;
}

template<>
KVPair1 MyWinKeyReducer::emit_kvpair(KVInter *iv) {
	xzl_assert(iv && iv->is_init);
	return make_pair(iv->k, iv->v);
}
