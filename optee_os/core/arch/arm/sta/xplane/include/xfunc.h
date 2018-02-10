#ifndef XFUNC_H
#define XFUNC_H

#include "type.h"

#define X_DEBUG				1

/************* TEE_Params for sharing info. *******************/
#define IDX_VALUES			0
#define IDX_OUTBUF			1
#define IDX_SRC				2
#define IDX_PARAMS			2
#define IDX_SEG_BASES		3


#define PARAMS_T_IDX(p)		(uint32_t)p[IDX_VALUES].value.b
#define PARAMS_N_OUTPUTS(p)	(uint32_t)p[IDX_VALUES].value.a
//#define PARAMS_DEST(p)		p[IDX_DEST].memref.buffer
#define PARAMS_SRC(p)		(batch_t *)*(x_addr *)p[IDX_SRC].memref.buffer

#define get_outbuf(p)				p[IDX_OUTBUF].memref.buffer
#define set_outbuf_size(p, s)		p[IDX_OUTBUF].memref.size = s

#define get_params(p)				p[IDX_PARAMS].memref.buffer
#define set_time_ms(p, ms)			p[IDX_VALUES].value.a = ms

#define get_seg_bases(p)			p[IDX_SEG_BASES].memref.buffer
#define set_seg_bases_size(p, s)	p[IDX_SEG_BASES].memref.size = s

#define x_return(p, s)				p[IDX_VALUES].value.a = s

/* return error type */
#define x_returnA(p, s)				p[IDX_VALUES].value.a = s

/* return exec time */
#define x_returnB(p, s)				p[IDX_VALUES].value.b = s

#define xpos_mov(arr, reclen, cnt)	arr += (reclen * cnt)

#define pos_checker(reclen, pos)	reclen <= pos
/*************************************************************/

/********** ERROR CODE **************/
#define X_SUCCESS			0
#define X_ERR_NOSBUF		1
#define X_ERR_SMALL_INPUT	2
#define X_ERR_NO_XADDR		3
#define X_ERR_BATCH_CLOSED	4
#define X_ERR_WRONG_VAL		5
#define X_ERR_SEG_BASE		6
#define X_ERR_NO_BATCH		7
#define X_ERR_NO_FUNC		8
#define X_ERR_POS			9
#define X_ERR_PARAMS		10

/**********************	 TEE_CMD info. ************************/
enum X_CMD {
	X_CMD_MAP,
	X_CMD_KILL,

	X_CMD_UA_RAND,
	X_CMD_UA_NSBUF,
	X_CMD_UA_CREATE,
	X_CMD_UA_RETIRE,
	X_CMD_UA_PSEUDO,
	X_CMD_UA_TO_NS,
	X_CMD_UA_DEBUG,
	X_CMD_SHOW_SBUFF,

	X_CMD_SEGMENT,
	X_CMD_SORT,
	X_CMD_MERGE,
	X_CMD_JOIN,
	X_CMD_FILTER,
	X_CMD_MEDIAN,
	X_CMD_SUMCOUNT,
	X_CMD_CONCATENATE,
	X_CMD_MISC
};                  
/*************************************************************/

typedef uint8_t func_t;

/* TODO change it to union */
typedef struct {
	x_addr srcA;
	x_addr srcB;

	uint32_t n_outputs;

	func_t func;

	idx_t keypos;
	idx_t vpos;
	idx_t countpos;

	idx_t reclen;

	simd_t lower;
	simd_t higher;

	float k;
	bool reverse;

} x_params;


#endif
