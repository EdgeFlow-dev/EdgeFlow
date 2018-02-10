#ifndef TYPE_H
#define TYPE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <mm/batch.h>
#include "params.h"


/************************ typedefs **********************/
typedef uint32_t simd_t ; /* the element type that simd operates on */
typedef uint64_t xscalar_t; /* encrypted 32-bit value */
typedef uint8_t idx_t; /* column index, # of cols, etc. */ 

typedef int32_t tuple_t;
typedef int32_t op_type;
typedef uint64_t x_addr;

typedef struct { int32_t val[1]; } x1_t;
typedef struct { int32_t val[2]; } x2_t;
typedef struct { int32_t val[3]; } x3_t;
typedef struct { int32_t val[4]; } x4_t;

typedef struct { x1_t val[4]; } x1x4_t;
typedef struct { x2_t val[4]; } x2x4_t;
typedef struct { x3_t val[4]; } x3x4_t;
typedef struct { x4_t val[4]; } x4x4_t;


#ifdef ONE_PER_ITEM
typedef struct _item_t { int32_t ts } item_t;
#endif
#ifdef TWO_PER_ITEM
typedef struct _item_t { int32_t ts, value; } item_t;
#endif
#ifdef THREE_PER_ITEM
typedef struct _item_t { int32_t ts, key, value; } item_t;
#endif
#ifdef FOUR_PER_ITEM
typedef struct _item_t { int32_t ts, key1, key2, value; } item_t;
#endif

#endif
