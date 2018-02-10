//
// Created by xzl on 12/1/17.
//

#include <list>
#include <mutex>
#include <vector>
// hym: use concurrentqueue to avoid locks
// ref: https://github.com/cameron314/concurrentqueue
#include "xplane_lib.h"

#include "concurrentqueue.h"
#include "xplane/type.h"
#include "log.h"
#include "../../linux-sizes.h"
#include "batch.h"
#include <sys/mman.h>

//defined in batch.h
/*
#define ITEMS_PER_RECORD	(3)
#define RECORDS_PER_BUNDLE	(100 * 1000) 	// 100k or 1M, should be consistent with engine
#define NUM_PARTITIONS		(20) 		// parallelism, should be consistent with engine
#define BUNDLES_PER_WINDOW	(20)		// # bundles between two watermarks 

#define BUFF_SIZE_4K			(4096)
#define BUFF_SIZE_BUNDLE_KP_PART	((RECORDS_PER_BUNDLE / NUM_PARTITIONS) * sizeof(simd_t)) //kbatch/pbatch will be splited into NUM_PARTITIONS	
#define BUFF_SIZE_BUNDLE_KP		(RECORDS_PER_BUNDLE * sizeof(simd_t))
#define BUFF_SIZE_BUNDLE_RECORD		(RECORDS_PER_BUNDLE * ITEMS_PER_RECORD * sizeof(simd_t))		

#define BUFF_SIZE_WINDOW_KP		(BUNDLES_PER_WINDOW * RECORDS_PER_BUNDLE * sizeof(simd_t))
#define BUFF_SIZE_WINDOW_RECORD		(BUNDLES_PER_WINDOW * RECORDS_PER_BUNDLE * ITEMS_PER_RECORD * sizeof(simd_t))
*/

#define BASE_POOL_SIZE 50

/* xzl : must be consisent with batch.h NUM_POOLS  */
/* number of chunks in each pool */
#define SLOW_POOL_SIZE_4K 						(100)
#define SLOW_POOL_SIZE_BUNDLE_KP_PART (BASE_POOL_SIZE * BUNDLES_PER_WINDOW * NUM_PARTITIONS)
#define SLOW_POOL_SIZE_BUNDLE_KP			(10 * BASE_POOL_SIZE * BUNDLES_PER_WINDOW)
#define SLOW_POOL_SIZE_BUNDLE_RECORD		(BASE_POOL_SIZE * BUNDLES_PER_WINDOW)
#define SLOW_POOL_SIZE_WINDOW_KP		(BASE_POOL_SIZE)
#define SLOW_POOL_SIZE_WINDOW_RECORD		(BASE_POOL_SIZE)

/* default values, can be overwritten */
#if 0
static std::array<unsigned long, NUM_POOLS> pool_grains = {
		BUFF_SIZE_4K,
		BUFF_SIZE_BUNDLE_KP_PART,
		BUFF_SIZE_BUNDLE_KP,
		BUFF_SIZE_BUNDLE_RECORD,
		BUFF_SIZE_WINDOW_KP,
		BUFF_SIZE_WINDOW_RECORD
};

static std::array<unsigned long, NUM_POOLS> pool_sizes = {
		SLOW_POOL_SIZE_4K,
		SLOW_POOL_SIZE_BUNDLE_KP_PART,
		SLOW_POOL_SIZE_BUNDLE_KP,
		SLOW_POOL_SIZE_BUNDLE_RECORD,
		SLOW_POOL_SIZE_WINDOW_KP,
		SLOW_POOL_SIZE_WINDOW_RECORD
};

static std::array<unsigned long, NUM_POOLS> pool_flags = {
		POOL_MALLOC,
		POOL_HUGETLB,
		POOL_HUGETLB,
		POOL_HUGETLB,
		POOL_HUGETLB,
		POOL_HUGETLB
};
#endif

struct pool_grain_cmpless {
	bool operator()(const struct pool_desc & lhs,
									const struct pool_desc & rhs) const {
		return lhs.grain < rhs.grain;
	}
};

struct pool_grainv_cmpless {
	bool operator()(struct pool_desc const & lhs,
									unsigned long const & rhs) const {
		return lhs.grain < rhs;
	}
};

static struct pool_desc default_pool_configs[NUM_POOLS + 1] = {
		{
				.name = "4k",
				.grain = BUFF_SIZE_4K,
				.size = SLOW_POOL_SIZE_4K,
				.flag = POOL_MALLOC
		},
		{
				.name = "kp_part",
				.grain = BUFF_SIZE_BUNDLE_KP_PART,
				.size = SLOW_POOL_SIZE_BUNDLE_KP_PART,
				.flag = POOL_HUGETLB
		},
		{
				.name = "kp",
				.grain = BUFF_SIZE_BUNDLE_KP,
				.size = SLOW_POOL_SIZE_BUNDLE_KP,
				.flag = POOL_HUGETLB
		},
		{
				.name = "record",
				.grain = BUFF_SIZE_BUNDLE_RECORD,
				.size = SLOW_POOL_SIZE_BUNDLE_RECORD,
				.flag = POOL_HUGETLB
		},
		{
				.name = "win_kp",
				.grain = BUFF_SIZE_WINDOW_KP,
				.size = SLOW_POOL_SIZE_WINDOW_KP,
				.flag = POOL_HUGETLB
		},
		{
				.name = "win_rec",
				.grain = BUFF_SIZE_WINDOW_RECORD,
				.size = SLOW_POOL_SIZE_WINDOW_RECORD,
				.flag = POOL_HUGETLB | POOL_CAN_FAIL
		},
		{
				.name = "end",
				.grain = 0,
				.size = 0,
				.flag = 0
		},
};

static std::vector<pool_desc> pools;

//static const char *pool_names[] = {"4k", "kp_part", "kp", "record", "win_kp", "win_rec"};
static std::array<std::atomic<int>, NUM_POOLS> state_cnt;
static std::array<std::atomic<int>, NUM_POOLS> state_max; /* max allocations */

#ifndef USE_MEM_POOL /* stat of the required allocation size */
#include <map>
//#include "boost/thread/thread.hpp" // uses macro V()
//boost::shared_mutex req_cnt_mtx;
std::mutex mtx_req;
std::map<unsigned long, unsigned long> req_cnt; /* req size, req current cnt */
std::map<unsigned long, unsigned long> req_max; /* req size, req max cnt */
#endif

/* return which pool should the allocation from from.
 * -1 if no found */
long mempool_dispatch(unsigned long buf_size) {
	/* first pool size that >= @buf_size */
	auto p = std::lower_bound(pools.cbegin(), pools.cend(), buf_size, pool_grainv_cmpless());
	if (p == pools.end()) {
		EE("size %lu MB", buf_size / 1024 / 1024);
		xzl_bug("can's satisify");
		return -1;
	}

	return std::distance(pools.cbegin(), p); /* index */
}

static void _stat_get(int p) {
//	xzl_bug_on(p == POOL_BUNDLE_KP);
	auto sofar = \
	state_cnt[p].fetch_add(1, std::memory_order_relaxed);

	sofar ++;

	auto m = state_max[p].load(std::memory_order_relaxed);
	if (sofar > m) {
		/* try to update the max count. don't retry if we failed due to competition. */
		state_max[p].compare_exchange_strong(m, sofar);
	}
}

static void _stat_put(int p) {
	state_cnt[p].fetch_sub(1, std::memory_order_relaxed);
}

#ifndef USE_MEM_POOL
/* stat for the actual sizes of mem req.
 * expensive -- only for dbg! */
static void _req_get(unsigned long sz) {
	//xzl_bug_on(sz % 100 != 0);

	std::unique_lock<std::mutex> lck(mtx_req);
	auto & sofar = req_cnt[sz]; /* will auto create k/v if v non existing */
	sofar ++;

	auto & m = req_max[sz];
	if (sofar > m)
		m = sofar;
}

static void _req_put(unsigned long sz) {
	std::unique_lock<std::mutex> lck(mtx_req);
	auto & sofar = req_cnt[sz]; /* will auto create the k/v if non existing */

	xzl_bug_on(sofar == 0);
	sofar --;
}
#endif

/* for statistics. can be called w/o locking */
void mempool_stat_get(unsigned long sz) {
	auto p = mempool_dispatch(sz);
	_stat_get(p);
#ifndef USE_MEM_POOL /* for stat only */
	_req_get(sz);
#endif
}

void mempool_stat_put(unsigned long sz) {
	auto p = mempool_dispatch(sz);
	_stat_put(p);
#ifndef USE_MEM_POOL /* for stat only */
	_req_put(sz);
#endif
}

std::array<int, NUM_POOLS> mempool_stat_snapshot(void) {
	std::array<int, NUM_POOLS> ret;

	for (auto i = 0; i < NUM_POOLS; i++) {
		ret[i] = state_cnt[i].load(std::memory_order_relaxed);
	}
	return ret;
}

extern "C"
void mempool_stat_print(void) {
#ifdef USE_MEM_POOL
	EE("dump mempool stat");
#else

	/* print out the stat of the actual req size -- long print list */
#if 1
	{
		printf("%10s %8s %8s\n", "sz", "curr", "max");
		std::unique_lock<std::mutex> lck(mtx_req);
		for (auto & kv : req_cnt) {
			auto m = req_max[kv.first];
			printf("%10lu %8lu %8lu \n", kv.first, kv.second, m);
		}
	}
#endif

	EE("dump mempool stat (emulated)");
#endif
	printf("%10s %8s %8s %8s %8s\n", "pool", "grain", "curr", "max", "total");
	for (auto i = 0; i < NUM_POOLS; i++) {
		printf("%10s %8lu %8d %8d %8lu \n", pools[i].name,
                     pools[i].grain,
					 state_cnt[i].load(std::memory_order_relaxed),
					 state_max[i].load(std::memory_order_relaxed),
					 pools[i].size
		);
	}
	printf("============\n");
}

#if 0
static moodycamel::ConcurrentQueue<simd_t *> mem_pool_slow_4k;
static moodycamel::ConcurrentQueue<simd_t *> mem_pool_slow_bundle_kp_part;
static moodycamel::ConcurrentQueue<simd_t *> mem_pool_slow_bundle_kp;
static moodycamel::ConcurrentQueue<simd_t *> mem_pool_slow_bundle_record;
static moodycamel::ConcurrentQueue<simd_t *> mem_pool_slow_window_kp;
static moodycamel::ConcurrentQueue<simd_t *> mem_pool_slow_window_record;
#endif

/* XXX change to vector? XXX */
static std::array<moodycamel::ConcurrentQueue<simd_t *>, NUM_POOLS> mem_pools;

struct pool_desc const get_pool_config(unsigned int id) {
	xzl_bug_on(id >= pools.size());
	return pools[id];
}

/* @p: mem pool config. see below 

	if !USE_MEM_POOL, still allocate pool descs but not the actual mem chunks.
	this is for profiling/debugging.
*/
void init_mem_pool_slow(struct pool_desc *p){

	if (!p)
		p = default_pool_configs;

	auto cnt = 0;
	/* load pool config to vector */
	pools.clear();
	while (p[cnt].size) {
		pools.push_back(p[cnt]);
		cnt ++;
	}
	xzl_bug_on(cnt != NUM_POOLS); /* XXX support different pool counts */

	/* sort pools by grains */
	std::sort(pools.begin(), pools.end(), pool_grain_cmpless());

#ifdef USE_MEM_POOL /* do actual allocation */
//	for (auto & pool : pools) {
	for (auto it = pools.begin(); it != pools.end(); it ++) {
		auto & pool = *it;
		auto index = it - pools.begin();
        bool fail; 

		const char * postfix = xzl_int_postfix(pool.grain);
		EE("preallocate: %9s: %6lu chunks, chunk size %6ld %sB", pool.name, pool.size,
			 xzl_int_val(pool.grain), postfix);

#if 1
		for (uint32_t i = 0; i < pool.size; i++) {
			simd_t * tmp;
			if (is_malloc(pool.flag)) {
				tmp = (simd_t *)malloc(pool.grain);
                fail = !tmp;
                xzl_assert(tmp != MAP_FAILED); /* shall never happen */
            } else {
				tmp = (simd_t *) mmap(NULL, pool.grain, PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB, 0, 0);
                fail = (tmp == MAP_FAILED); 
                xzl_assert(tmp); /* shall never happen */
            }

			if (fail) {
				EE("warning: malloc %s failed... (flag=%d) %d allocated", pool.name, pool.flag, i);
				if (pool.flag & POOL_CAN_FAIL)
					break; /* next pool */
				else
					abort();
			} else
				mem_pools[index].enqueue(tmp);
		}
#else /* allocate everything on huge pages */
		uint32_t chunk = 0, offset = 0; /* offset within the current huge page (map) */
		void *hugepage = nullptr; /* current map */

		while (chunk < pool.size) {

			if (!hugepage || offset + pool.grain > SZ_2M) { /* need a new hugepage */
				hugepage = mmap(NULL, pool.grain, PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB, 0, 0);
				offset = 0;
				fail = (hugepage == MAP_FAILED);
				xzl_assert(hugepage); /* shall never happen */
			}

			if (fail) {
				EE("warning: malloc %s failed... (flag=%d) %d allocated", pool.name, pool.flag, chunk);
				if (pool.flag & POOL_CAN_FAIL)
					break; /* next pool */
				else
					xzl_bug("can't alloc mem pool");
			}

			/* we have space on the current hugepage for the current chunk */
			mem_pools[index].enqueue((simd_t *)((char *)hugepage + offset));

			offset += pool.grain;
			chunk ++;
		}
#endif

	}

#endif

#if 0
	for(i = 0; i < SLOW_POOL_SIZE_4K; i++){
		simd_t * tmp = (simd_t *)malloc(pool_sizes[POOL_4K]);
		//simd_t * tmp = (simd_t *) mmap(NULL, BUFF_SIZE_1M, PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB, 0, 0);
		if(!tmp){
			EE("warning: malloc 4K failed.... %d allocated", i);
			abort();
			//return;
		}
		mem_pool_slow_4k.enqueue(tmp);
	}
	EE("preallocate %d chunks in mem_pool_slow_4k, chunk size is %d", i, BUFF_SIZE_4K);

	for(i = 0; i < SLOW_POOL_SIZE_BUNDLE_KP_PART; i++){
		//simd_t * tmp = (simd_t *)malloc(BUFF_SIZE_128M);
		simd_t * tmp = (simd_t *)mmap(NULL, pool_sizes[POOL_BUNDLE_KP_PART], PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB, 0, 0);
		if(tmp == MAP_FAILED || !tmp){
			EE("give up: mmap BUNDLE_KP_PART failed.... %d allocated", i);
			abort();
            		//return; 
		}
		mem_pool_slow_bundle_kp_part.enqueue(tmp);
	}
	EE("preallocate %d chunks in mem_pool_slow_bundle_kp_part, chunk size is %ld", i, BUFF_SIZE_BUNDLE_KP_PART);
	
	for(i = 0; i < SLOW_POOL_SIZE_BUNDLE_KP; i++){
		//simd_t * tmp = (simd_t *)malloc(BUFF_SIZE_128M);
		simd_t * tmp = (simd_t *)mmap(NULL, pool_sizes[POOL_BUNDLE_KP], PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB, 0, 0);
		if(tmp == MAP_FAILED || !tmp){
			EE("give up: mmap BUNDLE_KP failed.... %d allocated", i);
			abort();
            		//return; 
		}
		mem_pool_slow_bundle_kp.enqueue(tmp);
	}
	EE("preallocate %d chunks in mem_pool_bundle_kp pool, chunk size is %ld", i, BUFF_SIZE_BUNDLE_KP);
	
	for(i = 0; i < SLOW_POOL_SIZE_BUNDLE_RECORD; i++){
		//simd_t * tmp = (simd_t *)malloc(BUFF_SIZE_128M);
		simd_t * tmp = (simd_t *)mmap(NULL, pool_sizes[POOL_BUNDLE_RECORD], PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB, 0, 0);
		if(tmp == MAP_FAILED || !tmp){
			EE("give up: mmap BUNDLE_RECORD failed.... %d allocated", i);
			abort();
            		//return; 
		}
		mem_pool_slow_bundle_record.enqueue(tmp);
	}
	EE("preallocate %d chunks in mem_pool_bundle_record pool, chunk size is %ld", i, BUFF_SIZE_BUNDLE_RECORD);
	
	for(i = 0; i < SLOW_POOL_SIZE_WINDOW_KP; i++){
		//simd_t * tmp = (simd_t *)malloc(BUFF_SIZE_128M);
		simd_t * tmp = (simd_t *)mmap(NULL, pool_sizes[POOL_WINDOW_KP], PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB, 0, 0);
		if(tmp == MAP_FAILED || !tmp){
			EE("give up: mmap WINDOW_KP failed.... %d allocated", i);
			abort();
            		//return; 
		}
		mem_pool_slow_window_kp.enqueue(tmp);
	}
	EE("preallocate %d chunks in mem_pool_window_kp pool, chunk size is %ld", i, BUFF_SIZE_WINDOW_KP);

	for(i = 0; i < SLOW_POOL_SIZE_WINDOW_RECORD; i++){
		//simd_t * tmp = (simd_t *)malloc(BUFF_SIZE_128M);
		simd_t * tmp = (simd_t *)mmap(NULL, pool_sizes[POOL_WINDOW_RECORD], PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB, 0, 0);
		if(tmp == MAP_FAILED || !tmp){
			EE("give up: mmap WINDOW_RECORD failed.... %d allocated", i);
			//abort();
            		return; 
		}
		mem_pool_slow_window_record.enqueue(tmp);
	}
	EE("preallocate %d chunks in mem_pool_window_record pool, chunk size is %ld", i, BUFF_SIZE_WINDOW_RECORD);
#endif
}

/******************/
simd_t * get_slow(unsigned int i) {
	simd_t * tmp = NULL;
	xzl_bug_on(i >= NUM_POOLS);
	mem_pools[i].try_dequeue(tmp);
    xzl_assert(tmp != MAP_FAILED); /* shall never enter queue */
	if(tmp == NULL) {
		EE("mem_pool %s is empty...", pools[i].name);
		mempool_stat_print();
		abort();
	}
	return tmp;
}

void return_slow(unsigned int i, simd_t *bundle) {
	xzl_bug_on(i >= NUM_POOLS);
	mem_pools[i].enqueue(bundle);
}

#if 0
simd_t * get_slow_4k(){
	simd_t * tmp = NULL; 
	mem_pool_slow_4k.try_dequeue(tmp);
	if(tmp == NULL || tmp == MAP_FAILED){
		EE("mem_pool_slow_4k is empty...");
		abort();
	}
	return tmp;
}

void return_slow_4k(simd_t *bundle){
	mem_pool_slow_4k.enqueue(bundle);
}

/******************/
simd_t * get_slow_bundle_kp_part(){
	simd_t * tmp = NULL; 
	mem_pool_slow_bundle_kp_part.try_dequeue(tmp);
	if(tmp == NULL || tmp == MAP_FAILED){
		EE("mem_pool_slow_bundle_kp_part is empty...");
		abort();
	}
	return tmp;
}

void return_slow_bundle_kp_part(simd_t *bundle){
	mem_pool_slow_bundle_kp_part.enqueue(bundle);
}

/******************/
simd_t * get_slow_bundle_kp(){
	simd_t * tmp = NULL; 
	mem_pool_slow_bundle_kp.try_dequeue(tmp);
	if(tmp == NULL || tmp == MAP_FAILED){
		EE("mem_pool_slow_bundle_kp is empty...");
		abort();
	}
	return tmp;
}

void return_slow_bundle_kp(simd_t *bundle){
	mem_pool_slow_bundle_kp.enqueue(bundle);
}

/******************/
simd_t * get_slow_bundle_record(){
	simd_t * tmp = NULL; 
	mem_pool_slow_bundle_record.try_dequeue(tmp);
	if(tmp == NULL || tmp == MAP_FAILED){
		EE("mem_pool_slow_bundle_record is empty...");
		abort();
	}
	return tmp;
}

void return_slow_bundle_record(simd_t *bundle){
	mem_pool_slow_bundle_record.enqueue(bundle);
}

/******************/
simd_t * get_slow_window_kp(){
	simd_t * tmp = NULL; 
	mem_pool_slow_window_kp.try_dequeue(tmp);
	if(tmp == NULL || tmp == MAP_FAILED){
		EE("mem_pool_slow_window_kp is empty...");
		abort();
	}
	return tmp;
}

void return_slow_window_kp(simd_t *bundle){
	mem_pool_slow_window_kp.enqueue(bundle);
}

/******************/
simd_t * get_slow_window_record(){
	simd_t * tmp = NULL; 
	mem_pool_slow_window_record.try_dequeue(tmp);
	if(tmp == NULL || tmp == MAP_FAILED){
		EE("mem_pool_slow_window_record is empty...");
		abort();
	}
	return tmp;
}

void return_slow_window_record(simd_t *bundle){
	mem_pool_slow_window_record.enqueue(bundle);
}
#endif

/******************/

#if 0
#define  SLOW_POOL_SIZE_1M 	1000
#define  SLOW_POOL_SIZE_128M 	50

//static std::list<simd_t *> bundle_pool_slow_1M;
//static std::list<simd_t *> bundle_pool_slow_128M;
static moodycamel::ConcurrentQueue<simd_t *> bundle_pool_slow_1M;
static moodycamel::ConcurrentQueue<simd_t *> bundle_pool_slow_128M;

static int size_1M = 0;
static int size_128M = 0;

//moved to batch.h
//#define BUFF_SIZE_1M (1000000 * 3 * sizeof(simd_t))
//#define BUFF_SIZE_128M (128000000 * 3 * sizeof(simd_t))

void init_bundle_pool(){
	uint32_t i;
	for(i = 0; i < SLOW_POOL_SIZE_1M; i++){
//		simd_t * tmp = (simd_t *)malloc(BUFF_SIZE_1M);
		simd_t * tmp = (simd_t *) mmap(NULL, BUFF_SIZE_1M, PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB, 0, 0);
		if(tmp == MAP_FAILED || !tmp){
			EE("warning: malloc 1M failed.... %d allocated", i);
			abort();
		}
		//printf("sizeof(simd_t) is %d\n", sizeof(simd_t));
		//for(uint64_t k = 0; k < BUFF_SIZE_1M/sizeof(simd_t); k=k+5000){
		//	tmp[k] = 0;
		//}
		//bundle_pool_slow_1M.push_back(tmp);
		bundle_pool_slow_1M.enqueue(tmp);
		size_1M ++;
	}
	EE("preallocate %d chunks in 1M pool", i);

	for(i = 0; i < SLOW_POOL_SIZE_128M; i++){
//		simd_t * tmp = (simd_t *)malloc(BUFF_SIZE_128M);
		simd_t * tmp = (simd_t *)mmap(NULL, BUFF_SIZE_128M, PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB, 0, 0);
		if(tmp == MAP_FAILED || !tmp){
			EE("give up: malloc 128M failed.... %d allocated", i);
			//abort();
            return; 
		}
		//for(uint64_t k = 0; k < BUFF_SIZE_128M/sizeof(simd_t); k=k+5000){
		//	tmp[k] = 0;
		//}
		//bundle_pool_slow_128M.push_back(tmp);
		bundle_pool_slow_128M.enqueue(tmp);
		size_128M ++;
	}
	EE("preallocate %d chunks in 128M pool", i);
}


simd_t * get_bundle_slow_1M(){
	simd_t * tmp = NULL; 
	bundle_pool_slow_1M.try_dequeue(tmp);
	if(tmp == NULL || tmp == MAP_FAILED){
		EE("batch_pool_1M is empty...");
		abort();
	}
	size_1M --;
	//printf("get one from 1M queue: size is %d\n", size_1M);
	return tmp;
}

void return_bundle_slow_1M(simd_t *bundle){
	//EE("return one to 1M pool: size is %d", size_1M);
	//std::unique_lock<std::mutex> lock(mutex_pool_slow_1M);
	//bundle_pool_slow_1M.push_front(bundle);
	bundle_pool_slow_1M.enqueue(bundle);
	//printf("return to 1M queue: size is %d\n", size_1M);
	size_1M ++;
}

simd_t * get_bundle_slow_128M(){
	//EE("get one from 128M pool: size is %d", size_128M);
	simd_t * tmp = NULL; 
	bundle_pool_slow_128M.try_dequeue(tmp);
	if(tmp == NULL || tmp == MAP_FAILED){
		EE("batch_pool_128M is empty...");
		abort();
	}
	size_128M --;
	return tmp;
}

void return_bundle_slow_128M(simd_t *bundle){
	//EE("return one to 128M pool: size is %d", size_128M);
	bundle_pool_slow_128M.enqueue(bundle);
	size_128M ++;
}
#endif

#if 0
static std::list<simd_t *> bundle_pool_slow_1M;
static std::list<simd_t *> bundle_pool_slow_128M;

static std::mutex mutex_pool_slow_1M;
static std::mutex mutex_pool_slow_128M;

static int size_1M = 0;
static int size_128M = 0;

//moved to batch.h
//#define BUFF_SIZE_1M (1000000 * 3 * sizeof(simd_t))
//#define BUFF_SIZE_128M (128000000 * 3 * sizeof(simd_t))

void init_bundle_pool(){
	uint32_t i;
	for(i = 0; i < SLOW_POOL_SIZE_1M; i++){
		simd_t * tmp = (simd_t *)malloc(BUFF_SIZE_1M);
		if(tmp == NULL){
			EE("malloc 1M failed....");
			abort();
		}
		bundle_pool_slow_1M.push_back(tmp);
		size_1M ++;
	}
	EE("preallocate %d chunks in 1M pool", i);

	for(i = 0; i < SLOW_POOL_SIZE_128M; i++){
		simd_t * tmp = (simd_t *)malloc(BUFF_SIZE_128M);
		if(tmp == NULL){
			EE("malloc 128M failed....");
			abort();
		}
		bundle_pool_slow_128M.push_back(tmp);
		size_128M ++;
	}
	EE("preallocate %d chunks in 128M pool", i);
}


simd_t * get_bundle_slow_1M(){
	//EE("get one from 1M pool: size is %d", size_1M);
	std::unique_lock<std::mutex> lock(mutex_pool_slow_1M);
	if(bundle_pool_slow_1M.empty()){
		EE("batch_pool_1M is empty...");
		abort();
	}
	simd_t * tmp = bundle_pool_slow_1M.front();
	bundle_pool_slow_1M.pop_front();
	size_1M --;
	return tmp;
}

void return_bundle_slow_1M(simd_t *bundle){
	//EE("return one to 1M pool: size is %d", size_1M);
	std::unique_lock<std::mutex> lock(mutex_pool_slow_1M);
	bundle_pool_slow_1M.push_front(bundle);
	size_1M ++;
}

simd_t * get_bundle_slow_128M(){
	//EE("get one from 128M pool: size is %d", size_128M);
	std::unique_lock<std::mutex> lock(mutex_pool_slow_128M);
	if(bundle_pool_slow_128M.empty()){
		EE("batch_pool_128M is empty...");
		abort();
	}
	simd_t * tmp = bundle_pool_slow_128M.front();
	bundle_pool_slow_128M.pop_front();
	size_128M --;
	return tmp;
}

void return_bundle_slow_128M(simd_t *bundle){
	//EE("return one to 128M pool: size is %d", size_128M);
	std::unique_lock<std::mutex> lock(mutex_pool_slow_128M);
	bundle_pool_slow_128M.push_front(bundle);
	size_128M ++;
}
#endif

