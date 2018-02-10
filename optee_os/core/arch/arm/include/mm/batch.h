#ifndef BATCH_H
#define BATCH_H

#include <inttypes.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <trace.h>
#include <kernel/panic.h>

#define OBJ_TYPE int32_t

#define BATCH_STATE_OPEN 0
#define BATCH_STATE_CLOSE 1
#define BATCH_STATE_DEAD 2

struct batch {
    OBJ_TYPE *start;
    void *end;
    struct batch *next;
    uint32_t state;
    uint32_t size;
};

void BATCH_KILL(struct batch *batch, uint32_t idx);

typedef struct batch batch_t;

struct batch *batch_init(void *addr);
struct batch *batch_new(uint32_t idx);
struct batch *batch_new_after(struct batch *batch, uint32_t idx);
int32_t batch_close(struct batch *batch, void *end);
void batch_update(struct batch *batch, void *end);
uint32_t batch_lock_irq(void);
void batch_unlock_irq(uint32_t irq);
//struct sbuff *batch_get_sbuff(struct batch *batch);
//void batch_escape(uint32_t idx, int32_t count);
void batch_escape(uint32_t idx);
#endif
