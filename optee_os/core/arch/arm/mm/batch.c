#include <mm/batch.h>
#include <mm/sbuff.h>
#include <mm/core_mmu.h>
#include <string.h>
#include <assert.h>
#include <platform_config.h>
#include <stdatomic.h>
#include <kernel/spinlock.h>
#include <kernel/misc.h>
#include <kernel/abort.h>
#include <kernel/tee_time.h>
#define X_ERR_BATCH_CLOSED	4
#define X_SUCCESS			0

#define HEADER_ALIGNMENT 16

#define NUM_OP 19
#define NUM_EVENT 3

unsigned batch_spin_lock; /* init to be 0? */
extern int32_t batch_count[NUM_OP][NUM_EVENT];
extern struct core_mem_struct core_mem_arr[CFG_TEE_CORE_NB_CORE];
extern struct core_mem_shared core_mem_shared;
extern uint64_t mm_time;

/* extern uint32_t batch_count; */

inline uint32_t batch_lock_irq(void)
{
    uint32_t irq = thread_mask_exceptions(THREAD_EXCP_ALL);
    cpu_spin_lock(&batch_spin_lock);
    return irq;
}

inline void batch_unlock_irq(uint32_t irq)
{
    cpu_spin_unlock(&batch_spin_lock);
    thread_mask_exceptions(irq);
}

/* caller must hold the spinlock */
struct batch *batch_init(void *addr) {
    struct batch *batch = (struct batch *)addr;
    OBJ_TYPE *data_addr = (OBJ_TYPE *)((uint64_t)addr + sizeof(struct batch));
    /* DMSG("start a new batch at %p, data %p\n", addr, (void *)data_addr); */
    /* sz_show_resource(); */
    /* batch_count++; */
    batch->start= data_addr;
    batch->end = data_addr;
    batch->size = 0;
    batch->next = NULL;
    batch->state = BATCH_STATE_OPEN;
    return batch;
}

inline struct batch *batch_new(uint32_t idx)
{
    struct batch *new_batch;
    uint32_t irq_status;
#ifdef MEASURE_TIME
    TEE_Time start, end;
#endif
#ifdef MEASURE_TIME
    tee_time_get_sys_time(&start);
#endif
    atomic_fetch_add(&batch_count[idx][0], 1);
    irq_status = mem_lock_irq();
    new_batch = sbuff_new_batch();
    mem_unlock_irq(irq_status);
#ifdef MEASURE_TIME
    tee_time_get_sys_time(&end);
    atomic_fetch_add(&mm_time, sz_time_diff(&start, &end));
#endif
    return new_batch;
}

/* caller must hold batch lock */
static inline struct sbuff *batch_get_sbuff(struct batch *batch) {
    uint32_t sbuff_idx = ((uint64_t)batch - SBUFF_BASE) >> SBUFF_VA_SHIFT;
    if (sbuff_idx < NUM_SBUFF * 8)
        return &core_mem_arr[sbuff_idx / NUM_SBUFF].sbuff[sbuff_idx % NUM_SBUFF];
    else
        return &core_mem_shared.sbuff[sbuff_idx - NUM_SBUFF * 8];
}

/* Make sure no two batch_new_after on same batch */
struct batch *batch_new_after(struct batch *batch, uint32_t idx)
{
    struct sbuff *sbuff;
    struct batch *new_batch;
    void *nxt_batch_ptr;
    uint32_t irq_status;
#ifdef MEASURE_TIME
    TEE_Time start, end;

    tee_time_get_sys_time(&start);
#endif
    irq_status = mem_lock_irq();
    assert(batch->state == BATCH_STATE_CLOSE);
    atomic_fetch_add(&batch_count[idx][0], 1);
    if (batch->next != NULL) {
    	// xzl: another batch already placed after @batch. we need a new sbuff
        return sbuff_new_batch();
    }

    nxt_batch_ptr = (void *)ROUNDUP((uint64_t)batch->end,
                                    HEADER_ALIGNMENT);
    /* cpu_spin_unlock(&batch_spin_lock); */
    sz_allocate_page((vaddr_t)nxt_batch_ptr + sizeof(struct batch));

    new_batch = batch_init(nxt_batch_ptr);
    sbuff = batch_get_sbuff(batch);
                
    assert(sbuff->last == batch);
    sbuff->last->next = new_batch;
    sbuff->last = new_batch;
    sbuff->count++;
    mem_unlock_irq(irq_status);
#ifdef MEASURE_TIME
    tee_time_get_sys_time(&end);
    atomic_fetch_add(&mm_time, sz_time_diff(&start, &end));
#endif
    return new_batch;
}

void batch_escape(uint32_t idx)
{
    atomic_fetch_add(&batch_count[idx][2], 1);
}

int32_t batch_close(struct batch *batch, void *end)
{
    uint32_t irq_status;
    uint32_t st;
#ifdef MEASURE_TIME
    TEE_Time start_t, end_t;

    tee_time_get_sys_time(&start_t);
#endif
    irq_status = mem_lock_irq();
    st = batch->state;
    if (st != BATCH_STATE_OPEN) {
        EMSG("bug: %s: batch %lx should be OPEN. but it is %d", __func__, (uint64_t)batch, st);
        mem_unlock_irq(irq_status);
        return -X_ERR_BATCH_CLOSED;
    }

    batch->state = BATCH_STATE_CLOSE;
    batch->end = end;
    batch->size = ((uint64_t)end - (uint64_t)batch->start) / sizeof(OBJ_TYPE);
    //	DMSG("batch close : addr %p size : %d", (void *) batch, batch->size);

    mem_unlock_irq(irq_status);
#ifdef MEASURE_TIME
    tee_time_get_sys_time(&end_t);
    atomic_fetch_add(&mm_time, sz_time_diff(&start_t, &end_t));
#endif
    return X_SUCCESS;
}

void batch_update(struct batch *batch, void *end)
{
    uint32_t irq_status;
#ifdef MEASURE_TIME
    TEE_Time start_t, end_t;
    /* struct sbuff *sbuff; */

    tee_time_get_sys_time(&start_t);
#endif
    irq_status = mem_lock_irq();

    if (batch->state != BATCH_STATE_OPEN) {
        XMSG(" XXXXXXXXXXXXXX batch state is %u XXXXXXXXXXXX \n", batch->state);
    }

    assert(batch->state == BATCH_STATE_OPEN);
    batch->end = end;
    batch->size = ((uint64_t)end - (uint64_t)batch->start) / sizeof(OBJ_TYPE);

    mem_unlock_irq(irq_status);
#ifdef MEASURE_TIME
    tee_time_get_sys_time(&end_t);
    atomic_fetch_add(&mm_time, sz_time_diff(&start_t, &end_t));
#endif
}

inline uint32_t *batch_get_end(struct batch *batch) {
    assert(batch->state == BATCH_STATE_OPEN && batch->next == NULL);
    return batch->end;
}

void BATCH_KILL(struct batch *batch, uint32_t idx) {
    struct sbuff *sbuff;
    uint32_t irq_status;
    uint32_t sbuff_idx;
    vaddr_t start,end;
#ifdef MEASURE_TIME
    TEE_Time start_t, end_t;

    tee_time_get_sys_time(&start_t);
#endif
    irq_status = mem_lock_irq();
    if (batch->state != BATCH_STATE_OPEN && batch->state != BATCH_STATE_CLOSE) {
        EMSG("core %lu: Batch cannot be killed with state %d. addr %lx",
             (get_core_pos()),
             batch->state,
             (unsigned long)batch);
        panic("Batch cannot be killed with state");
        return;
    }
    batch->state = BATCH_STATE_DEAD;
    /* sbuff = batch_get_sbuff(batch); */
    sbuff_idx = ((uint64_t)batch - SBUFF_BASE) >> SBUFF_VA_SHIFT;
    if (sbuff_idx < NUM_SBUFF * 8)
        sbuff = &core_mem_arr[sbuff_idx / NUM_SBUFF].sbuff[sbuff_idx % NUM_SBUFF];
    else
        sbuff = &core_mem_shared.sbuff[sbuff_idx - NUM_SBUFF * 8];

    sbuff->count--;

    if (sbuff->count == 0) {
        if (sbuff_idx < NUM_SBUFF * 8)
            core_mem_arr[sbuff_idx / NUM_SBUFF].sbuff_bmap[sbuff_idx % NUM_SBUFF] = 0;
        else
            core_mem_shared.sbuff_bmap[sbuff_idx - NUM_SBUFF * 8] = 0;
        start = (vaddr_t)sbuff->recycle;
        end = (vaddr_t)sbuff->last->end;
        sbuff->recycle = sbuff->base;
        sbuff->valid = sbuff->base;
        sbuff->last = sbuff->base;
        sbuff->last_pgt = NULL;
        sz_reset_phys_mem(start, end);
    }
    atomic_fetch_add(&batch_count[idx][1], 1);

    mem_unlock_irq(irq_status);
#ifdef MEASURE_TIME
    tee_time_get_sys_time(&end_t);
    atomic_fetch_add(&mm_time, sz_time_diff(&start_t, &end_t));
#endif
    return;
}
