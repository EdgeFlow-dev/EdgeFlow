#ifndef PARAMS_H
#define PARAMS_H

#define RESET   "\033[0m" 
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"

#define N_TUPLES	64 * 1024

#define CACHE_LINE_SIZE 64
#define L1_CACHE_SIZE (32 * 1024)
#define L2_CACHE_SIZE (512 * 1024)


/* check sorted or not */
//#define IS_SORTED

/* number of elements per item */
//#define ONE_PER_ITEM
//#define TWO_PER_ITEM
#define THREE_PER_ITEM
//#define FOUR_PER_ITEM

#ifdef ONE_PER_ITEM
	#define ELES_PER_ITEM	1
#endif
#ifdef TWO_PER_ITEM
	#define ELES_PER_ITEM	2
#endif
#ifdef THREE_PER_ITEM
	#define ELES_PER_ITEM	3
#endif
#ifdef FOUR_PER_ITEM
	#define ELES_PER_ITEM	4
#endif


#define L1_BLK_SIZE			(L1_CACHE_SIZE / (2 * sizeof(int32_t)))		// hp : 4k
#define CHUNKS_PER_L2		(int32_t)(L2_CACHE_SIZE / L1_CACHE_SIZE)

//#define ITEMS_PER_L1		1360
#define ITEMS_PER_L1		1024
#define ITEMS_PER_L2		16384
//#define ITEMS_PER_L1		(L1_CACHE_SIZE/ 2 /(sizeof(int32_t) * ELES_PER_ITEM))
//#define ITEMS_PER_L2		ITEMS_PER_L1 * CHUNKS_PER_L2

#define N_ITEMS(size)		(size / ELES_PER_ITEM)
#define N_CHUNKS(size)		(size / ELES_PER_ITEM) / ITEMS_PER_L1
#define N_SEGS(size)		(size / ELES_PER_ITEM) / ITEMS_PER_L2

#define TUPLES_PER_CACHELINE(size) (CACHE_LINE_SIZE/size)

#define SIZE_TO_ITEM(size)	size / ELES_PER_ITEM

#define x3_size_to_item(size)	size / 3
#define x4_size_to_item(size)	size / 4

#endif
