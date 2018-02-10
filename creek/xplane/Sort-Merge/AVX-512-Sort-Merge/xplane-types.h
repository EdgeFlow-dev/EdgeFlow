#ifndef AVX512_TYPES_H
#define AVX512_TYPES_H 

#define OBJ_TYPE int64_t
typedef uint64_t x_addr; //stores the address of batch struct
typedef uint8_t idx_t; 

/* Descriptor of an uarray */
struct batch{	
	OBJ_TYPE *start;
	void *end;
	batch *next;
	uint64_t state;
	uint64_t size;
};

typedef struct batch batch_t;

#endif /* AVX512-TYPES_H */
