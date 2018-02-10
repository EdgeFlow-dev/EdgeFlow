//
// Created by xzl on 8/22/17.
//

#ifndef CREEK_SEQBUF_H
#define CREEK_SEQBUF_H

#include <memory>	/* shared_ptr */
#include <vector>
#include <algorithm>
#include <iostream>

//#include <arm_fp16.h> /* for SumCount trick */

#include "log.h"
#include "Record.h"

#include <string>
extern std::string Backtrace(int skip = 1);


#ifdef USE_UARRAY
extern "C" {
#include "xplane_lib.h"
}
#endif

//#include "WinKeySeqFrag.h"
#include <map>

using namespace std;

#include "Window.h"
#include "linux-sizes.h"

/*
 * forward decl of KeyFrag, to resolve circular dep
 */
template <class KVPair, unsigned N>
struct WinKeySeqFrag;


#define WARN_UNUSED __attribute__((warn_unused_result))
//#define WARN_UNUSED

/* compared based on the k of KV records */
template<class KVRecord>
struct KVRecordCompLess {
	bool operator()(const KVRecord & lhs,
									const KVRecord & rhs) const {
		return lhs.data.first < rhs.data.first;
	}
};

/* comp based on a key field*/
template<class K2VRecord, int keypos>
struct K2VCompLess {
	bool operator()(const K2VRecord & lhs,
									const K2VRecord & rhs) const {
		return std::get<keypos>(lhs.data) < std::get<keypos>(rhs.data);
	}
};

#ifdef USE_UARRAY
/* using shared ptr to pointing to this */
struct sbatch {
	x_addr xaddr;

	sbatch() : xaddr(x_addr_null){ }

	sbatch(const sbatch & that) = delete;

	/* from a ns raw buf */
	sbatch(uint32_t *ns, uint32_t len) {
#define MAX_NSBUF_LEN	(SZ_2M)  /* in simd_t */

		/* NB: @size is bound by the nw/tz shared mem size. we have to create small nsbufs in tz and concatenate them */

		uint32_t remaining = len;
		xzl_bug_on(len == 0 || !ns);

		W("to load %u bytes to tz...", std::min(remaining, (uint32_t)MAX_NSBUF_LEN));

		xaddr = create_uarray_nsbuf(ns, std::min(remaining, (uint32_t)MAX_NSBUF_LEN));
		remaining -= std::min(remaining, (uint32_t)MAX_NSBUF_LEN);
		xzl_bug_on_msg(!xaddr, "create_uarray_nsbuf returns nullptr");

		int32_t ret;
		while (remaining) {

			W("to load %u bytes to tz...", std::min(remaining, (uint32_t)MAX_NSBUF_LEN));

			x_addr tmp = create_uarray_nsbuf(ns + (len - remaining), std::min(remaining, (uint32_t)MAX_NSBUF_LEN));
			xzl_bug_on_msg(!tmp, "create_uarray_nsbuf returns nullptr");
			remaining -= std::min(remaining, (uint32_t)MAX_NSBUF_LEN);

			ret = concatenate(xaddr, tmp);
			xzl_bug_on_msg(ret < 0, "concatenate failed");

			ret = retire_uarray(tmp);
			xzl_bug_on_msg(ret < 0, "retire uarray failed");
		}
	}

	~sbatch() {
		if (xaddr) {
			I("to retire uarray %lx", xaddr);
//			cout << Backtrace(2) << endl;

			retire_uarray(xaddr);
			xaddr = x_addr_null;
		}
	}
};

template<class T>
idx_t const tspos() { return sizeof(typename T::ElementT) / sizeof(simd_t); }

//template<Record<creek::kvpair>>
//idx_t get_tspos() { return sizeof(creek::kvpair) / sizeof(simd_t); }
//
//template<Record<creek::k2v>>
//idx_t get_tspos() { return sizeof(creek::k2v) / sizeof(simd_t); }

template<class T>
idx_t const reclen() { return sizeof(T) / sizeof(simd_t); }



#endif

namespace creek {
	enum OpMode {
		/* --- perkey, 1x sort --- */
		SB_SUMCOUNT1_PERKEY,
		SB_SUMCOUNT_PERKEY,
		SB_AVG_PERKEY,
		SB_UNIQUE_PERKEY,
		SB_SUM_PERKEY,
		/* --- perkey, 2x sort --- */
		SB_MED_PERKEY,
		SB_TOPK_PERKEY,
		SB_PERCENTILE_PERKEY,
		/* --- all --- */
		SB_SUMCOUNT1_ALL,
		SB_SUMCOUNT_ALL,
		SB_AVG_ALL,
	};
};

//static uint32_t* rec_get_count(Record<creek::k2v> & t) {
//	return (uint32_t *)(&t.ts);
//}

using namespace creek;

/* seqbuf size */
#define SEQBUFSZ_0 		0
#define SEQBUFSZ_1 		1
#define SEQBUFSZ_SOME 2 /* > 0 */

/* wrap around immutable seq buffers. copy of the object should be cheap -- only copies
 * the references. */

/* A pseudo implementation of SeqBuf. It uses vector underneath. */
template <typename T>
struct SeqBuf {

	/*
	 * ctors
	 */

	/* default ctor: no underlying buffer */
	SeqBuf() {
		pbuf = nullptr;
	}

	// copy ctor -- just copy the ptr/ref, not the actual content.
	SeqBuf(const SeqBuf & other) {
		pbuf = other.pbuf;
		sorted_ = other.sorted_;
#ifdef USE_UARRAY
		size_ = other.Size();
#endif
	}

	// ctor -- from raw data (for testing only)
	/// @len: # of T
	SeqBuf(T const * raw, unsigned len) {
#ifdef USE_UARRAY
		static_assert(sizeof(T) >= sizeof(simd_t));
		pbuf = make_shared<sbatch>((uint32_t *)raw, len * reclen<T>());
//		size_ = len;
		size_ = SEQBUFSZ_SOME;
#else
		pbuf = make_shared<vector<T>>(raw, raw + len);
#endif
	}

	/* force allocating a new underlying buf. fail if there's already one.
	 * TZ: won't allocate sec buf*/
	bool AllocBuf() {
		if (pbuf) {
			xzl_bug("try to reallocate pbuf");
			return false;
		}
#ifdef USE_UARRAY
		pbuf = make_shared<sbatch>(); // this won't go to tz to allocate actual buf
#else
		pbuf = make_shared<vector<T>>();
#endif
		return true;
	}

	size_t Size() const {
		if (!pbuf)
			return 0;
		else
#ifdef USE_UARRAY
			return size_;
//			return 2; // we don't know what size is...
#else
			return pbuf->size();
#endif
	}

	/*
	 * operators
	 */

#if 0
	// OBSOLETED see below
	// will create another buf as return value
	// wil need specialization if comp function is not generic/default
	SeqBuf Merge(SeqBuf const & other) {
		SeqBuf res;
		res.AllocBuf();

		xzl_bug_on_msg(!IsSortedSame(*this, other), "bug: try to merge unsorted seqbufs");

		std::merge(pbuf->begin(), pbuf->end(), other.pbuf->begin(), other.pbuf->end(),
							 std::back_inserter(*(res.pbuf)));  // xzl: must use this
		res.sorted_ = 0;

		return res;
	}
#endif

	/* specify where to find the merge key.
	 * NB: calling this when T!=tuple or kvpair will fail...*/
	template <int keypos>
	SeqBuf MergeByKey(SeqBuf const &other) const {
		SeqBuf res;

		res.AllocBuf();

		xzl_bug_on_msg(!IsSortedSame(*this, other), "bug: try to merge seqbufs sorted in diff ways");
		if (this->IsSorted() && this->Size())
			xzl_bug_on_msg(this->SortedKey() != keypos, "bug: sort/merge on diff keys");
		if (other.IsSorted() && other.Size())
			xzl_bug_on_msg(other.SortedKey() != keypos, "bug: sort/merge on diff keys");


#ifdef USE_UARRAY
		/* possible if one sbatch, or both, is not allocated (e.g. merge multiple empty partitions) */
		if (!this->pbuf->xaddr && !other.pbuf->xaddr) {
//			cout << Backtrace() << endl;
			return res; // res's xaddr = 0
		} else if (!this->pbuf->xaddr)
//			res.pbuf->xaddr = other.pbuf->xaddr;
			res.pbuf = other.pbuf;
		else if (!other.pbuf->xaddr)
//			res.pbuf->xaddr = this->pbuf->xaddr;
			res.pbuf = this->pbuf;
		else { /* both non null, do actual merge */
			auto ret = merge(&res.pbuf->xaddr, 1 /* #outputs */, this->pbuf->xaddr, other.pbuf->xaddr, keypos, reclen<T>());
			xzl_bug_on(ret < 0);
			xzl_bug_on_msg(!res.pbuf->xaddr, "merge return xaddr (0)?");
			I("merge ret new sbatch xaddr %lx", res.pbuf->xaddr);
		}

		res.size_ = SEQBUFSZ_SOME;
#else
		std::merge(pbuf->begin(), pbuf->end(), other.pbuf->begin(), other.pbuf->end(),
							 back_inserter(*(res.pbuf)),   /* must use this. otherwise no enough elements in dst */
							 K2VCompLess<T, keypos>());
#endif

		res.sorted_ = keypos;


		return res;
	}

	/* Simply append @other into this seqbuf.
	 * Can be unsorted.
	 **/
	void Concatenate(SeqBuf const &other) {
		xzl_bug_on(!this->pbuf || !other.pbuf);

#ifdef USE_UARRAY
//		xzl_bug_on(!(this->pbuf->xaddr && other.pbuf->xaddr));
		xzl_bug_on(!(this->size_ || other.size_)); /* at least has some items */

		if (!this->pbuf->xaddr) { /* this seqbuf is null */
			xzl_bug_on(!other.pbuf->xaddr);
			this->pbuf = other.pbuf; /* move over */
			this->size_ = other.size_;
			this->sorted_ = other.sorted_;
		} else { /* this seqbuf is not null */
			if (other.pbuf->xaddr) {
				auto ret = concatenate(pbuf->xaddr, other.pbuf->xaddr);
				if (ret < 0)
					EE("concatenate returns %d. input %lx %lx", ret, pbuf->xaddr, other.pbuf->xaddr);
				xzl_bug_on(ret < 0);
				this->size_ = SEQBUFSZ_SOME;
			} else { /* other is null */
				/* do nothing */
			}
		}
#else
		pbuf->insert(pbuf->end(), other.pbuf->begin(), other.pbuf->end());
#endif

		if (other.Size())
			this->sorted_ = -1; /* becomes unsorted */
	}

	/* drop records that only exist on one side. */
	template <int keypos>
	SeqBuf JoinByKey(SeqBuf const &other) const {

		SeqBuf res;
		res.AllocBuf();

		xzl_bug_on_msg(!IsSortedSame(*this, other), "bug: try to join seqbufs sorted in diff ways");
		if (this->IsSorted())
			xzl_bug_on_msg(this->SortedKey() != keypos, "bug: sort/join on diff keys");
		if (other.IsSorted())
			xzl_bug_on_msg(other.SortedKey() != keypos, "bug: sort/join on diff keys");

#ifdef USE_UARRAY
		xzl_assert(this->pbuf->xaddr && other.pbuf->xaddr);
		xzl_assert(this->size_ || other.size_); /* at least has some items */
		auto ret = join_bykey(&res.pbuf->xaddr, pbuf->xaddr, other.pbuf->xaddr, keypos, reclen<T>());
		xzl_bug_on(ret < 0);
		xzl_bug_on(!res.pbuf->xaddr);
		res.size_ = SEQBUFSZ_SOME; /* what if both empty? */
#else
		std::set_intersection(pbuf->begin(), pbuf->end(),
		other.pbuf->begin(), other.pbuf->end(),
							 back_inserter(*(res.pbuf)),   /* must use this. otherwise no enough elements in dst */
							 K2VCompLess<T, keypos>());
#endif
		res.sorted_ = keypos;

		return res;
	}

	/* @other: the seqbuf contains only one tuple, in which @vpos is the threshold
	 * return: the tuples from @this seqbuf, where value < threshold.
	 *
	 * this has nothing to do with key. so @this can be unsorted.
	 */
	template <int vpos>
	SeqBuf JoinByFilter(SeqBuf const &other) const {
		xzl_bug_on_msg(other.Size() != 1, "filter has !=1 records");

		SeqBuf res;
		res.AllocBuf();

#ifdef USE_UARRAY
		xzl_assert(other.pbuf->xaddr && other.size_);
		if (!this->pbuf->xaddr) { /* this seqbuf is null */
			; /* do no thing */
		} else {
			auto ret = join_byfilter(&res.pbuf->xaddr, pbuf->xaddr, other.pbuf->xaddr, vpos, reclen<T>());
			if (ret < 0)
				EE("join_byfilter returns %d. input %lx %lx. vpos %d reclen %d",
				ret, pbuf->xaddr, other.pbuf->xaddr, vpos, reclen<T>());
			xzl_bug_on(ret < 0);
			xzl_bug_on(!res.pbuf->xaddr);
			res.size_ = SEQBUFSZ_SOME;
		}
#else
		auto & th = std::get<vpos>(other.pbuf->at(0).data); /* the only tuple in @other */
		for (auto & r : *pbuf) {
			if (std::get<vpos>(r.data) > th) {
				r.ts = th; /* for debugging - encode threshold in ts (unused) */
				res.pbuf->push_back(r); /* copy over */
			}
		}
		EE("input %lu elements th %u return %lu elements", this->Size(), th, res.Size());
#endif
		return res;
	}

	// NOT very useful. see SumCountXXX
	// Collapse a seqbuf into one tuple
	// Although the result is one T, we still return SeqBuf<T> to simplify type systems:
	// e.g. although we don't need the final result to carry timestamps.
#ifdef USE_UARRAY
	template <int vpos>
	SeqBuf<T> SumAllToOne(void) const {
		xzl_bug("tbd");
	}
#else
template <int vpos>
SeqBuf<T> SumAllToOne(void) const {
	T res;
	SeqBuf sb;
	creek::vtype v = 0;
	sb.AllocBuf();

	if (!this->pbuf->size())
		return sb; 		// nothing. we still return with pbuf allocated (XXX overkill XXX)

	for (auto & rec : *(this->pbuf)) {
		auto &vv = std::get<vpos>(rec.data);
		v += vv;
	}

	/* emit the tuple */
		std::get<vpos>(res.data) = v;
		sb.pbuf->push_back(res);

		return sb;
	}
#endif


	/* Run summation over the values (at @vpos) of the same key (at @keypos).
	 * Emit <key, ... sum, ...> tuples.
	 * NB: other elements in the output tuples are UNDEFINED.
	 * Seqbuf must be sorted.
	 *
	 * @n: # output seqbufs */
#ifdef USE_UARRAY
	/* see SumCountXXX() */
#else
	template <int keypos, int vpos>
	vector<SeqBuf<T>> SumByKey(unsigned n_output) const {
		xzl_bug_on_msg(this->Size() && this->SortedKey() != keypos, "bug: unsorted or sorted on a diff key");

		vector<SeqBuf<T>> res(n_output);
		creek::ktype k;
		creek::vtype v;
		int outi = 0;

		for (auto & r : res) {
			r.AllocBuf();
			r.sorted_ = keypos;
		}

		if (!this->pbuf->size())
			return res; 		// nothing

		k = std::get<keypos>(this->pbuf->at(0).data);
		v = std::get<vpos>(this->pbuf->at(0).data);

		for (auto & rec : *(this->pbuf)) {
			auto & kk = std::get<keypos>(rec.data);
			auto & vv = std::get<vpos>(rec.data);

			if (kk != k) { /* emit a record of k2v. round robin to output seqbufs */
				T t; /* a Record type. NB: record ts, and k2 in tuple UNDEFINED */
				std::get<keypos>(t.data) = k;
				std::get<vpos>(t.data) = v;
				res[outi].pbuf->push_back(t);
				outi = (outi + 1) % n_output;
				k = kk;
				v = 0;
			}
			v += vv;
		}

		/* emit last tuple */
		T t;
		std::get<keypos>(t.data) = k;
		std::get<vpos>(t.data) = v;
		res[outi].pbuf->push_back(t);

		return res;
	};
#endif /* see above */

	/* Scan the sorted seqbuf. output the median v for each k
	 *
	 * Assume whole seqbuf is already sorted by k then v.
	 *
	 * XXX: since this random access the input seqbuf, it needs extra work to factor into ScanSeqBuf() */
#ifdef USE_UARRAY
	template <int keypos, int vpos>
	vector<SeqBuf<T>> MedByKey(unsigned n_outputs, OpMode mode, float k) const {
		xzl_bug_on_msg(this->Size() && this->SortedKey() != keypos,
									 "bug: unsorted or sorted on a diff key");

		vector<SeqBuf<T>> res(n_outputs);
		x_addr dests[n_outputs];

		for (auto & r : res) {
			r.AllocBuf();
			r.sorted_ = keypos;
		}

		if (!this->Size())
			return res; 		// nothing

		xzl_assert(pbuf->xaddr);
		int32_t ret;
		switch (mode) {
			case SB_MED_PERKEY:  /* todo: combine these two */
				ret = med_bykey(dests, n_outputs, pbuf->xaddr, keypos, vpos, reclen<T>());\
				break;
			case SB_PERCENTILE_PERKEY:
				xzl_bug("todo");
				break;
			case SB_TOPK_PERKEY:
				xzl_bug_on(k <= 0 || k >= 1);
				ret = topk_bykey(dests, n_outputs, pbuf->xaddr, keypos, vpos, reclen<T>(), k, true /* reverse */);
				break;
			default:
				xzl_bug("unknown mode");
				break;
		}
		xzl_bug_on(ret < 0 || !dests[0]);

		for (unsigned i = 0; i < n_outputs; i++) {
			res[i].pbuf->xaddr = dests[i];
			res[i].size_ = SEQBUFSZ_SOME;
		}

		return res;
	};
#else // USE_UARRAY
	template <int keypos, int vpos>
	vector<SeqBuf<T>> MedByKey(unsigned n_output) const {

		xzl_bug_on_msg(this->Size() && this->SortedKey() != keypos,
		"bug: unsorted or sorted on a diff key");

		vector<SeqBuf<T>> res(n_output);
		creek::ktype k;
//		creek::vtype v;
		unsigned start_idx = 0, midx; // start/median idx of the current key range
		int outi = 0;

		for (auto & r : res) {
			r.AllocBuf();
			r.sorted_ = keypos;
		}

		if (!this->pbuf->size())
			return res; 		// nothing

		k = std::get<keypos>(this->pbuf->at(0).data);
//		v = std::get<vpos>(this->pbuf->at(0).data);

		for (unsigned ii = 0; ii < this->pbuf->size(); ii++) {
			auto & rec = this->pbuf->at(ii);
			auto & kk = std::get<keypos>(rec.data);

			if (kk != k) { /* emit a record of k2v. round robin to output seqbufs */
				T t; /* a Record type. NB: record ts, and k2 in tuple UNDEFINED */
				midx = (ii + start_idx) / 2;
				std::get<keypos>(t.data) = k;
				std::get<vpos>(t.data) = std::get<vpos>(pbuf->at(midx).data);
				res[outi].pbuf->push_back(t);
				outi = (outi + 1) % n_output;
				k = kk;
				start_idx = ii;
			}
		}

		/* emit last tuple */
		T t;
		std::get<keypos>(t.data) = k;
		midx = (this->pbuf->size() + start_idx) / 2; // this wont go out of bound...
		std::get<vpos>(t.data) = std::get<vpos>(pbuf->at(midx).data);
		res[outi].pbuf->push_back(t);

		return res;
	};
#endif // USE_UARRAY

	template <int keypos, int vpos>
	SeqBuf<T> MedByKey_1_OutBatch(OpMode mode, float k) {
		auto res = this->MedByKey<keypos, vpos>(1, mode, k);
		return res[0];
	};

#if 0
private:
	/*
	 * +--- count (uint16) ---+--- sum (float16) ---+
	 *
	 * encode two into one */
	static uint32_t  u16f16_to_u32_(uint16_t count, float16_t sum) {
		uint16_t *p = (uint16_t *)(&sum);
		uint32_t ret = ((count << 16) | (*p));
		return ret;
	}

	/* decode */
	static void u32_to_u16f16_(uint16_t* count, float16_t* sum, uint32_t t) {
		uint16_t  tt;
		*count = (0xffff & (t >> 16));
		tt = (0xffff & t);
		*sum = *(float16_t *)(&tt);
	}

public:
	/* __fp16 only works on ARM */
	template <int keypos, int vpos>
	vector<SeqBuf<T>> SumCount(unsigned n_output,
		bool inv32, bool outv32) const {

		static_assert(sizeof(creek::vtype) >= 4);  /* due to our tricks.. */

		xzl_bug_on_msg(this->Size() && this->SortedKey() != keypos,
		"bug: unsorted or sorted on a diff key");

		vector<SeqBuf<T>> res(n_output);
		creek::ktype k;
		creek::vtype sum32;
		float16_t sum16;  /* only support by aarch64 */
		uint16_t cnt16;

		int outi = 0;

		for (auto & r : res) {
			r.AllocBuf();
			r.sorted_ = keypos;
		}

		if (!this->pbuf->size())
			return res; 		// nothing

		k = std::get<keypos>(this->pbuf->at(0).data);
		sum32 = std::get<vpos>(this->pbuf->at(0).data);
		if (inv32) {
			u32_to_u16f16_(&cnt16, &sum16, sum32);
		}

		for (auto & rec : *(this->pbuf)) {
			auto & kk = std::get<keypos>(rec.data);
			auto & vv = std::get<vpos>(rec.data);

			if (kk != k) { /* emit a record of k2v. round robin to output seqbufs */
				T t; /* a Record type. NB: record ts, and k2 in tuple UNDEFINED */
				std::get<keypos>(t.data) = k;
				if (outv32)
					std::get<vpos>(t.data) = sum32;
				else
					std::get<vpos>(t.data) = u16f16_to_u32_(cnt16, sum16);
				res[outi].pbuf->push_back(t);
				outi = (outi + 1) % n_output;
				k = kk;
				v = 0;
			}
			xzl_bug_on_msg(cnt == USHRT_MAX, "bug: cnt overflow");
			cnt ++;
			v += (float16_t)vv;
		}

		/* emit last tuple */
		T t;
		std::get<keypos>(t.data) = k;
		std::get<vpos>(t.data) = v;
		res[outi].pbuf->push_back(t);

		return res;
	};
#endif

	/*
	 * repurpose ts as the count. since we won't need record's ts after gbk.
	 * for sum, support 3 modes:
	 * 	1. input kvpair: disregard ts; treat count as 1; output kvpair: @ts as count
	 * 	2. input kvpair: @ts as count; output: ts as count
	 *  3. input kvpair: @ts as count; output: avg (sum/count)
	 */

private:
#if 0 /* obsoleted? */
	uint32_t & get_count(Record<T> & t) const {
		return (uint32_t)t.ts;
	}
#endif

#ifndef USE_UARRAY
	struct scan_state {
		creek::ktype k;
		creek::vtype v;
		uint32_t count;
	};

	template<int keypos, int vpos>
	struct OpSumCount1_PerKey {
		static void CheckSorted(SeqBuf const & s) {
			xzl_bug_on_msg(s.Size() && s.SortedKey() != keypos,
										 "bug: unsorted or sorted on a diff key");
		}

		static void SetSortKey(SeqBuf* p) { p->sorted_ = keypos; }

		static bool IsKeyDiff(scan_state const & state, T const & newrec) {
			return (state.k != std::get<keypos>(newrec.data));
		}

		static void EmitRecord(T* rec, scan_state const & state) {
			std::get<keypos>(rec->data) = state.k;
			std::get<vpos>(rec->data) = state.v;
			rec->ts = state.count;
		}

		// Update scan state
		static void OnNewKey(T const & rec, scan_state * state) {
			state->count = 1;
			state->k = std::get<keypos>(rec.data);
			state->v = std::get<vpos>(rec.data);
		}

		static void OnSameKey(T const & rec, scan_state * state) {
			state->count += 1;
			state->v += std::get<vpos>(rec.data);
		}
	};

	template<int keypos, int vpos>
	struct OpSumCount_PerKey {
		static void CheckSorted(SeqBuf const & s) {
			xzl_bug_on_msg(s.Size() && s.SortedKey() != keypos,
										 "bug: unsorted or sorted on a diff key");
		}

		static void SetSortKey(SeqBuf* p) { p->sorted_ = keypos; }

		static bool IsKeyDiff(scan_state const & state, T const & newrec) {
			return (state.k != std::get<keypos>(newrec.data));
		}

		static void EmitRecord(T* rec, scan_state const & state) {
			std::get<keypos>(rec->data) = state.k;
			std::get<vpos>(rec->data) = state.v;
			rec->ts = state.count;
		}

		// Update scan state
		static void OnNewKey(T const & rec, scan_state * state) {
			state->count = rec.ts;
			state->k = std::get<keypos>(rec.data);
			state->v = std::get<vpos>(rec.data);
		}

		static void OnSameKey(T const & rec, scan_state * state) {
			state->count += rec.ts;
			state->v += std::get<vpos>(rec.data);
		}
	};

	template<int keypos, int vpos>
	struct OpAvg_PerKey {
		static void CheckSorted(SeqBuf const & s) {
			xzl_bug_on_msg(s.Size() && s.SortedKey() != keypos,
										 "bug: unsorted or sorted on a diff key");
		}

		static void SetSortKey(SeqBuf* p) { p->sorted_ = keypos; }

		static bool IsKeyDiff(scan_state const & state, T const & newrec) {
			return (state.k != std::get<keypos>(newrec.data));
		}

		static void EmitRecord(T* rec, scan_state const & state) {
			std::get<keypos>(rec->data) = state.k;
			std::get<vpos>(rec->data) = state.v / state.count;
//			rec->ts = state.count;  // no needed. useful for debugging
		}

		// Update scan state
		static void OnNewKey(T const & rec, scan_state * state) {
			state->count = rec.ts;
			state->k = std::get<keypos>(rec.data);
			state->v = std::get<vpos>(rec.data);
		}

		static void OnSameKey(T const & rec, scan_state * state) {
			state->count += rec.ts;
			state->v += std::get<vpos>(rec.data);
		}
	};

	/* ---------------------------------------------------------------- */

	template<int vpos>
	struct OpSumCount1_All {
		static void CheckSorted(SeqBuf const & s) { /* nothing */ }

		static void SetSortKey(SeqBuf* p) { p->sorted_ = -1; }

		static bool IsKeyDiff(scan_state const & state, T const & newrec) {
			return false;
		}

		static void EmitRecord(T* rec, scan_state const & state) {
//			std::get<keypos>(rec->data) = state.k;
			std::get<vpos>(rec->data) = state.v;
			rec->ts = state.count;
		}

		// Update scan state
		static void OnNewKey(T const & rec, scan_state * state) {
			state->count = 1;
//			state->k = std::get<keypos>(rec.data);
			state->v = std::get<vpos>(rec.data);
		}

		static void OnSameKey(T const & rec, scan_state * state) {
			state->count += 1;
			state->v += std::get<vpos>(rec.data);
		}
	};

	template<int vpos>
	struct OpSumCount_All {
		static void CheckSorted(SeqBuf const & s) { /* nothing */ }

		static void SetSortKey(SeqBuf* p) { p->sorted_ = -1; }

		static bool IsKeyDiff(scan_state const & state, T const & newrec) {
			return false;
		}

		static void EmitRecord(T* rec, scan_state const & state) {
//			std::get<keypos>(rec->data) = state.k;
			std::get<vpos>(rec->data) = state.v;
			rec->ts = state.count;
		}

		// Update scan state
		static void OnNewKey(T const & rec, scan_state * state) {
			state->count = rec.ts;
//			state->k = std::get<keypos>(rec.data);
			state->v = std::get<vpos>(rec.data);
		}

		static void OnSameKey(T const & rec, scan_state * state) {
			state->count += rec.ts;
			state->v += std::get<vpos>(rec.data);
		}
	};

	template<int vpos>
	struct OpAvg_All {
		static void CheckSorted(SeqBuf const & s) { /* nothing */ }

		static void SetSortKey(SeqBuf* p) { p->sorted_ = -1; }

		static bool IsKeyDiff(scan_state const & state, T const & newrec) {
			return false;  // treat key always the same
		}

		static void EmitRecord(T* rec, scan_state const & state) {
//			std::get<keypos>(rec->data) = state.k;
			std::get<vpos>(rec->data) = state.v / state.count;
//			rec->ts = count;  // needed?
		}

		// Update scan state
		static void OnNewKey(T const & rec, scan_state * state) {
			state->count = rec.ts;
//			state->k = std::get<keypos>(rec.data);
			state->v = std::get<vpos>(rec.data);
		}

		static void OnSameKey(T const & rec, scan_state * state) {
			state->count += rec.ts;
			state->v += std::get<vpos>(rec.data);
		}
	};

	/* ---------------------------------------------------------------- */

	template<int keypos, int vpos>
	struct OpMedV_PerKey {
		static void CheckSorted(SeqBuf const & s) {
			xzl_bug_on_msg(s.Size() && s.SortedKey() != keypos,
										 "bug: unsorted or sorted on a diff key");
		}

		static void SetSortKey(SeqBuf* p) { p->sorted_ = keypos; }

		static bool IsKeyDiff(scan_state const & state, T const & newrec) {
			return (state.k != std::get<keypos>(newrec.data));
		}

		static void EmitRecord(T* rec, scan_state const & state) {
			std::get<keypos>(rec->data) = state.k;
			std::get<vpos>(rec->data) = state.v / state.count;
//			rec->ts = state.count;  // no needed. useful for debugging
		}

		// Update scan state
		static void OnNewKey(T const & rec, scan_state * state) {
			state->count = rec.ts;
			state->k = std::get<keypos>(rec.data);
			state->v = std::get<vpos>(rec.data);
		}

		static void OnSameKey(T const & rec, scan_state * state) {
			state->count += rec.ts;
			state->v += std::get<vpos>(rec.data);
		}
	};

	/* For average. Sum values and count per key. This repurposes ts for saving @count.
	 * This supports mutiple modes.
	 *
	 * XXX make this template (mode = template arg) */
	template <int keypos, int vpos, class ops>
	vector<SeqBuf<T>> ScanSeqBuf(unsigned n_output) const {

		static_assert(sizeof(creek::etime) >= 4);  /* due to our tricks.. */

		ops::CheckSorted(*this);

		vector<SeqBuf<T>> res(n_output);
		scan_state state;

		int outi = 0;

		for (auto & r : res) {
			r.AllocBuf();
			ops::SetSortKey(&r);
		}

		if (!this->pbuf->size())
			return res; 		// nothing. note that seqbufs@ still have backing buffers

		auto & rec = this->pbuf->at(0);
		ops::OnNewKey(rec, &state);

		for (auto & rec : *(this->pbuf)) {

			if (ops::IsKeyDiff(state, rec)) {
				T t;
				/* a Record type. NB: record ts, and k2 in tuple UNDEFINED */
				/* emit a record of k2v.
				 * round robin to output seqbufs */
				ops::EmitRecord(&t, state);
				res[outi].pbuf->push_back(t);
				outi = (outi + 1) % n_output;

				ops::OnNewKey(rec, &state);
			} else
				ops::OnSameKey(rec, &state);
		}

		/* emit last tuple */
		T t;
		ops::EmitRecord(&t, state);
		res[outi].pbuf->push_back(t);

		return res;
	};
#endif // !USE_UARRAY

public:

	template <int keypos, int vpos>
	vector<SeqBuf<T>> SumCount(unsigned n_outputs, OpMode mode) const {
#ifdef USE_UARRAY
		x_addr dests[n_outputs] = {0};
		int32_t ret = 0;

		vector<SeqBuf<T>> res(n_outputs);

		xzl_assert(pbuf);

		for (unsigned i = 0; i < n_outputs; i++) {
			res[i].AllocBuf();
		}

		if (!this->pbuf->xaddr) /* this is okay (e.g. when closing empty windows). just return empty seqbufs */
			return res;

		/* pre ops: check sorted. alloc buf so that in case nothing to compute, we still return empty seqbuf */
		switch (mode) {
			case SB_SUMCOUNT1_PERKEY:
			case SB_SUMCOUNT_PERKEY:
			case SB_AVG_PERKEY:
			case SB_UNIQUE_PERKEY:
			case SB_SUM_PERKEY:
				xzl_bug_on_msg(SortedKey() != keypos, "bug: unsorted or sorted on a diff key");
				break;
			case SB_SUMCOUNT1_ALL:
			case SB_SUMCOUNT_ALL:
			case SB_AVG_ALL:
				xzl_bug_on(n_outputs != 1);
				break;
			default:
				break; /* do nothing */
		}

		switch (mode) {
			/* per key version */
			case SB_SUMCOUNT1_PERKEY:
				ret = sumcount1_perkey(dests, n_outputs, pbuf->xaddr, keypos, vpos, tspos<T>(), reclen<T>());
				break;
			case SB_SUMCOUNT_PERKEY:
				ret = sumcount_perkey(dests, n_outputs, pbuf->xaddr, keypos, vpos, tspos<T>(), reclen<T>());
				break;
			case SB_AVG_PERKEY:
				ret = avg_perkey(dests, n_outputs, pbuf->xaddr, keypos, vpos, tspos<T>(), reclen<T>());
				break;
			case SB_UNIQUE_PERKEY:
				xzl_bug_on(n_outputs != 1); /* unsupported */
				ret = unique_key(dests, pbuf->xaddr, keypos, reclen<T>());
				break;
			case SB_SUM_PERKEY:
				ret = sum_perkey(dests, n_outputs, pbuf->xaddr, keypos, vpos, reclen<T>());
				break;
			/* ---- *All version --- */
			case SB_SUMCOUNT1_ALL:
				ret = sumcount1_all(dests, pbuf->xaddr, keypos, vpos, tspos<T>(), reclen<T>());
				break;
			case SB_SUMCOUNT_ALL:
				ret = sumcount_all(dests, pbuf->xaddr, keypos, vpos, tspos<T>(), reclen<T>());
				break;
			case SB_AVG_ALL:
				ret = avg_all(dests, pbuf->xaddr, keypos, vpos, tspos<T>(), reclen<T>());
				break;
			default:
				xzl_bug("unknown mode?");
				break;
		}

		xzl_bug_on(ret < 0);

		/* post ops: set sorted key & return results...*/
		switch (mode) {
			case SB_SUMCOUNT1_PERKEY:
			case SB_SUMCOUNT_PERKEY:
			case SB_AVG_PERKEY:
			case SB_SUM_PERKEY:
			case SB_UNIQUE_PERKEY:
				for (unsigned i = 0; i < n_outputs; i++) {
					xzl_assert(dests[i]);
					res[i].pbuf->xaddr = dests[i];
					res[i].sorted_ = keypos;
					res[i].size_ = SEQBUFSZ_SOME;
					I("sumcount ret %u: new sbatch xaddr %lx", i, dests[i]);
				}
				break;
			case SB_SUMCOUNT1_ALL:
			case SB_SUMCOUNT_ALL:
			case SB_AVG_ALL:
				xzl_assert(dests[0]);
				res[0].pbuf->xaddr = dests[0];
				res[0].sorted_ = -1;
				res[0].size_ = SEQBUFSZ_1;
				I("sumcount ret new sbatch xaddr %lx", dests[0]);
				break;
			default:
				xzl_bug("unknown mode?");
				break;
		}

		return  res;
#else
		switch (mode) {
			case SB_SUMCOUNT1_PERKEY:
				return ScanSeqBuf<keypos, vpos, OpSumCount1_PerKey<keypos,vpos>>(n_outputs);
			case SB_SUMCOUNT_PERKEY:
				return ScanSeqBuf<keypos, vpos, OpSumCount_PerKey<keypos,vpos>>(n_outputs);
			case SB_AVG_PERKEY:
				return ScanSeqBuf<keypos, vpos, OpAvg_PerKey<keypos,vpos>>(n_outputs);
			case SB_SUMCOUNT1_ALL:
				return ScanSeqBuf<keypos, vpos, OpSumCount1_All<vpos>>(n_outputs);
			case SB_SUMCOUNT_ALL:
				return ScanSeqBuf<keypos, vpos, OpSumCount_All<vpos>>(n_outputs);
			case SB_AVG_ALL:
				return ScanSeqBuf<keypos, vpos, OpAvg_All<vpos>>(n_outputs);
			default:
				xzl_bug("unknown mode?");
				break;
		}
#endif
	};

	/* output to one seqbuf only. so the return value does not have to be a vec */
	template <int keypos, int vpos>
	SeqBuf<T> SumCount_1_OutBatch(OpMode mode) const {
		VV("starts");
		auto res = this->SumCount<keypos, vpos>(1, mode);
		VV("ends");
		// the following is okay when input xaddr is 0
//		xzl_assert(res[0].pbuf->xaddr);
		return res[0];
	};

#ifndef USE_UARRAY
	// force copying the underlying buffers. Drop any previous contents.
	// the underlying buf must be explicitly allocated already.
	bool CopyFrom(SeqBuf & other) {
		if (!pbuf) {
			xzl_bug("must allocated pbuf first");
			return false;
		}
		pbuf->erase(pbuf->begin(), pbuf->end());
		pbuf->insert(pbuf->begin(), other.pbuf->begin(), other.pbuf->end());

		sorted_ = other.sorted_;

		return true;
	}
#endif

	/* decl only */
//	template <typename VecRet>
//	VecRet Segment(creek::etime, creek::etime);

	map<Window, WinKeySeqFrag<typename T::ElementT, 1/*N*/>, Window>
	Segment(creek::etime base, creek::etime subrange)
	{
		std::map<Window, WinKeySeqFrag<typename T::ElementT, 1/*N*/>, Window> m;
		/* seg into map */

#ifdef USE_UARRAY
		x_addr *dests;
		xscalar_t *seg_bases;
		uint32_t n_segs;

		VV("base %u subsrange %u tspos %u reclen %u", base, subrange, tspos<T>(), reclen<T>());

		auto ret = segment(&dests, &seg_bases, &n_segs, 1/*#outputs*/, pbuf->xaddr, base, subrange, tspos<T>(), reclen<T>());
		xzl_bug_on(ret < 0);
		xzl_bug_on_msg((!dests || !seg_bases), "bug: API mem alloc failed?");

		VV("segment: input xaddr %lx sz %d (in simd_t), return nsegs = %u", pbuf->xaddr, uarray_size(pbuf->xaddr), n_segs);

		xzl_assert(seg_bases);

		for (unsigned i = 0; i < n_segs; i++) {
			I("segment %u, base = %lu, new sbatch xaddr %lx, sz = %d (in simd_t).", i,  seg_bases[i], dests[i], uarray_size(dests[i]));
			Window w(seg_bases[i] /* window start */, subrange);
			if (m.find(w) == m.end()) {
				m[w] = WinKeySeqFrag<typename T::ElementT, 1/*N*/>(w); /* default has 1 seqbuf */
				m[w].seqbufs[0].AllocBuf();  /* alloc underlying buffer */
			}
			xzl_assert(dests[i]);
			m[w].seqbufs[0].pbuf->xaddr = dests[i];
			m[w].seqbufs[0].size_ = SEQBUFSZ_SOME;
		}

		free(dests);
		free(seg_bases);
#else
	for (auto & x : *pbuf) {
		xzl_bug_on(x.ts < base);
		Window w(x.ts - ((x.ts - base) % subrange) /* window start */, subrange);
		if (m.find(w) == m.end()) {
			m[w] = WinKeySeqFrag<typename T::ElementT, 1/*N*/>(w); /* default has 1 seqbuf */
			m[w].seqbufs[0].AllocBuf();  /* alloc underlying buffer */
		}
		m[w].seqbufs[0].pbuf->push_back(x);
	}
	DD("map has %lu segments", m.size());
#endif

		return m;
	};

#ifndef USE_UARRAY
	/* OBOSLETE generic sort, where default cmp func exists */
	SeqBuf Sort(void) {
		SeqBuf res;

		xzl_bug_on_msg(IsSorted(), "bug: try to sort a sorted seqbuf?");
		EE("sort %lu items", pbuf->size());

		res.AllocBuf();
		res.CopyFrom(*this);
		std::sort(res.pbuf->begin(), res.pbuf->end());
		res.sorted_ = 0;
		return res;
	}
#endif

	template <int keypos> SeqBuf Sort2() const {
		SeqBuf res;
		const unsigned n_outputs = 1;  /* fixed so far */

		xzl_bug_on_msg(IsSorted(), "bug: try to sort a sorted seqbuf?");
		xzl_bug_on(!this->pbuf->xaddr);
		DD("To sort xaddr %lx", this->pbuf->xaddr);

		res.AllocBuf();

#ifdef USE_UARRAY
		x_addr dests[n_outputs] = {0};
		auto ret = sort(dests, n_outputs, this->pbuf->xaddr, keypos, reclen<T>());
		if (ret < 0) {
			int32_t sz = uarray_size(this->pbuf->xaddr);
			Backtrace();
			EE("bug? sort returns %d. source sz = %d", ret, sz);
		}
		xzl_bug_on(ret < 0);
		res.pbuf->xaddr = (dests)[0];
		xzl_assert(res.pbuf->xaddr);
#else
		res.CopyFrom(*this);
		std::sort(res.pbuf->begin(), res.pbuf->end(),
							K2VCompLess<T, keypos>()
		);
#endif

		I("sorted: src: xaddr %lx, res: new sbatch xaddr %lx, keypos %u", this->pbuf->xaddr, dests[0], keypos);

		res.sorted_ = keypos;
		res.size_ = this->size_;
		return res;
	}

	/* can return multiple outputs */
	template <int keypos>
	vector<SeqBuf<T>> Sort(unsigned n_outputs) const
	{
#ifdef USE_UARRAY
		x_addr dests[n_outputs] = {0};
		int32_t ret = 0;

		vector<SeqBuf<T>> res(n_outputs);

		xzl_bug_on_msg(IsSorted(), "bug: try to sort a sorted seqbuf?");
		DD("To sort xaddr %lx", this->pbuf->xaddr);
		xzl_bug_on(!this->pbuf->xaddr);

		for (auto & r : res) {
			r.AllocBuf();
		}

		ret = sort(dests, n_outputs, this->pbuf->xaddr, keypos, reclen<T>());
		if (ret < 0) {
			int32_t sz = uarray_size(this->pbuf->xaddr);
			Backtrace();
			EE("bug? sort returns %d. source sz = %d", ret, sz);
		}
		xzl_bug_on(ret < 0);

		for (unsigned i = 0; i < n_outputs; i++) {
			xzl_assert(dests[i]);
			res[i].pbuf->xaddr = dests[i];
			res[i].sorted_ = keypos;
			res[i].size_ = SEQBUFSZ_SOME;
			I("Sort ret %u: new sbatch xaddr %lx", i, dests[i]);
		}

		return res;
#else
		xzl_bug("tbd");
#endif
	}

#ifdef USE_UARRAY
	/* sort the input to N seqbufs. In each output, the records are primary
	 * sorted by k1, then k2.
	 *
	 * Be careful to retire any intermediate seqbufs */
	template <int k1pos, int k2pos>
	vector<SeqBuf<T>> SortMultiKey(unsigned n_outputs) const
	{
		xzl_bug_on_msg(IsSorted(), "bug: try to sort a sorted seqbuf?");
//		xzl_assert(n_outputs == 1 || n_outputs == 2 || n_outputs == 4);
		xzl_assert(n_outputs <= 7 && n_outputs > 0);  /* just sanity check */

		vector<SeqBuf<T>> res(n_outputs);
		x_addr dests[n_outputs] = {0}; /* intermediate */
		for (auto & r : res) {
			r.AllocBuf();
			r.sorted_ = k1pos; /* should we use bitmap? */
		}

		if (!this->Size())
			return res; 		// nothing

		xzl_assert(pbuf->xaddr);

		/* 1: sort input (through k2) into n outputs */
		auto ret = sort(dests, n_outputs, this->pbuf->xaddr, k2pos, reclen<T>());
		xzl_bug_on(ret < 0);

		/* 2: (stable) sort each output through k1, store to output*/
		for (unsigned i = 0; i < n_outputs; i++) {
			xzl_bug_on(!dests[i]);
			x_addr x = 0;
			auto ret = sort_stable(&x, 1 /*n output*/, dests[i], k1pos, k2pos, reclen<T>());
			xzl_bug_on(ret < 0 || !x);
			res[i].pbuf->xaddr = x;
			res[i].size_ = SEQBUFSZ_SOME;

			/* retire intermediate */
			ret = retire_uarray(dests[i]);
			xzl_bug_on(ret < 0);
		}

		return res;
	};
#endif

	/* emit WHERE low < value  < high
 * @low/high are all records for simplicity
 * */
	template <int vpos>
	SeqBuf FilterBand(T const & low, T const & high) {
		SeqBuf ret;
		ret.AllocBuf();
		const unsigned n_outputs = 1;
#ifdef USE_UARRAY
		x_addr dests[n_outputs];

		auto res = filter_band(dests, n_outputs, pbuf->xaddr,
								std::get<vpos>(low.data), std::get<vpos>(high.data), vpos, reclen<T>());
		xzl_bug_on(res < 0);
#else
		for (auto & x : *pbuf) {
			if (std::get<vpos>(low.data) < std::get<vpos>(x.data)
					&& std::get<vpos>(x.data) < std::get<vpos>(high.data))
				ret.pbuf->push_back(x);
		}
#endif
		xzl_bug_on_msg(!dests[0], "invalid xaddr (0)?");
		ret.pbuf->xaddr = dests[0];
		ret.size_ = SEQBUFSZ_SOME;

		return ret;
	}

	inline bool IsSorted(void) const {
		xzl_bug_on(!pbuf);

		/* if 0/1 element, we regarded it as sorted */
		return (sorted_ != -1 || Size() <= 1);
	}

	inline int SortedKey(void) const {
		return sorted_;
	}

	/* true if seq1/2 sorted and sorted by the same key */
	static bool IsSortedSame(SeqBuf const & seq1, SeqBuf const & seq2)
	{
		xzl_bug_on(!seq1.pbuf || !seq2.pbuf); /* mem must be allocated */

		/* okay -- either seq has 0/1 element */
		if (seq1.Size() <= 1 || seq2.Size() <= 1)
			return true;

		auto key1 = seq1.SortedKey();
		auto key2 = seq2.SortedKey();

		/* not okay -- one seq is unsorted */
		if (key1 == -1 || key2 == -1) {
			EE("IsSortedSame failed: seq1 key %d size %lu, seq2 key %d size %lu", key1, seq1.Size(), key2, seq2.Size());
			return false;
		}

		if (key1 != key2)
			EE("IsSortedSame failed: seq1 key %d size %lu, seq2 key %d size %lu", key1, seq1.Size(), key2, seq2.Size());

		return (key1 == key2);
	}

	virtual ~SeqBuf() {

	}

	/* dbg */
	void Dump(const char * tag = "") const {
		printf("dump %s: total %lu items\n", tag, Size());
#ifdef USE_UARRAY
		printf("tz sbatch. content invisible\n");
#else
		for (auto && x : *pbuf) {
			for (int i = 0; i < std::tuple_size<T>::value; i++)
				cout << x << " " << endl;
		}
		cout << endl;
#endif
	}

	/* default = operator */

	/* only used by source -- for backward compatible */
//	void add_record (const T & kv)

#ifdef USE_UARRAY
	shared_ptr<sbatch> pbuf;
#else
	shared_ptr<vector<T>> pbuf;
#endif

//private:
public:
	int sorted_ = -1;  /* never sorted: -1; otherwise return the sorted key (0, 1, ...). this is quite messy now */
#ifdef USE_UARRAY
	/* # elements.
	 * for tz we need to keep this ourselves
	 * 0: exact 0
	 * 1: exact 1
	 * 2: >0, but don't know how many
	 * */
	size_t size_ = SEQBUFSZ_0;
//	const unsigned reclen = sizeof(T)/sizeof(simd_t);
#endif
};

/* --- specialized for kvpair  --- */

using RecordKVPair = Record<creek::kvpair>;

/*
 * For KVPair
 */
//template<>
//SeqBuf<RecordKVPair>
//SeqBuf<RecordKVPair>::Sort();

template<>
void SeqBuf<RecordKVPair>::Dump(const char *) const;

//template<>
//SeqBuf<RecordKVPair>
//SeqBuf<RecordKVPair>::Merge(SeqBuf<RecordKVPair> const & other);

#if 0
template<>
SeqBuf<RecordKVPair> SeqBuf<RecordKVPair>::FilterBand(RecordKVPair const & low,
																									RecordKVPair const & high);

#include "Window.h"
/*
 * forward decl of KeyFrag, to resolve circular dep
 */
/* unused? */
template <typename T>
struct WinSeqFrag;

template <class KVPair, unsigned N>
struct WinKeySeqFrag;

template<> template<>
vector<WinSeqFrag<creek::kvpair>>
SeqBuf<RecordKVPair>::Segment(creek::etime base, creek::etime subrange);

/* unused? */
template<> template<>
map<Window, WinSeqFrag<creek::kvpair>, Window>
SeqBuf<RecordKVPair>::Segment(creek::etime base, creek::etime subrange);

template<> template<>
map<Window, WinKeySeqFrag<creek::kvpair,1/*N*/>, Window>
SeqBuf<RecordKVPair>::Segment(creek::etime base, creek::etime subrange);
#endif


#endif //CREEK_SEQBUF_H
