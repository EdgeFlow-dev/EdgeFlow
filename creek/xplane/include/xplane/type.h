#ifndef XPLANE_TYPE_H
#define XPLANE_TYPE_H

/* arch specific types that will be included by the lib user.
 *
 * Do NOT put anything else here
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
//#include <mm/batch.h>
//#include "params.h"

// #include <time.h>
//#include "common-types.h"
//#include "log.h"

/************************ typedefs **********************/
#ifdef __aarch64__
typedef int32_t simd_t ; /* the element type that simd operates on */
#define SIMDT_MAX INT_MAX
#elif defined(__x86_64)
typedef int64_t simd_t ; /* the element type that simd operates on */
#define SIMDT_MAX INT64_MAX
#else
#error "arch=?"
#endif

typedef uint64_t xscalar_t; /* encrypted 32-bit value */
typedef uint8_t idx_t; /* column index, # of cols, etc. */ 

typedef int32_t tuple_t;
typedef int32_t op_type;
typedef uint64_t x_addr;

#define x_addr_null 0

#endif // XPLANE_TYPE_H
