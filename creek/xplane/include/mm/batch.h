#ifndef BATCH_H
#define BATCH_H

#include <inttypes.h>
//#include <stdatomic.h>
#include <stdbool.h>

#include "xplane_lib.h" /* for pool desc */

#include "xplane/trace.h"
//#include <kernel/panic.h>
#include "xplane/type.h"
#include "xplane-internal.h"

#define OBJ_TYPE simd_t

#define BATCH_STATE_OPEN 0
#define BATCH_STATE_CLOSE 1
#define BATCH_STATE_DEAD 2


/* move def of struct batch into type.h" */
#if 0
struct batch {
	simd_t *start;
    void *end;
    struct batch *next;
    uint32_t state;
	simd_t size;
//    uint32_t size;
};

typedef struct batch batch_t;
#endif

#ifdef __cplusplus
extern "C"{
#endif

void BATCH_KILL(struct batch *batch, uint32_t idx);

void batch_check_overflow(struct batch *batch, simd_t **cur_ptr, int n);
struct batch *batch_init(void *addr);
struct batch *batch_new(uint32_t idx, uint32_t buf_size);
struct batch *batch_new_after(struct batch *batch, uint32_t idx);
int32_t batch_close(struct batch *batch, void *end);
void batch_update(struct batch *batch, void *end);
uint32_t batch_lock_irq(void);
void batch_unlock_irq(uint32_t irq);
//struct sbuff *batch_get_sbuff(struct batch *batch);
//void batch_escape(uint32_t idx, int32_t count);
void batch_escape(uint32_t idx);

/*
void init_bundle_pool();
simd_t * get_bundle_slow_1M();
void return_bundle_slow_1M(simd_t *bundle);
simd_t * get_bundle_slow_128M();
void return_bundle_slow_128M(simd_t *bundle);
*/

/* xzl */
#define NUM_POOLS 6
enum POOL_ID {
	POOL_4K = 0,
	POOL_BUNDLE_KP_PART,
	POOL_BUNDLE_KP,
	POOL_BUNDLE_RECORD,
	POOL_WINDOW_KP,
	POOL_WINDOW_RECORD
};


void init_mem_pool_slow(struct pool_desc *p);

long mempool_dispatch(unsigned long buf_size);
struct pool_desc const get_pool_config(unsigned int id);

simd_t * get_slow(unsigned int id);

simd_t * get_slow_4k();
simd_t * get_slow_bundle_kp_part();
simd_t * get_slow_bundle_kp();
simd_t * get_slow_bundle_record();
simd_t * get_slow_window_kp();
simd_t * get_slow_window_record();

void return_slow(unsigned int id, simd_t *buf);

void return_slow_4k(simd_t *buf);
void return_slow_bundle_kp_part(simd_t *buf);
void return_slow_bundle_kp(simd_t *buf);
void return_slow_bundle_record(simd_t *buf);
void return_slow_window_kp(simd_t *buf);
void return_slow_window_record(simd_t *buf);

/* statistics */
void mempool_stat_get(unsigned long sz);
void mempool_stat_put(unsigned long sz);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
/* statistics */
#include <array>
std::array<int, NUM_POOLS> mempool_stat_snapshot();
#endif

/* xzl: default mempool config. can be overwritten by argument to ini_mem_pool_slow() */

//#define ITEMS_PER_RECORD	(3)
#define ITEMS_PER_RECORD		(3*4)
#define RECORDS_PER_BUNDLE	(100 * 1000) 	// 100k or 1M, should be consistent with engine
#define NUM_PARTITIONS			(4) 		// parallelism, should be consistent with engine
#define BUNDLES_PER_WINDOW	(4)		// # bundles between two watermarks

#define BUFF_SIZE_4K			(4096)
#define BUFF_SIZE_BUNDLE_KP_PART	((RECORDS_PER_BUNDLE / NUM_PARTITIONS) * sizeof(simd_t)) //kbatch/pbatch will be splited into NUM_PARTITIONS	
#define BUFF_SIZE_BUNDLE_KP		(RECORDS_PER_BUNDLE * sizeof(simd_t))
#define BUFF_SIZE_BUNDLE_RECORD		(RECORDS_PER_BUNDLE * ITEMS_PER_RECORD * sizeof(simd_t))		

#define BUFF_SIZE_WINDOW_KP		(BUNDLES_PER_WINDOW * RECORDS_PER_BUNDLE * sizeof(simd_t))
#define BUFF_SIZE_WINDOW_RECORD		(10 * BUNDLES_PER_WINDOW * RECORDS_PER_BUNDLE * ITEMS_PER_RECORD * sizeof(simd_t))

#endif // BATCH_H
