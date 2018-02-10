#include "common-types.h"
#include "xplane/utils.h"

#define N_THREADS	8

#define xstr(s) str(s)
#define str(s) #s
const char * xplane_lib_arch = xstr(XPLANE_LIB_ARCH);

/*
@ gid: unbouded array group id
@ size: size of allocated memory
*/

uint32_t bitcnt(uint32_t n){
	int c, v = n;
	for(c = 0; v; c++)
	{
		v &= v - 1;
	}
	return c;
}

int32_t x_pow(int32_t p, int n){
	int32_t res = 1;
	while (n--) res *= p;
	return res;
}

int32_t is_power_of_two(uint32_t x)
{
	return ((x > 0) && !(x & (x - 1)));
}

uint32_t my_log2(uint32_t x){
	uint32_t ans = 0;
	while (x >>= 1) ans++;
	return ans;
}

uint32_t ceil_log2(uint32_t x){
	uint32_t ans = 0;

	ans = my_log2(x);
	if(is_power_of_two(x)) return ans;
	else return ans + 1;
}

/*
void arr_dump(int32_t *arr, int32_t len){
	int32_t i;
	IMSG("arr_dump] first addr : %p, last addr: %p", (void *)arr, (void *) (arr + len));
	for(i = 0; i < len - 8; i += 8){
		IMSG("[i = %5d] %5d %5d %5d %5d %5d %5d %5d %5d", i, arr[i], arr[i+1], arr[i+2], arr[i+3], \
			arr[i+4], arr[i+5], arr[i+6], arr[i+7]);
	}
}
*/

int32_t x_is_sorted(int32_t *arr, int32_t len, uint32_t cmp, idx_t reclen){
	int32_t i;
	for(i = 0; i < len - 1; i++){
//		assert(arr[i] <= arr[i+1]);
		if(arr[i * reclen + cmp] > arr[(i + 1) * reclen + cmp]){
			XMSG("idx : %d | [%d:%d] [%d:%d]", i, i, arr[i * reclen + cmp], i+1, arr[(i + 1) * reclen + cmp]);
			XMSG("addr : %p", (void *)&arr[i * reclen + cmp]);
			return 0;
		}
	}
	return 1;
}

int32_t is_sorted(int32_t *arr, int32_t len){
	int32_t i;
	for(i = 0; i < len - 1; i++){
//		assert(arr[i] <= arr[i+1]);
		if(arr[i] > arr[i+1]){
			XMSG("[%d|%d] [%d|%d] [%d|%d] [%d|%d] len %d\n", \
				i-2, arr[i-2], i-1, arr[i-1], i, arr[i], i+1, arr[i+1], len);
			return 0;
		}
	}
	return 1;
}

int32_t generate_input(tuple_t * tuples, int32_t ntuples)
{
	int32_t i;
	for(i = 0 ; i < ntuples; i++){
		tuples[i] = lfsr113_Bits() % 100000;

//		if((i + 1) % (1 * 1024 * 1024 / 4) == 0)
//			XMSG("[hj] key gen %d", (i +1) / (1 * 1024 * 1024 / 4));
	}
	return 1;
}

//extern double cal_exec_time(struct timeval start, struct timeval stop);
//extern double get_exec_time_ms(struct timespec start, struct timespec stop);

/*
void *random_unique_gen_thread(void *args)
{
	create_arg_t *arg	= (create_arg_t*)args;
	relation_t *rel = arg->rel;
//	unsigned int seed = time(NULL) + *(unsigned int*) pthread_self();
	uint64_t i;

	for(i = arg->start; i < arg->end; i++ ){
//		rel->tuples[i] = rand() % 1000;
		rel->tuples[i].key = rand() % 1000;
		rel->tuples[i].value = rand() % 1000;
	}
}
*/

