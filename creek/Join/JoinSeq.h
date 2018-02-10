#ifndef SORTJOIN_H
#define SORTJOIN_H

#include <boost/thread/locks.hpp>
#include "util.h"
#include "core/Transforms.h"
#include "Values.h"
#include "BundleContainer.h"

extern "C" {
#include "measure.h"
}

namespace creek {
	enum JoinMode {
		JOIN_BYKEY,
		JOIN_BYFILTER_LEFT, 	/* left is the threshold */
		JOIN_BYFILTER_RIGHT,	/* right is the threshold */
	};
}

template<typename T, int keypos, int vpos, int N>
class JoinSeqEval;

/* Join by key. Expect sorted tuples.
 *
 * (Can also do Join by filtering.)
 *
 * left/right inputs are streams of record<kvpair>
 * Input/OutputBundle: WinSeqBundle (no window?)
 * */
template <
	class KVPair,
	int keypos,
	int vpos, /* -1 if does not care. must specify if join by value filtering */
	int N
>
class JoinSeq: public PTransform {
  using RecordKV = Record<KVPair>;

public:

	//////////////////////////////////////////////////////////////

	/*
	 * Join's internal state
	 */

	/* Represent the internal state of one side */
  struct WinKVPairContainer {
	  /* one container per window */
	  map<Window, WinKeySeqFrag<KVPair, N>, Window> containers;

		boost::shared_mutex mtx_container; /* for creating/destroying windows */

		/* each lock is for the i-th partitions in *all* windows. (a bit weird)
		 * for r/w partitions in a window. we can't use winkeyfrag's internal
		 * lock since we want r/w lock */
	  array<boost::shared_mutex, N> mtx_part;

		/* combine an incoming window to internal state
		 * may take the ownership of the underlying seqbuf of @incoming
		 * */
		void CombineByKey(WinKeySeqFrag<KVPair, N> const &incoming) {
			xzl_bug_on(incoming.seqbufs.size() != N);

			boost::shared_lock<boost::shared_mutex> rlock(mtx_container);
			boost::upgrade_lock<boost::shared_mutex> ulock(mtx_container, boost::defer_lock);
			unique_ptr<boost::upgrade_to_unique_lock<boost::shared_mutex>> pwlock
					= nullptr; /* will std::move later */

			/* no need to lock. rely on WinKeyFrag's internal lock */
//			boost::unique_lock<boost::shared_mutex> writer_lock(mtx_ctn);

start:
			if (!containers.count(incoming.w)) {  // a new window
				if (!upgrade_locks(&rlock, &ulock, &pwlock))
					goto start;

				containers[incoming.w] = incoming;
				VV("a new window. ts: " ETIME_FMT " container sz %lu",
					 incoming.w.start, containers.size());
			} else {  // combine to an existing window
				// don't have to use -Locked() version since we already took
				// part lock
//				containers[incoming.w].template Merged<keypos>(incoming, 0);

				auto & mycontainer = containers[incoming.w];

				for (auto i = 0u; i < N; i++) {
					boost::unique_lock<boost::shared_mutex> writer_lock(mtx_part[i]);
					mycontainer.template MergedByKey<keypos>(incoming, i);
				}
			}
		}

		/* todo: we may implement a combine() that simply concatenates seqbufs */

		// may emit a frag w/o seqbuf if no such window
		WinKeySeqFrag<KVPair> JoinByKeyEmit(WinKeySeqFrag<KVPair, N> & incoming) {
			WinKeySeqFrag<KVPair> res;

			xzl_bug_on(incoming.seqbufs.size() != N);

			vector<SeqBuf<Record<KVPair>>> emitted_seqbufs; /* will merge them w/o lock */

			res.w = incoming.w;
			res.seqbufs[0].AllocBuf();

			boost::shared_lock<boost::shared_mutex> rlock(mtx_container);

			if (!containers.count(incoming.w)) {
				I("nothing to emit");
				return res; // nothing to emit
			}

			auto & mybufs = containers[incoming.w].seqbufs;
			auto & theirbufs = incoming.seqbufs;
			auto & outbufs = res.seqbufs;

			/* join w/ each partition, and merge the emitted seqbuf to one seqbuf in res */
			for (auto i = 0u; i < N; i++) {
				boost::shared_lock<boost::shared_mutex> reader_lock(mtx_part[i]);
				/* XXX support join_kp */
				emitted_seqbufs.emplace_back(mybufs[i].template JoinByKey<keypos>(theirbufs[i]));
//				SeqBuf<Record<KVPair>> s = mybufs[i].template JoinByKey<keypos>(theirbufs[i]);
//				outbufs[0] = outbufs[0].template MergeByKey<keypos>(s);
			}

			rlock.unlock();

			/* now we have no lock. merge all emitted parts & emit */
			for (auto & em : emitted_seqbufs) {
//				EE("emitted kbatch"); dump_xaddr_info(em.kbatch->xaddr);
//				EE("emitted pbatch"); dump_xaddr_info(em.pbatch->xaddr);
				outbufs[0] = outbufs[0].template MergeByKey<keypos>(em);
			}

			return res;
		}

		/* @emit_incoming: true if incoming contains data tuples and @this contains
		 * the threshold. vice versa.
		 *
		 * Only supports N==1
		 * */
		WinKeySeqFrag<KVPair> JoinByFilterEmit(WinKeySeqFrag<KVPair,N> const & incoming,
			bool emit_incoming) {
			WinKeySeqFrag<KVPair> res;

			xzl_bug_on(N != 1);
			xzl_bug_on(incoming.seqbufs.size() != 1);

			res.w = incoming.w;
			res.seqbufs[0].AllocBuf();

			boost::shared_lock<boost::shared_mutex> reader_lock(mtx_container);

			xzl_bug_on_msg(!emit_incoming && incoming.seqbufs[0].Size() != 1,
			"incoming (filter) has !=1 records?");

			if (!containers.count(incoming.w)) {
				EE("window " ETIME_FMT " does not exist (total wins %zd). nothing to emit", incoming.w.start, containers.size());
				return res; // nothing to emit
			}

			boost::shared_lock<boost::shared_mutex> part_reader_lock(mtx_part[0]);

			if (emit_incoming)
				res.seqbufs[0] =
						incoming.seqbufs[0].\
						template JoinByFilter<vpos>(containers[incoming.w].seqbufs[0]);
			else
				res.seqbufs[0] =
						containers[incoming.w].seqbufs[0].\
						template JoinByFilter<vpos>(incoming.seqbufs[0]);

			return res;
		}

	  // drop containers whose records are earlier than or equal to @cutoff
	  // (i.e. only keep those who have records strictly later than @cutoff)
	  // @return: the min_ts among the remaining containers.
	  // NB: this is latency sensitive since it takes writer lock.
	  // So we only actually destroy hashmaps after releasing writer lock.
	  void purge_containers(etime cutoff) {
			boost::unique_lock<boost::shared_mutex> writer_lock(mtx_container);

			if (!containers.size())
				return; // nothing to purge. future wms will purge these containers.

		  /* temporarily hold the HT to be destroyed, but don't do so until
		   * we release the writer lock.
		   * The actual destruction of HT can be expensive.
		   */
			std::vector<WinKeySeqFrag<KVPair, N>> vct;

			//k2_measure("begin-purge");

		  // note that we erase containers while iterating - need some special treatment
		  for (auto && it = containers.begin(); it != containers.end(); /* no increment */ )
		  {
		  	Window const & win = it->first;
		  	auto & winfrag = it->second;

			  if (win.start + win.duration <= cutoff) {
			  	  //k2_measure("begin-erase");
				  vct.push_back(winfrag);
				  containers.erase(it++);
				  //k2_measure("end-erase");
				  //k2_measure_flush();
			  }
			  else
				  ++ it;
		  }

			writer_lock.unlock();

			/* dbg */
			for (auto __attribute__((__unused__)) & x : vct) {
				I("one window: drop %lu items", x.seqbufs[0].Size());
			}

			/* vct will be destroyed automatically, causing freeing of HT  */
	  }

	  WinKVPairContainer() { }
  };

  //////////////////////////////////////////////////////////////

  // state of the entire transform. left, right.
public:
	WinKVPairContainer win_containers[2];
  etime window_size;
	creek::JoinMode mode;

	// combine an incoming window's result to one side's internal state.
	// caller must not hold writer lock
	void Combine(WinKeySeqFrag<KVPair, N> const & incoming, int side) {
		win_containers[side].CombineByKey(incoming);
	}

	// join an incoming window's result with one side's internal state, and emit
	// the joint results.
	// @side: the internal side to join with. 0-left 1-right (NB: NOT side_info)
	// caller must not hold reader lock
	WinKeySeqFrag<KVPair> JoinEmit(WinKeySeqFrag<KVPair, N> & incoming, int side) {
		int incoming_side = 1 - side;
		int incoming_side_info = incoming_side + 1;

		I("to join with side %d", side);

		if (mode == JOIN_BYKEY)
			return win_containers[side].JoinByKeyEmit(incoming);
		else if (mode == JOIN_BYFILTER_LEFT) { /* emit from right bundles */
			return win_containers[side].JoinByFilterEmit(incoming,
																									 SIDE_INFO_R ==
																									 incoming_side_info);
		} else if (mode == JOIN_BYFILTER_RIGHT) { /* emit from left bundles */
			return win_containers[side].JoinByFilterEmit(incoming,
																									 SIDE_INFO_L ==
																									 incoming_side_info);
		} else
			xzl_bug("mode=?");
	}

  /* Drop containers.
   * this will internally lock join state */
  void flush_state(etime const & ts, int i) {
		I("side=%d, ts=" ETIME_FMT, i, ts);
  	win_containers[1-i].purge_containers(ts);
  }

  JoinSeq(string name, JoinMode mode, etime window_size)
    : PTransform(name, SIDE_INFO_J), window_size(window_size),  mode(mode) {

		/* sanity check */
		if (mode == JOIN_BYFILTER_LEFT || mode == JOIN_BYFILTER_RIGHT) {
			xzl_bug_on_msg(vpos == -1, "vpos seems unspecified?");
			xzl_bug_on_msg(keypos == vpos, "vpos=kpos?");
		}
	}

  void ExecEvaluator(int nodeid, EvaluationBundleContext *c,
  	shared_ptr<BundleBase> bundle_ptr) override {
		/* instantiate an evaluator */
		JoinSeqEval<KVPair, keypos, vpos, N> eval(nodeid);
		eval.evaluate(this, c, bundle_ptr);
	}
};
#endif /* SORTJOIN_H */
