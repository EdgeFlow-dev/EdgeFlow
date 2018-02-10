/*
 * hym: 12/20/2017
 *   1: is expane() latency sensitive? check cache miss
 *   2: use hyper threads to do expand()? better performance??
 **/

//for CPU_SET
//https://stackoverflow.com/questions/24034631/undefined-reference-to-cpu-zero
#define _GNU_SOURCE

#include <err.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <pthread.h>
#include <mm/batch.h>
#include "log.h"// xzl
#include <xplane/index.h>
#include "xplane-internal.h"
#include "ubench.h"
//#include "climits.h"
#include "limits.h"
#include <time.h>
#include "measure.h"
#include <sched.h>

//#define N_BATCH 	(10)
#define N_BATCH 	(1)
#define reclen 		(3)
//#define rec_per_batch 	(1000 * 1000)
#define rec_per_batch 	(10)
#define THREADS 4

//ref: https://stackoverflow.com/questions/6127503/shuffle-array-in-c
/* Arrange the N elements of ARRAY in random order.
   Only effective if N is much smaller than RAND_MAX;
   if this may not be the case, use a better random
   number generator. */
void shuffle(simd_t *array, uint32_t n)
{
    if (n > 1) 
    {
        uint32_t i;
        for (i = 0; i < n - 1; i++) 
        {
          uint32_t j = i + rand() / (RAND_MAX / (n - i) + 1);
          simd_t t = array[j];
          array[j] = array[i];
          array[i] = t;
        }
    }
}

void print_r_batch(batch_t * batch_r){
	printf("in print_r_batch(): batch_r->size is %ld\n", batch_r->size);
	for(uint32_t i = 0; i < batch_r->size/reclen; i++){
	//for(uint32_t i = 0; i < rec_per_batch; i++){
		for(uint32_t j = 0; j < reclen; j++){
			printf("%ld,", batch_r->start[i*reclen + j]);
		}
		printf("\n");
	}
	printf("\n");
}

//#define rec_per_batch (10)
void test_expand(){
	batch_t * r_batch[N_BATCH];		
	batch_t * p_batch;	
	
	p_batch = batch_new(0, N_BATCH * rec_per_batch * sizeof(simd_t));
	batch_update(p_batch, p_batch->start + N_BATCH * rec_per_batch);	
		
	uint32_t idx_p = 0;	
	k2_measure("start");	
	for(int i = 0; i < N_BATCH; i++){
		r_batch[i] = batch_new(0, rec_per_batch * reclen * sizeof(simd_t));
		batch_update(r_batch[i], r_batch[i]->start + rec_per_batch * reclen);	
		//printf("r_batch[i]->size is %ld, rec_per_batch * reclen is %d\n", r_batch[i]->size, rec_per_batch * reclen);

		for(uint32_t j = 0; j < rec_per_batch; j++){
			for(uint32_t k = 0; k < reclen; k++){
				r_batch[i]->start[j*reclen + k] = rand()% INT_MAX;
			}	
			/*
			r_batch[i]->start[j*reclen] = j;
			r_batch[i]->start[j*reclen + 1] = j;
			r_batch[i]->start[j*reclen + 2] = j;
			*/	
			p_batch->start[idx_p++] = (simd_t) &(r_batch[i]->start[j*reclen]);
		}
	}
	k2_measure("init");	
/*
	printf("before shffle: records\n");
	for(int i = 0; i < N_BATCH; i++){
		print_r_batch(r_batch[i]);
	}
	
	printf("before shuffle: pbatch");
	for(uint32_t i = 0; i < idx_p; i++){
		printf("%ld, ", p_batch->start[i]);
	}
	printf("\n");	
*/
	//random shuffle p_batch->start[]
	shuffle(p_batch->start, idx_p);	
	k2_measure("shuffle");	
	//printf("shuffle done\n");
/*
	printf("after shuffle: pbatch");
	for(uint32_t i = 0; i < idx_p; i++){
		printf("%ld, ", p_batch->start[i]);
	}
	printf("\n");	
*/
	//sleep(10);	
	
	x_addr exp_r;
	x_addr ptr = (x_addr) p_batch;	
	
	expand_p(&exp_r, ptr, reclen); 
	k2_measure("expand");	
/*	
	printf("after expand\n");
	for(int i = 0; i < N_BATCH; i++){
		print_r_batch((batch_t *)exp_r);
	}
*/	
	k2_measure_flush();	
	EE("single thread expand_p %ld records", p_batch->size);
}

struct arg_struct{
	x_addr *dest; //record batch
	x_addr src_p;
	idx_t rec_len;
	uint32_t offset;
	uint32_t len;
	int core;
};

void * expand(void *arg){
	struct arg_struct * args = (struct arg_struct *) arg;

	//https://stackoverflow.com/questions/10490756/how-to-use-sched-getaffinity2-and-sched-setaffinity2-please-give-code-samp	
	cpu_set_t my_set;        /* Define your cpu_set bit mask. */
	CPU_ZERO(&my_set);       /* Initialize it all to 0, i.e. no CPUs selected. */
	//CPU_SET(7, &my_set);     /* set the bit that represents core 7. */
	CPU_SET(args->core, &my_set);     /* set the bit that represents core 7. */
	sched_setaffinity(0, sizeof(cpu_set_t), &my_set); /* Set affinity of tihs process to */
	
	expand_p_one_part(args->dest, args->src_p, args->rec_len, args->offset, args->len);
	
	//printf("%d core done\n", args->core);
	return 0;
}


//int32_t expand_p_one_part(x_addr *dest, x_addr src_p, idx_t reclen, uint32_t offset, uint32_t len) {
void test_expand_hyperthreading(){
	batch_t * r_batch[N_BATCH];		
	batch_t * p_batch;	
	
	p_batch = batch_new(0, N_BATCH * rec_per_batch * sizeof(simd_t));
	batch_update(p_batch, p_batch->start + N_BATCH * rec_per_batch);	
		
	uint32_t idx_p = 0;	
	
	k2_measure("start");	
	for(int i = 0; i < N_BATCH; i++){
		r_batch[i] = batch_new(0, rec_per_batch * reclen * sizeof(simd_t));
		batch_update(r_batch[i], r_batch[i]->start + rec_per_batch);	
		
		for(uint32_t j = 0; j < rec_per_batch; j++){
			r_batch[i]->start[j*reclen] = rand()% INT_MAX;
			r_batch[i]->start[j*reclen + 1] = rand()% INT_MAX;
			r_batch[i]->start[j*reclen + 2] = rand()% INT_MAX;
			p_batch->start[idx_p++] = (simd_t) &(r_batch[i]->start[j*reclen]);
		}
	}
	k2_measure("init");	
	
	shuffle(p_batch->start, idx_p);	
	k2_measure("shuffle");	
	
	//printf("shuffle done\n");
	//sleep(10);

	batch_t * exp_batch = batch_new(0, p_batch->size * reclen * sizeof(simd_t));
	batch_update(exp_batch, exp_batch->start + p_batch->size * reclen);
	
	x_addr exp_r = (x_addr) exp_batch;
	x_addr ptr = (x_addr) p_batch;	

	//split pbatch into THREADS partitions
	uint32_t offset[THREADS];
	uint32_t len[THREADS];
	uint32_t each_len = p_batch->size / THREADS;
	uint32_t last_len = p_batch->size - (each_len * (THREADS - 1));
	
	offset[0] = 0;	
	len[0] = each_len;	
	for(int i = 1; i < THREADS - 1; i++){	
		offset[i] = offset[i-1] + len[i-1];
		len[i] = each_len;
	}
	
	if(THREADS > 1){
		offset[THREADS - 1] = offset[THREADS - 2] + len[THREADS - 2]; 
		len[THREADS - 1] = last_len;
	}
	
	struct arg_struct * args = (struct arg_struct *) malloc(THREADS * sizeof(struct arg_struct));	
	for(int i = 0; i < THREADS; i++){
		args[i].dest = &exp_r;
		args[i].src_p = ptr;
		args[i].rec_len = reclen;
		args[i].offset = offset[i];
		args[i].len = len[i];				
	}

	//bind to core 4, 5, 6, and 7, which belong to hardware core 1	
	for(int i = 0; i < THREADS; i++){
		//TODO change core later
		args[i].core = i + 4; 
	}	

	pthread_t tid[THREADS];	
	for(int i = 0; i < THREADS; i++){
		pthread_create(&tid[i], NULL, expand, &args[i]);			
	}

	for(int i = 0; i < THREADS; i++){
		pthread_join(tid[i], NULL);	
	}	
	k2_measure("expand");	
	
	//for(int i = 0; i < p_batch->size; i++){
	//	printf("<%ld,%ld,%ld>, ", exp_batch->start[i*reclen], exp_batch->start[i*reclen + 1], exp_batch->start[i*reclen + 2]);
	//}
	//printf("\n");
	
	k2_measure_flush();	
	//EE("N_BATCH * rec_per_batch is %ld", N_BATCH * rec_per_batch);
	EE(" 4 hyper threads expand_p %ld records", p_batch->size);
}

int main(){
	//init_mem_pool_slow();
	xplane_init(NULL);
	test_expand();	
	//test_expand_hyperthreading();
} 

