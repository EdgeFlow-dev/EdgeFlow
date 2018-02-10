//
// Created by xzl on 9/1/17.
//

#ifndef CREEK_SEQBUNDLE_H
#define CREEK_SEQBUNDLE_H

#include "Bundle.h"

/* cf: RecordBundle
 * wrap around secure buffers that are inaccessible to normal world */
template <typename T>
struct RecordSeqBundle : public BundleBase {

	using RecordT = Record<T>;

	/* create seqbuf from raw content, which is immutable */
	RecordSeqBundle(RecordT *raw, int size)
			: BundleBase(-1 /* node */), seqbuf(raw, size) {
		xzl_bug_on(!(raw && size));
	}

	/* Used by source only. to be backward compatible.
	 * @size may be ignored */
	RecordSeqBundle(int size) : BundleBase (-1 /* node */) {
		seqbuf.AllocBuf();
	}

	/* compatible with other bundle interface.
	 * cf SingleInputTransformEvaluator.h process_bundle()
	 */
	RecordSeqBundle(int size, int node) : BundleBase (-1 /* node */) { }

	long size() override { return seqbuf.Size(); }

#if 0
	/* for debugging */
  virtual ~RecordSeqBundle() {
    W("bundle destroyed. #records = %lu", _content.size());
  }
#endif

#ifndef USE_UARRAY
	/* used by source only */
	void add_record(const RecordT & val) {
		seqbuf.pbuf->push_back(val);
	}
#endif

	SeqBuf<RecordT> seqbuf;

private:
};

template <typename KVPair, unsigned N=1 /* # of seqbufs per win */>
struct WinKeySeqBundle : public BundleBase {

//	using K = decltype(KVPair::first);
//	using V = decltype(KVPair::second);
	using RecordKV = Record<KVPair>;
	using WinKeySeqFragKV = WinKeySeqFrag<KVPair,N>;

	/* multiple windows are still organized in maps */
	// ordered, can iterate in window time order.
	// embedding WindowFragmentKV, which does not contain the actual Vs
	map<Window, WinKeySeqFragKV, Window> vals;

	/* no method for adding record */

	/* ignore the capacity */
	WinKeySeqBundle(int capacity = 0, int node = -1) : BundleBase(node) { };
};

template <typename T, unsigned N=1>
using WinSeqBundle = WinKeySeqBundle<T, N>;

#endif //CREEK_SEQBUNDLE_H
