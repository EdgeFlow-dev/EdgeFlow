//
// Created by xzl on 8/23/17.
//

#ifndef CREEK_BUNDLE_H
#define CREEK_BUNDLE_H

#include "config.h"

#include "Values.h"
#include "Record.h"
#include "WinKeyFrag.h"
#include "WinKeySeqFrag.h"
#include "SeqBuf.h"

struct Window;

/* records stored in vector */
// row format.
// @T: element type, e.g. long
#ifdef USE_FOLLY_VECTOR
#include "folly/FBVector.h"  /* if using folly... */
#endif

struct bundle_container;

enum LIST_INFO {
	LIST_INVALID = -1,
	LIST_ORDERED = 0,
	LIST_L,
	LIST_R,
	LIST_UNORDERED,
	LIST_MAX
};

/* does not contain the actual data */
struct BundleBase {

	using etime = creek::etime;

public:

	PValue * value; // the PValue.that encloses the bundle.
	// XXX now only the "Windowed" bundle maintains this
	etime min_ts = etime_max;

	int node; // the residing numa node

	atomic<long>* refcnt; /* pointing to the refcnt in the enclosing container */
	atomic<bundle_container*> container; /* the enclosing container */

//#ifdef MEASURE_LATENCY
	vector<mark_entry> marks;
	void mark(string const & msg) {
		marks.emplace_back(msg, boost::posix_time::microsec_clock::local_time());
	}

	void inherit_markers(BundleBase const & other) {
		this->marks.insert(this->marks.end(), other.marks.begin(),
											 other.marks.end());
	}

	void dump_markers() {
//#if 0
		int i = 0;
		boost::posix_time::ptime first;
		boost::posix_time::ptime last;

//  	cout << "dump markers: <<<<<<<<<<<<<<<<<<<<<<<<<<<\n";
		cout << "dump markers: <<<<<<<<<<<<<<" << (this->min_ts) << endl;
		for (auto & e : this->marks) {
			cout << "dump markers:" << i++ << "\t"
					 << e.msg << " ";

			if (i == 1) {
				first = e.ts;
				cout << to_simple_string(e.ts) << endl;
			}
			else {
				cout << " +" << (e.ts - last).total_milliseconds() << " ms" << endl;
			}
			last = e.ts;
		}
		cout << "dump markers: >>>>>>>>>>>>>>>>>>>>>>>>>>>>";
		cout << "total " << (last - first).total_milliseconds() << " ms" << endl;
//#endif
	}
//#endif

	/* must be explicit on node */
	BundleBase(int node) : value(nullptr), node(node) { }

	virtual ~BundleBase() {
//  	VV("bundle destroyed");
	}

	/* how many records in the bundle? useful if the bundle class has
	 * linear d/s internally.
	 * -1 means the bundle class does not implement the method.
	 */
	virtual int64_t size() { return -1; }
};

/* punctuation. reuse min_ts as the carried upstream wm snapshot. */
struct Punc : public BundleBase {

private:
	/* track which side a punc comes from (partial wm) */
	enum LIST_INFO list_info = LIST_INVALID;

public:
	enum LIST_INFO get_list_info(){
		return list_info;
	}
	void set_list_info(enum LIST_INFO i) {
		list_info = i;
	}

	Punc(creek::etime const & up_wm, int node = -1) : BundleBase(node) {
		this->min_ts = up_wm;
		this->staged_bundles = nullptr;
	}

	/* bundle staged in the container which this punc is assigned to. before
	 * depositing the punc to downstream, these bundles must be deposited first.
	 */
	vector<shared_ptr<BundleBase>>* staged_bundles;

};

template <typename T>
struct RecordBundle : public BundleBase {
	using RecordT = Record<T>;

	using AllocatorT = creek::allocator<RecordT>;

#ifdef USE_FOLLY_VECTOR
	using VecT = folly::fbvector<RecordT, AllocatorT>;
#else
	using VecT = vector<RecordT, AllocatorT>;
#endif

//  static NumaAllocator<T> _alloc;
//  NumaAllocator<RecordT> alloc;

	VecT content;

	int64_t size() override { return content.size(); }

	// minimize reallocation
	RecordBundle(int init_capacity = 1024, int node = -1)
#ifdef USE_NUMA_ALLOC
	: BundleBase(node) , content(AllocatorT(node))
#else
			: BundleBase(node)
#endif

	{
		assert(init_capacity >= 0);
		content.reserve(init_capacity);
	}

	// has to maintain per bundle ts
	virtual void add_record(const RecordT& rec) {
		content.push_back(rec);
		if (rec.ts < min_ts)
			min_ts = rec.ts;
	}

	/* only for specialized */
	void add_record(const Window & win, const RecordT& rec);

	virtual void emplace_record(const T& v, creek::etime const & ts) {
		content.emplace_back(v, ts);
		if (ts < min_ts)
			min_ts = ts;
	}

//  typename vector<RecordT>::iterator begin() {
//  typename decltype(content)::iterator begin() {
	typename VecT::iterator begin() {
		return content.begin();
	}

//  typename vector<RecordT>::iterator end() {
	typename VecT::iterator end() {
		return content.end();
	}

#if 0
	/* for debugging */
  virtual ~RecordBundle() {
    W("bundle destroyed. #records = %lu", _content.size());
  }
#endif
};

/* specialization (inline).
 * ignore the @win info. to be compatible with the WindowsBundle interface */
template<> inline
void RecordBundle<creek::tvpair>::add_record(const Window & win, const RecordT& rec) {
	add_record(rec);
}

template <typename T>
struct RecordBundleDebug : public RecordBundle<T> {
	/* for debugging */
	virtual ~RecordBundleDebug () {
		W("bundle destroyed. #records = %lu", this->content.size());
	}
};

/* has a bitmap that supports indicate "masked/deleted" records.
 * provides an iterator to transparently go over the unmasked Records.
 */
template <typename T>
struct RecordBitmapBundle : public RecordBundle<T>
{
	using RecordT = Record<T>;

//#ifdef USE_NUMA_ALLOC
//  using AllocatorT = NumaAllocator<bool>;
//#else
//  using AllocatorT = std::allocator<bool>;
//#endif

	using AllocatorT = creek::allocator<bool>;

	vector<bool, AllocatorT> selector;
	//vector<bool> selector;

	//////////////////////////////////////////////////////////////
	// cannot use "const" iterator, since we have to modify the bitmap
	class iterator {
	private:
		uint64_t index;
		RecordBitmapBundle *bundle;

	public:
		iterator(RecordBitmapBundle* b, uint64_t index)
				: index(index), bundle(b) {
			// only can be constructed from begin and end.
			// since any where in the middle may hit "holes" and we don't
			// deal with that.
			assert(index == bundle->content.size() || index == 0);

			// if this is begin, seek the next unmasked item
			while (this->index < bundle->content.size()
						 && !bundle->selector[this->index])
				this->index ++;
		}

		bool operator !=(iterator const& other) const {
			return index != other.index;
		}

		// find the next unmasked item
		iterator& operator++() {
			if (index < bundle->content.size()) {
				index ++;
				while (index < bundle->content.size() && !bundle->selector[index])
					index ++;
			}
			return *this;
		}

		// will this incur extra copy?
		RecordT const& operator*() {
//    RecordT & operator*() {
			return bundle->content[index];
		}

		bool mask() {
			assert(index >= 0 && index < bundle->content.size()
						 && index < bundle->selector.size());

			bundle->selector[index] = false;
			return true; // XXX gracefully
		}

		// ever needed?how
		bool unmask() {
			assert(index >= 0 && index < bundle->content.size()
						 && index < bundle->selector.size());

			bundle->selector[index] = true;
			return true; // XXX gracefully
		}

	};
	//////////////////////////////////////////////////////////////

	// XXX make the following two consts

	iterator begin() {
		return iterator(this, 0);
	}

	iterator end() {
		return iterator(this, this->content.size());
	}

	RecordBitmapBundle(int init_capacity = 128, int node = -1)
			: RecordBundle<T>(init_capacity, node)
#ifdef USE_NUMA_ALLOC
	, selector(AllocatorT(node))
#endif
	{
		// {
		selector.reserve(init_capacity);
	}

	virtual void add_record(const RecordT& rec) override {
		selector.push_back(true);
		RecordBundle<T>::add_record(rec);
	}

	void print(ostream &os) const {
		int count = 0, max_count = 3;
		os << "dump RecordBitmapBundle: ";
		for (auto it = this->begin(); it != this->end(); it++) {
			if (count ++ >= max_count)
				break;
			os << it->data << " ";
		}
		os << endl;
	}
};

#if 0  // obsoleted
/*
 * A bundle of multiple WindowFragments (may belong to multi windows).
 * The output of a GroupBy() from a bundle.
 */
template <typename K, typename V>
struct WindowedKVBundle : public BundleBase {
  using KVS = tuple<K, vector<V>>;

  /* sorted by window start time */
  map<Window, WindowedKVOutput<K,V>*, Window> vals;

  void add(WindowedKVOutput<K,V>* output) {
    vals[output->w] = output;
  }
};
#endif

/* A bundle with records in multiple windows.
 * Each window is assoc with a in Fragments
 * (i.e. a vector of Records, not grouped by key yet.)
 *
 * T: element type, e.g. long
 *
 * We do not implement size() since the bundle has a map inside.
 */
template <typename T>
struct WindowsBundle : public BundleBase {

	using RecordT = Record<T>;

	// sorted, by the window time order.
	map<Window, shared_ptr<WindowFragment<T>>, Window> vals;

	void add(shared_ptr<WindowFragment<T>> output) {
		vals[output->w] = output;

		if (output->min_ts < min_ts)
			min_ts = output->min_ts;
	}

	/* can be called with (win, val), for which a record will be auto constructed based on val */
	void add_record(const Window& w, const RecordT& val) {
		add_value(w, val);
	}

	// this is actually "add record"
	// @val must be a timestamped value.
	void add_value(const Window& w, const RecordT& val) {
		if (!vals.count(w)) {
			vals[w] = make_shared<WindowFragment<T>>(w);
			VV("create one window ... %d", vals[w]->id);
		}
		vals[w]->add(val);

		if (val.ts < min_ts)
			min_ts = val.ts;
	}

	// for debugging.
	void show_window_sizes() {
		unsigned long sum = 0;
		for (auto&& it = vals.begin(); it != vals.end(); it++) {
			auto output = it->second;
			assert(output);
			EE("window %d (start %s) size: %lu",
				 output->id, to_simple_string(output->w.start).c_str(),
				 output->size());
			sum += output->size();
		}
		EE("----- total %lu", sum);
	}

	/* simply ignore the capacity */
	WindowsBundle(int capacity = 0, int node = -1) : BundleBase(node) { };

};

/* A bundle with records in multiple windows.
 *
 * Each window is assoc with a KeyedFragments (i.e. Records grouped by key)
 * Windows are sorted (facilitating output them in order).
 *
 * XXX merge this with WindowsBundle
 */
template <class KVPair,
		/* WindowKeyFrag can be specialized based on key/val distribution */
		template<class> class WindowKeyedFragmentT // = WindowKeyedFragment
>
struct WindowsKeyedBundle : public BundleBase {

	using K = decltype(KVPair::first);
	using V = decltype(KVPair::second);
	using RecordKV = Record<KVPair>;
//  using WindowFragmentKV = WindowKeyedFragment<KVPair>;

	/* no need to use concurrent map */
//  using WindowFragmentKV = WindowKeyedFragment<KVPair,
//  		std::unordered_map<decltype(KVPair::first),
//  												ValueContainer<decltype(KVPair::second)>>>;
	using WindowFragmentKV = WindowKeyedFragmentT<KVPair>;

	// ordered, can iterate in window time order.
	// embedding WindowFragmentKV, which does not contain the actual Vs
	map<Window, WindowFragmentKV, Window> vals;

	// copy a given kvpair record under the corresponding window
	// and key.
	// XXX have a lockfree implementation
	void add_record(const Window& w, const RecordKV& val) {
//    if (!vals.count(w)) {
//        VV("create one window ... %d", vals[w].id);
//    }
//    assert(val.data.second != 0);
		vals[w].add_record_unsafe(val); /* since this works on a bundle */

		if (val.ts < min_ts)
			min_ts = val.ts;
	}

	/* ignore the capacity */
	WindowsKeyedBundle(int capacity = 0, int node = -1) : BundleBase(node) { };
};


/* cf: WindowsKeyedBundle. But here thTe underlying is SeqBuf */
#include "SeqBuf.h"
#include "WinKeySeqFrag.h"
#include "BundleContainer.h"

// same type. just unsorted by key yet
;

#endif //CREEK_BUNDLE_H
