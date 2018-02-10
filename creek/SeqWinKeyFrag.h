//
// Created by xzl on 3/17/17.
//

#ifndef CREEK_APPENDKVBUFFER_H
#define CREEK_APPENDKVBUFFER_H


/* wrapper for the per thread memblocks
 *
 * ideas from
 * http://stackoverflow.com/questions/14807192/can-i-use-an-stdvector-as-a-facade-for-a-pre-allocated-raw-array
 *
 * the caller must ensure allocation request won't exceed the bound of @pre_allocated_memory
 * */
template <typename T>
class placement_memory_allocator: public std::allocator<T>
{
	void* pre_allocated_memory;
public:
	typedef size_t size_type;
	typedef T* pointer;
	typedef const T* const_pointer;

	template<typename _Tp1>
	struct rebind
	{
		typedef placement_memory_allocator<_Tp1> other;
	};

	pointer allocate(size_type n, const void *hint=0)
	{
		char* p = new(pre_allocated_memory)char[n * sizeof(T)];
		cout << "Alloc " << dec << n * sizeof(T) << " bytes @" << hex << (void*)p <<endl;
		return (T*)p;
	}

	/* xzl: TODO we shall never deallocate right?
	 * */
	void deallocate(pointer p, size_type n)
	{
		cout << "(should) Dealloc " << n * sizeof(T) << " bytes @" << hex << p << endl;
//		free(p);
	}

	placement_memory_allocator(void* p = 0) throw(): std::allocator<T>(), pre_allocated_memory(p) {
//		cout << "Hello allocator!" << endl;
	}

	placement_memory_allocator(const placement_memory_allocator &a) throw(): std::allocator<T>(a) {
		pre_allocated_memory = a.pre_allocated_memory;
	}

	~placement_memory_allocator() throw() { }
};

template<class RecordKV>
bool CompKVRecordByKey(const RecordKV &lhs, const RecordKV &rhs) {
	using KV = typename RecordKV::ElementT;
	return lhs.data.first < rhs.data.first;
}

/* append only. wrapping around the underlying seq buffer
 * single threaded */
template<class KVPair>
struct SeqWinKeyFrag
{

	using K = decltype(KVPair::first);
	using V = decltype(KVPair::second);
	using RecordKV = Record<KVPair>;

private:
//	unsigned long len_;
	unsigned long max_len_;

	placement_memory_allocator<RecordKV> alloc_;

	/* this internal d/s. don't expose this
	 * TODO: need more compact format to suit SIMD */
	vector<RecordKV, placement_memory_allocator<RecordKV>> vals;

	Window w;

public:
	void add(const RecordKV& val) {
		vals.push_back(val);
	}

	bool is_sorted() { return sorted_; }

	/* return: okay or not */
	bool sort_by_key() {
		if (sorted_)
			return  false;
		std::sort(vals.begin(), vals.end(), CompKVRecordByKey<RecordKV>);
		sorted_ = true;
		return true;
	}

	SeqWinKeyFrag(void* buf, unsigned long total_bytes)
		: max_len_(total_bytes / sizeof(RecordKV)), alloc_(buf), vals(buf)
	{
		vals.reserve(max_len_);  /* spanning the whole buffer */
	}

	void dump() { // debugging
		cout << "dump: " << endl;
		for (const auto & rec : vals) {
			cout << dec << rec.data.first << ", " << rec.data.second << endl;
		}
	}

private:
	unsigned long len_ = 0; /* # of current elements */
	bool sorted_ = false;
};

/* per window, the workspace d/s: a table of SeqWinKeyFrag */
#if 0
template <class FragT>
struct SeqWinKeyFragTable
{
	FragT table[CONFIG_MAX_NUM_CORES][CONFIG_MAX_BUNDLES_PER_EPOCH];
	unsigned int count[CONFIG_MAX_NUM_CORES] = {0};

	/* thread safe */
	void depositOneFragT(int tid, FragT const & f) {
		xzl_bug_on(tid >= CONFIG_MAX_NUM_CORES);
		xzl_bug_on(count[tid] == CONFIG_MAX_BUNDLES_PER_EPOCH);

		table[tid][count[tid]] = f;
		count[tid] ++;
	}

};
#endif

/* Saving FragT as shared ptrs */
template <class FragT>
struct SeqWinKeyFragTable
{
	shared_ptr<FragT> table[CONFIG_MAX_NUM_CORES][CONFIG_MAX_BUNDLES_PER_EPOCH];
	unsigned int count[CONFIG_MAX_NUM_CORES] = {0};

	/* thread safe */
	void depositOneFragT(int tid, shared_ptr<FragT> const & f) {
		xzl_bug_on(tid >= CONFIG_MAX_NUM_CORES);
		xzl_bug_on(count[tid] == CONFIG_MAX_BUNDLES_PER_EPOCH);

		table[tid][count[tid]] = f;
		count[tid] ++;
	}

};


#endif //CREEK_APPENDKVBUFFER_H
