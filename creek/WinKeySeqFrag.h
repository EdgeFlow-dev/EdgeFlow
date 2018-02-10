//
// Created by xzl on 8/23/17.
//
/* Derived from WinKeyFrag.h. However, the underlying bufs are SeqBuf */

#ifndef CREEK_WINKEYSEQFRAG_H
#define CREEK_WINKEYSEQFRAG_H

#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>

//#include "Values.h"
#include "Window.h"
#include "Record.h"
#include "SeqBuf2.h"

template <typename T>
struct SeqBuf;

template <typename T>
struct WinSeqFrag
{
	using RecordT = Record<T>;

	Window w;
	SeqBuf<RecordT> vals;

	WinSeqFrag() { }

	WinSeqFrag<T>(const Window& w) : w(w) { /* underlying buf not allocated */ }

	WinSeqFrag<T>(const Window& w, const SeqBuf<RecordT>& v) : w(w), vals(v) { }

	void print(ostream &os) const {
		os << "WindowFragment min_ts: " << " data: {";
		for (auto & rec : *vals.pbuf) {
			os << rec.data << ",";
		}
		os << "} end" << endl;
	}
};


/* One window, multiple keys, and the value containers associated with
 * the keys.
 *
 * Copying this should be cheap (= copy a shared ptr)
 *
 * @N: # of underlying seqbufs
 */
template <class KVPair, unsigned int N = 1>
struct WinKeySeqFrag
{
	using RecordKV = Record<KVPair>;

	Window w;

	/* each frag (a sorted seq of kvpairs) */
//	boost::shared_mutex mtx_; /* coarse grained mtx to protect the entire d/s */

	// for parallelism we may enclose multiple seqbufs
//	vector<SeqBuf<RecordKV>> seqbufs;
	array<SeqBuf<RecordKV>, N> seqbufs;
	array<boost::mutex, N> mtx_; /* each proteching one seqbuf */

	/* explict ctor needed to avoid copying mtx */
	WinKeySeqFrag(WinKeySeqFrag<KVPair, N> const & other)
	: w(other.w), seqbufs(other.seqbufs) { }

	/* window only */
	WinKeySeqFrag(const Window & w) : w(w) { }

	/* win and 1 seqbuf */
	WinKeySeqFrag(const Window & w, SeqBuf<RecordKV> const & other)
	: w(w) { seqbufs[0] = other; }

	/* win + an array of seqbufs */
	WinKeySeqFrag(const Window & w, array<SeqBuf<RecordKV>, N> const & other)
			: w(w), seqbufs(other) { }

	/* for compatiblity */
	WinKeySeqFrag(const Window & w, vector<SeqBuf<RecordKV>> const & othervec)
			: w(w){
		xzl_bug_on(othervec.size() != N);
		for (unsigned i = 0; i < N; i++)
			seqbufs[i] = othervec[i];
	}

	WinKeySeqFrag() { }

//	WinKeySeqFrag(unsigned int n_seqbuf) : seqbufs(n_seqbuf) { }

	unsigned int Size(void) const {
		return seqbufs.size();
	}

#if 0
	/* resize to @seqbufs to be size @i. will succeed iff current seqbufs have no underlying buffers.
	 * return if resize is okay */
	bool Resize(unsigned int i) {
		for (auto & s : seqbufs)
			if (s.Size() != 0) {
				xzl_bug("bug?");
				return false;
			}

		seqbufs.resize(i);
		return true;
	}
#endif

	/* sort #i seqbuf in place(and replace the backing seqbuf)
	 * */
	template <int keypos>
	void Sorted2(unsigned int part) {
		xzl_bug_on(part > seqbufs.size() - 1);
		xzl_bug_on(!seqbufs[part].pbuf);

		if (!seqbufs[part].IsSorted()) {
			seqbufs[part] = seqbufs[part].template Sort2<keypos>();
		}
	}

	template <int k1pos, int k2pos>
	void SortedMultiKey(unsigned int part) {
		xzl_bug_on(part > seqbufs.size() - 1);
		xzl_bug_on(!seqbufs[part].pbuf);

		if (!seqbufs[part].IsSorted()) {
			seqbufs[part] = seqbufs[part].template SortMultiKey<k1pos, k2pos>(1/*part*/)[0];
		}
	};

	/* merge an incoming WinKeySeqwFrag into this
	 * merge the i-th seqbuf only (for parallel execution)
	 * (and replace the backing seqbuf) */
	template <int keypos>
	void MergedByKey(WinKeySeqFrag const &other, unsigned int i) {
		/* sanity check: # of underlying seqbufs must agree */
		xzl_bug_on(seqbufs.size() != other.seqbufs.size());
		seqbufs[i] = seqbufs[i].template MergeByKey<keypos>(other.seqbufs[i]);
	}

	/* lock version for concurrent combination of window state */
	template <int keypos>
	void Merged2Locked(WinKeySeqFrag const & other, unsigned int i) {
		boost::unique_lock<boost::mutex> lock(mtx_[i]);
		MergedByKey<keypos>(other, i);
	}

	/* merge all local (multiple) seqbufs into the 0-th one */
	template <int keypos>
	void MergedByKey() {
		int len = seqbufs.size();
		xzl_bug_on(!len);

		W("frag total %d seqbufs", len);

		if (len == 1)
			return; // no need to merge

		/* merged all into the 0-th seqbuf */
		for (int i = 1; i < len; i++)
			seqbufs[0] = seqbufs[0].template MergeByKey<keypos>(seqbufs[i]);

		/* drop all but the 0-th seqbuf. we can't delete elements (?) from array */
		for (unsigned i = 1; i < N; i++)
			seqbufs[i].pbuf = nullptr;  /* clear the shared ptr */
//			seqbufs.resize(1);
	}

	/* run summation on the i-th seqbuf.
	 * XXX need lock if multi threads access different i? */
	template <int keypos, int vpos>
	void SumByKeyInplace(unsigned int i) {
		xzl_bug_on(i + 1 > seqbufs.size());
		xzl_bug_on(!seqbufs[i].pbuf);

		seqbufs[i] = seqbufs[i].template SumCount_1_OutBatch<keypos,vpos>(SB_SUM_PERKEY);
	}

	template <int keypos, int vpos>
	void MedByKeyInplace(unsigned int i, OpMode mode, float k) {
		xzl_bug_on(i + 1 > seqbufs.size());
		xzl_bug_on(!seqbufs[i].pbuf);

		seqbufs[i] = seqbufs[i].template MedByKey_1_OutBatch<keypos,vpos>(mode, k);
	}

	template <int keypos, int vpos>
	void SumCountInplace(unsigned i, OpMode mode) {
		xzl_bug_on(i + 1 > seqbufs.size());
		xzl_bug_on(!seqbufs[i].pbuf);

		seqbufs[i] = seqbufs[i].template SumCount_1_OutBatch<keypos,vpos>(mode);
	};

	template <int vpos>
	void SumAllInplace(unsigned int i) {
		xzl_bug_on(i + 1 > seqbufs.size());
		xzl_bug_on(!seqbufs[i].pbuf);

		seqbufs[i] = seqbufs[i].template SumAllToOne<vpos>();
//		seqbufs[i] = seqbufs[i].template SumCountToOne<0 /* don't care */, vpos>();
		/* XXX assign the window's ts to the only output tuple? */
	};

	void ConcatenatedLocked(WinKeySeqFrag const &other, unsigned int i) {
		xzl_bug_on(seqbufs.size() != other.seqbufs.size());
		boost::unique_lock<boost::mutex> lock(mtx_[i]);
		seqbufs[i].Concatenate(other.seqbufs[i]);
	}

	WinKeySeqFrag & operator= (WinKeySeqFrag const & other) {
		w = other.w;
		seqbufs = other.seqbufs; /* copy vector */
		/* do nothing with mtx */
		return *this;
	}
};

#endif //CREEK_WINKEYSEQFRAG_H
