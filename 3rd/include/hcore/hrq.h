#ifndef _HRQ_H_
#define _HRQ_H_
#include "hdef.h"

// RQ
// TODO cache alive!!
#define HC_RET_BASE_RB HC_RET_BASE_DS
#define HRB_MIN(A, B) ((A) < (B) ? (A) : (B))

typedef struct hrb_ {
  size_t size;  // 2^n
  size_t in;
  size_t out;
  uint8_t data[];
} hrb_t;
size_t hrb_alloc_size(size_t s);
HFC_EXPORT hrb_t* hrb_alloc(size_t s);
HFC_EXPORT int hrb_free(hrb_t* rq);
HFC_EXPORT size_t hrb_push(hrb_t* rq, void* data, size_t size);
HFC_EXPORT size_t hrb_pop(hrb_t* rq, void* data, size_t size);
HFC_EXPORT size_t hrb_peek(hrb_t* rq, void* data, size_t size);



#endif