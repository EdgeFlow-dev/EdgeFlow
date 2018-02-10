#ifndef SBUFF_H
#define SBUFF_H

#include <mm/batch.h>
#include <mm/pgt_cache.h>

#define HEADER_ALIGNMENT 16
#define SBUFF_FIRST_BATCH(sbuff) (sbuff->valid)

#define SBUFF_BASE 0x0200000000
#define SBUFF_VA_SHIFT 30     /* 1GB */
#define SBUFF_VA_SPACE_SIZE (1ul << SBUFF_VA_SHIFT)
#define NUM_SBUFF 48
#define NUM_SBUFF_SHARED 12 * 8

struct sbuff {
    void *base;
    void *recycle;
    struct batch *valid;
    struct batch *last;
    struct pgt_cache pgt_list;
    struct pgt *l2_pgt;
    struct pgt *last_pgt;
    uint32_t count;
};

void sbuff_init(void);
struct batch *sbuff_new_batch(void);
void sbuff_insert_l2_pgt(struct sbuff *sbuff, struct pgt *pgt);
void sbuff_insert_pgt(struct sbuff *sbuff, struct pgt *pgt);
#endif
