#include <mm/sbuff.h>
#include <mm/core_mmu.h>
#include <trace.h>
#include <assert.h>
#include <string.h>
#include <kernel/misc.h>
#include <kernel/panic.h>
#include <platform_config.h>
#include <kernel/spinlock.h>
#include <string.h>

extern struct core_mem_struct core_mem_arr[CFG_TEE_CORE_NB_CORE];
extern struct core_mem_shared core_mem_shared;
void sbuff_init(void)
{
    unsigned int i, j;
    uint64_t addr = SBUFF_BASE;
    struct core_mem_struct *mem;

    for (i = 0; i < CFG_TEE_CORE_NB_CORE; ++i) {
        mem = &core_mem_arr[i];
        memset(mem->sbuff_bmap, 0x00, NUM_SBUFF);
        mem->num_sbuff = NUM_SBUFF;
        for (j = 0; j < NUM_SBUFF; ++j) {
            mem->sbuff[j].base = (void *)addr;
            mem->sbuff[j].recycle = (void *)addr;
            mem->sbuff[j].valid = (struct batch*)addr;
            mem->sbuff[j].last = (struct batch*)addr;
            mem->sbuff[j].count = 0;
            mem->sbuff[j].last_pgt = NULL;
            mem->sbuff[j].l2_pgt = NULL;
            mem->sbuff[j].pgt_list = (struct pgt_cache)SLIST_HEAD_INITIALIZER(pgt_list); /* has to be done during initialization, otherwise use compound literal */
            addr += 1 << SBUFF_VA_SHIFT;
            /* DMSG("sz: sb[%d] %p", i, (void*)&mem->sbuff[j]); */
            /* DMSG("init sbuff %d: %lx -- %lx", i, */
            /* 		(unsigned long)mem->sbuff[j].base, */
            /* 		(unsigned long)mem->sbuff[j].base + (1 << SBUFF_VA_SHIFT)); */
            /* mem->sbuff[j].pgt_list = {NULL}; */
        }
    }
    core_mem_shared.num_sbuff = NUM_SBUFF_SHARED;
    for (j = 0; j < NUM_SBUFF_SHARED; ++j) {
        core_mem_shared.sbuff[j].base = (void *)addr;
        core_mem_shared.sbuff[j].recycle = (void *)addr;
        core_mem_shared.sbuff[j].valid = (struct batch*)addr;
        core_mem_shared.sbuff[j].last = (struct batch*)addr;
        core_mem_shared.sbuff[j].count = 0;
        core_mem_shared.sbuff[j].last_pgt = NULL;
        core_mem_shared.sbuff[j].l2_pgt = NULL;
        core_mem_shared.sbuff[j].pgt_list = (struct pgt_cache)SLIST_HEAD_INITIALIZER(pgt_list);
        addr += 1 << SBUFF_VA_SHIFT;
    }
}

/* caller must hold batch lock */
/* xzl: create a new batch in a dedicated sbuff.
 * this assumes no concurrent sbuff_new_batch() 
 * NB: caller must not hold spinlock 
 */
struct batch *sbuff_new_batch(void)
{
    struct sbuff *sbuff;
    struct core_mem_struct *mem;
    struct batch *batch;
    unsigned int n;
    vaddr_t start,end;

    mem = &core_mem_arr[get_core_pos()];
    for (n = 0; n < mem->num_sbuff; ++n) {
        sbuff = &mem->sbuff[n];
        /* Think about following condition carefully */
        if (!mem->sbuff_bmap[n] || (sbuff->count == 0)) {
            if(mem->sbuff_bmap[n]) { /* sbuff in use, but last batch is dead. reset sbuff */
                start = (vaddr_t)sbuff->recycle;
                end = (vaddr_t)sbuff->last->end;
                sbuff->recycle = sbuff->base;
                sbuff->valid = sbuff->base;
                sbuff->last = sbuff->base;
                sbuff->last_pgt = NULL;
                sbuff->count = 0;
                /* __asm__ __volatile__("dc civac, %0\n\t" : : "r" (sbuff) : "memory"); */

                /* batch_unlock_irq(irq_status); */
                if (start < end)
                    sz_reset_phys_mem(start, end);
                /* touch batch to trigger page fault */
            } else {
                /* cannot work with recycle mem */
                assert(sbuff->base == sbuff->valid);
                assert(sbuff->count == 0);
                mem->sbuff_bmap[n] = 1;
                /* batch_unlock_irq(irq_status); */
            }
            /* irq_status = batch_lock_irq(); */
            assert(sbuff->base == sbuff->valid);
            assert(sbuff->count == 0);
            sz_allocate_page((vaddr_t)sbuff->valid);

            batch = batch_init(sbuff->valid);
            sbuff->count += 1;
            /* if ((uint64_t)batch >= 0x6200000000) { */
            /*     EMSG("sbuff base %s %lx", __func__, (uint64_t)sbuff->base); */
            /*     EMSG("new %lx %d", (uint64_t)batch, sbuff->count); */
            /* } */

            return batch;
        }
    }

    for (n = 0; n < core_mem_shared.num_sbuff; ++n) {
        sbuff = &core_mem_shared.sbuff[n];
        if (!core_mem_shared.sbuff_bmap[n] || (sbuff->count == 0)) {
            if(core_mem_shared.sbuff_bmap[n]) { /* sbuff in use, but last batch is dead. reset sbuff */
                start = (vaddr_t)sbuff->recycle;
                end = (vaddr_t)sbuff->last->end;
                sbuff->recycle = sbuff->base;
                sbuff->valid = sbuff->base;
                sbuff->last = sbuff->base;
                sbuff->last_pgt = NULL;
                sbuff->count = 0;
                __asm__ __volatile__("dc civac, %0\n\t" : : "r" (sbuff) : "memory");

                sz_reset_phys_mem(start, end);
                /* touch batch to trigger page fault */
            } else {
                /* cannot work with recycle mem */
                assert(sbuff->base == sbuff->valid);
                assert(sbuff->count == 0);
                core_mem_shared.sbuff_bmap[n] = 1;
            }
            /* irq_status = batch_lock_irq(); */
            assert(sbuff->base == sbuff->valid);
            assert(sbuff->count == 0);
            sz_allocate_page((vaddr_t)sbuff->valid);

            batch = batch_init(sbuff->valid);
            sbuff->count += 1;
            return batch;
        }

    }
    EMSG("No uarrayGroup available!!");
    sz_show_resource();
    panic("No uarrayGroup available!!");
    return NULL;
}

inline void sbuff_insert_l2_pgt(struct sbuff *sbuff, struct pgt *pgt)
{
    sbuff->l2_pgt = pgt;
}

inline void sbuff_insert_pgt(struct sbuff *sbuff, struct pgt *pgt)
{
    if (SLIST_EMPTY(&sbuff->pgt_list)) {
        SLIST_FIRST(&sbuff->pgt_list) = pgt;
        sbuff->last_pgt = pgt;
    } else {
        SLIST_NEXT(sbuff->last_pgt, link) = pgt;
        sbuff->last_pgt = pgt;
    }
}
