#ifndef _HBUF_H_
#define _HBUF_H_
#include "hdef.h"


// BUF
#define HC_RET_BUF_BASE HC_RET_BASE_DS
typedef struct hbuf_ {
  size_t len;
  size_t size;
  uint8_t* data;
} hbuf_t;

HFC_EXPORT int hbuf_init(hbuf_t* b, size_t s);
HFC_EXPORT int hbuf_fini(hbuf_t* b);
HFC_EXPORT int hbuf_push(hbuf_t* b, const void* d, size_t dSize);
HFC_EXPORT int hbuf_clear(hbuf_t* b);
HFC_EXPORT int hbuf_grow(hbuf_t* b, size_t wish_size);
// HFC_EXPORT int hbuf_print(hbuf_t* b, size_t ofs, size_t size);




#endif